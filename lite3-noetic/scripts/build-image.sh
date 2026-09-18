#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_TAG="${LITE3_IMAGE_TAG:-lite3-noetic:local}"

docker build \
  --build-arg UID="$(id -u)" \
  --build-arg GID="$(id -g)" \
  --build-arg USERNAME="${USER:-ros}" \
  -t "${IMAGE_TAG}" \
  -f "${ROOT_DIR}/Dockerfile" \
  "${ROOT_DIR}"

docker build \
  -t "${LITE3_OPENAI_IMAGE_TAG:-lite3-openai-runtime:local}" \
  -f "${ROOT_DIR}/openai-runtime/Dockerfile" \
  "${ROOT_DIR}"
