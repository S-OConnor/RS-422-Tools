#!/usr/bin/env bash
# Idempotent CI dependency installer for the default debian:trixie-slim image.
#
# It installs only what a job asks for, and no-ops when the tools are already
# present — so the SAME before_script works whether a job runs on plain
# debian:trixie-slim or on the prebuilt toolchain image (docker/Containerfile.ci,
# selected via the CI_IMAGE variable).
#
#   ci/setup.sh base lint
#   ci/setup.sh base cross-riscv64 qemu
#   ci/setup.sh base python qemu
#
# Profiles:
#   base           compilers, cmake, ninja, ccache, dpkg-dev, iproute2, curl …
#   lint           clang-tidy, clang-format, cppcheck, shellcheck
#   host-gtest     system GoogleTest (skips the FetchContent clone on host builds)
#   cross-arm64    aarch64-linux-gnu cross toolchain + target libc/libstdc++
#   cross-riscv64  riscv64-linux-gnu cross toolchain + target libc/libstdc++
#   qemu           qemu-user-static (run/emulate foreign-arch binaries)
#   python         python3 + pyserial (drives the reference simulators)
#   sbom           syft + grype (installed to /usr/local/bin)
set -euo pipefail

pkgs=()
want_syft=0

for profile in "$@"; do
  case "$profile" in
    base)          pkgs+=(build-essential cmake ninja-build ccache git ca-certificates
                         pkg-config file dpkg-dev fakeroot iproute2 curl xz-utils) ;;
    lint)          pkgs+=(clang-tidy clang-format cppcheck shellcheck) ;;
    host-gtest)    pkgs+=(libgtest-dev libgmock-dev) ;;
    cross-arm64)   pkgs+=(crossbuild-essential-arm64) ;;
    cross-riscv64) pkgs+=(crossbuild-essential-riscv64) ;;
    qemu)          pkgs+=(qemu-user-static) ;;
    python)        pkgs+=(python3 python3-serial) ;;
    sbom)          want_syft=1 ;;
    *) echo "ci/setup.sh: unknown profile '$profile'" >&2; exit 2 ;;
  esac
done

# Install only the apt packages that aren't already present.
missing=()
for pkg in "${pkgs[@]}"; do
  if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q 'install ok installed'; then
    missing+=("$pkg")
  fi
done

if [ "${#missing[@]}" -gt 0 ]; then
  echo "ci/setup.sh: apt-get install ${missing[*]}"
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq
  apt-get install -y --no-install-recommends "${missing[@]}"
  rm -rf /var/lib/apt/lists/*
elif [ "${#pkgs[@]}" -gt 0 ]; then
  echo "ci/setup.sh: all apt deps already present"
fi

# syft + grype come from upstream install scripts (not in Debian). Pin a version
# via SYFT_VERSION / GRYPE_VERSION for reproducibility; default is latest.
if [ "$want_syft" -eq 1 ] && ! command -v syft >/dev/null 2>&1; then
  echo "ci/setup.sh: installing syft + grype into /usr/local/bin"
  curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh \
    | sh -s -- -b /usr/local/bin ${SYFT_VERSION:+"v${SYFT_VERSION}"}
  curl -sSfL https://raw.githubusercontent.com/anchore/grype/main/install.sh \
    | sh -s -- -b /usr/local/bin ${GRYPE_VERSION:+"v${GRYPE_VERSION}"}
fi
