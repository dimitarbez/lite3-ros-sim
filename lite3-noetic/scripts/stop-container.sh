#!/usr/bin/env bash
set -euo pipefail

CONTAINER_NAME="${LITE3_CONTAINER_NAME:-lite3-noetic-dev}"

docker rm -f "${CONTAINER_NAME}"
