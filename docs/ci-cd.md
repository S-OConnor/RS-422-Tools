# CI/CD — multi-arch pipeline

The GitLab pipeline ([`.gitlab-ci.yml`](../.gitlab-ci.yml)) builds, tests, and packages
the C++ `serial_link` library and `rs422-*` apps for five microarchitecture
**variants** across three ISAs, and can be run **locally** with
[`gitlab-ci-local`](https://github.com/firecow/gitlab-ci-local).

## Stages

| Stage | What it does | Scope |
|-------|--------------|-------|
| **lint** | `clang-format` + `clang-tidy` (advisory), `cppcheck` + `shellcheck` (gating) | once, native |
| **build** | CMake + Ninja Release build per variant, `cmake --install` to a staging tree | per variant |
| **test** | `ctest` (GoogleTest); cross binaries run under **qemu-user** | per variant |
| **integration** | C++ apps ⇄ Python reference sims over TCP (streaming + register C2) | per variant |
| **package** | `cpack` → a `.tar.gz` and a `.deb` tagged with the target microarch | per variant |
| **sbom** | `syft` → CycloneDX + SPDX; `grype` vuln scan (advisory) | per variant + source |

`build → package` cover every variant, exactly as required; `test`, `integration`,
and `sbom` cover them too. Each `test`/`integration`/`package`/`sbom` job `needs`
only its own `build`, so the five variant chains run as independent DAGs.

## Variants

Every variant is one file: `cmake/toolchains/<variant>.cmake`. That file owns the
`-march`/`-mtune`, the cross compiler, the qemu-user emulator (used to run the test
suite for foreign arches on an x86 runner), and the package tag + dpkg architecture.

| VARIANT | ISA | `-march` / `-mcpu` | qemu `-cpu` | target |
|---------|-----|--------------------|-------------|--------|
| `x86_64-broadwell` | x86-64 | `-march=broadwell` | native | Intel Broadwell |
| `x86_64-znver2` | x86-64 | `-march=znver2` | native | AMD Ryzen (Zen 2) |
| `aarch64-cortex-a72` | arm64 | `-mcpu=cortex-a72` | `cortex-a72` | Cortex-A72 (RPi 4, SBCs) |
| `riscv64-sifive-x280` | riscv64 | `rv64gcv_zba_zbb` | `rv64,v,vlen=512` | SiFive Intelligence X280 |
| `riscv64-spacemit-k1` | riscv64 | `rv64gcv_zba_zbb_zbs` | `rv64,v,vlen=256` | SpacemiT K1 — Orange Pi RV2 |

The two x86 variants build natively (same ISA family as the runner, just different
`-march`); AArch64 and RISC-V cross-compile with the Debian `*-linux-gnu` toolchains.
`gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)` (see [tests/CMakeLists.txt](../tests/CMakeLists.txt))
defers running the test binary to `ctest` time so cross binaries execute through the
toolchain's `CMAKE_CROSSCOMPILING_EMULATOR` (qemu-user) instead of on the build host.

### Adding a variant

1. Add `cmake/toolchains/<variant>.cmake` (copy the closest existing one; set the
   flags, `CMAKE_CROSSCOMPILING_EMULATOR`, `SERIAL_LINK_TARGET_TAG`, and
   `SERIAL_LINK_DEB_ARCH`).
2. In `.gitlab-ci.yml` add a `.v-<variant>` template and the five
   `build/test/integration/package/sbom:<variant>` jobs (copy an existing block).

## The runner image

Jobs run on `debian:trixie-slim` and install just what they need via
[`ci/setup.sh`](../ci/setup.sh) (idempotent — a no-op when the tool is already
present). For faster pipelines, build the bundled toolchain image and point the
pipeline at it:

```bash
podman build -f docker/Containerfile.ci -t <registry>/serial-link/toolchain:latest .
podman push <registry>/serial-link/toolchain:latest
# then set the CI/CD variable:  CI_IMAGE = <registry>/serial-link/toolchain:latest
```

The `ci-image` job builds & pushes that image with Kaniko (on the default branch
when the Containerfile changes, on schedules, or manually). It is gated on
`REGISTRY_ENABLED` so it never runs locally.

## Running locally with gitlab-ci-local

[`gitlab-ci-local`](https://github.com/firecow/gitlab-ci-local) runs the real jobs
in Docker/Podman containers — no GitLab, no runner. [`.gitlab-ci-local-variables.yml`](../.gitlab-ci-local-variables.yml)
disables the registry push locally.

```bash
# install (npm) — or: brew install gitlab-ci-local
npm i -g gitlab-ci-local

# with Docker: just run it. With rootless Podman, point it at the podman socket:
systemctl --user start podman.socket
export DOCKER_HOST="unix://$XDG_RUNTIME_DIR/podman/podman.sock"

gitlab-ci-local --list                              # list all jobs
gitlab-ci-local --container-executable podman \
    build:aarch64-cortex-a72 test:aarch64-cortex-a72 # run a variant's build+test
gitlab-ci-local --container-executable podman        # run the whole pipeline
```

Notes:
- **Uncommitted files** must be `git add`-ed — gitlab-ci-local copies tracked files
  into the job containers.
- Run a job **and** its `needs` in one invocation (e.g. `build:X test:X`) so the
  build artifact is present when the downstream job starts.
- With Docker, drop `--container-executable podman`.

Every stage in this pipeline has been verified this way end-to-end: native x86,
plus cross-built AArch64 and RISC-V whose test suites pass under qemu-user.

## Artifacts

- **build** — the install tree + build dir (`install/<variant>/`, `build/<variant>/`), 1 day.
- **test** — JUnit report (GitLab test tab), 1 week.
- **package** — `dist/<variant>/*.tar.gz` + `*.deb`, 1 month.
- **sbom** — `sbom/<variant>/{cyclonedx,spdx}.json`; CycloneDX is surfaced as a
  GitLab `cyclonedx` report (Dependency List / security dashboard), 1 month.
