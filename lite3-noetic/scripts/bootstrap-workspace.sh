#!/usr/bin/env bash
set -euo pipefail

CONTAINER_NAME="${LITE3_CONTAINER_NAME:-lite3-noetic-dev}"

docker exec -it "${CONTAINER_NAME}" bash -lc '/workspaces/lite3-noetic/scripts/in-container-bootstrap.sh'
