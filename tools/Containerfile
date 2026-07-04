# Image for both apps (sensor_sim + monitor). They share one image; compose
# overrides the command per service.
FROM python:3.12-slim

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUTF8=1

WORKDIR /app

# Install the package — this pulls in the core (serial_link) + pyserial.
COPY pyproject.toml ./
COPY serial_link ./serial_link
COPY apps ./apps
RUN pip install --no-cache-dir .

# Overridden by compose; harmless default.
CMD ["python", "-m", "apps.monitor", "--help"]
