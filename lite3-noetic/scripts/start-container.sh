#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMOTION_BOT_DIR="${LITE3_EMOTION_BOT_DIR:-$(cd "${ROOT_DIR}/.." && pwd)/emotion-bot}"
IMAGE_TAG="${LITE3_IMAGE_TAG:-lite3-noetic:local}"
CONTAINER_NAME="${LITE3_CONTAINER_NAME:-lite3-noetic-dev}"

mkdir -p "${ROOT_DIR}/ws"

if [ ! -d "${EMOTION_BOT_DIR}/.git" ]; then
  echo "Missing required emotion-bot checkout at ${EMOTION_BOT_DIR}."
  echo "Clone and pin it before starting the container; see README.md."
  exit 2
fi

container_has_expected_runtime() {
  local mounted_source init_enabled
  mounted_source="$(docker inspect --format '{{range .Mounts}}{{if eq .Destination "/workspaces/emotion-bot"}}{{.Source}}{{end}}{{end}}' "${CONTAINER_NAME}")"
  init_enabled="$(docker inspect --format '{{.HostConfig.Init}}' "${CONTAINER_NAME}")"
  [ "${mounted_source}" = "${EMOTION_BOT_DIR}" ] && [ "${init_enabled}" = "true" ]
}

if docker ps --format '{{.Names}}' | grep -Fxq "${CONTAINER_NAME}"; then
  if ! container_has_expected_runtime; then
    echo "Container ${CONTAINER_NAME} has stale mounts/runtime settings. Run 'make restart' to recreate it with ${EMOTION_BOT_DIR} and Docker init."
    exit 2
  fi
  echo "Container ${CONTAINER_NAME} is already running."
  exit 0
fi

if docker ps -a --format '{{.Names}}' | grep -Fxq "${CONTAINER_NAME}"; then
  if ! container_has_expected_runtime; then
    echo "Container ${CONTAINER_NAME} has stale mounts/runtime settings. Run 'make restart' to recreate it with ${EMOTION_BOT_DIR} and Docker init."
    exit 2
  fi
  docker start "${CONTAINER_NAME}" >/dev/null
  echo "Started existing container ${CONTAINER_NAME}."
  exit 0
fi

docker run -d \
  --init \
  --name "${CONTAINER_NAME}" \
  --hostname "${CONTAINER_NAME}" \
  --network host \
  --ipc host \
  -e DISPLAY="${DISPLAY:-:0}" \
  -e ROS_MASTER_URI="${ROS_MASTER_URI:-http://127.0.0.1:11311}" \
  -e ROS_HOSTNAME="${ROS_HOSTNAME:-127.0.0.1}" \
  -e ROS_IP="${ROS_IP:-127.0.0.1}" \
  -e WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}" \
  -e XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir \
  -e PULSE_SERVER=unix:/mnt/wslg/PulseServer \
  -e QT_X11_NO_MITSHM=1 \
  -e LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}" \
  -v "${ROOT_DIR}:/workspaces/lite3-noetic" \
  -v "${EMOTION_BOT_DIR}:/workspaces/emotion-bot:ro" \
  -v /mnt/wslg:/mnt/wslg \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  "${IMAGE_TAG}" \
  sleep infinity >/dev/null

echo "Created container ${CONTAINER_NAME}."
