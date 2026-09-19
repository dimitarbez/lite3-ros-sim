#!/usr/bin/env bash
set -euo pipefail

MOTION_SSH="${ROBOT_MOTION_SSH:-ysc@192.168.2.1}"
PERCEPTION_SSH="${ROBOT_PERCEPTION_SSH:-ysc@192.168.1.103}"
ROBOT_USER="${ROBOT_USER:-ysc}"
TUNNEL_PORT="${HARDWARE_TUNNEL_PORT:-8767}"
CONTAINER_NAME="${LITE3_CONTAINER_NAME:-lite3-noetic-dev}"
CHAT_BACKEND="${CHAT_BACKEND:-openai}"
CHAT_MODEL="${CHAT_MODEL:-gpt-5-mini}"
NEUTRAL_BREATHING="${HARDWARE_NEUTRAL_BREATHING:-true}"
MINIMUM_BATTERY="${HARDWARE_MINIMUM_BATTERY:-25}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SSH_OPTIONS=(-o ConnectTimeout=8 -o ExitOnForwardFailure=yes -o ServerAliveInterval=5 -o ServerAliveCountMax=3)
CONTROL_DIR="$(mktemp -d)"
CONTROL_SOCKET="${CONTROL_DIR}/perception-ssh"
BREATHING_PID=""
BRAIN_PID=""
BRAIN_PID_FILE="/tmp/emotion-bot-hardware-brain-$$.pid"

cleanup() {
  trap - EXIT INT TERM
  if [[ -n "${BREATHING_PID}" ]] && kill -0 "${BREATHING_PID}" 2>/dev/null; then
    kill -TERM "${BREATHING_PID}" 2>/dev/null || true
    for _attempt in $(seq 1 30); do
      kill -0 "${BREATHING_PID}" 2>/dev/null || break
      sleep 0.1
    done
    kill -KILL "${BREATHING_PID}" 2>/dev/null || true
  fi
  wait "${BREATHING_PID}" 2>/dev/null || true
  docker exec "${CONTAINER_NAME}" bash -lc \
    'pid_file="$1"; if [[ -r "${pid_file}" ]]; then read -r pid <"${pid_file}"; if [[ "${pid}" =~ ^[0-9]+$ ]]; then kill -INT "${pid}" 2>/dev/null || true; fi; rm -f -- "${pid_file}"; fi' \
    _ "${BRAIN_PID_FILE}" >/dev/null 2>&1 || true
  if [[ -n "${BRAIN_PID}" ]] && kill -0 "${BRAIN_PID}" 2>/dev/null; then
    kill -INT "${BRAIN_PID}" 2>/dev/null || true
    for _attempt in $(seq 1 20); do
      kill -0 "${BRAIN_PID}" 2>/dev/null || break
      sleep 0.1
    done
    kill -KILL "${BRAIN_PID}" 2>/dev/null || true
  fi
  wait "${BRAIN_PID}" 2>/dev/null || true
  ssh -S "${CONTROL_SOCKET}" -O exit "${PERCEPTION_SSH}" >/dev/null 2>&1 || true
  rm -rf -- "${CONTROL_DIR}"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

if ! docker ps --format '{{.Names}}' | grep -Fxq "${CONTAINER_NAME}"; then
  echo "Container ${CONTAINER_NAME} is not running. Run: make -C lite3-noetic start" >&2
  exit 2
fi

echo "Checking installed fail-closed robot services..."
ssh "${SSH_OPTIONS[@]}" "${MOTION_SSH}" \
  "systemctl is-active 'emotion-bot-stop-observer@${ROBOT_USER}.service'"
ssh "${SSH_OPTIONS[@]}" -J "${MOTION_SSH}" "${PERCEPTION_SSH}" \
  "set -e; systemctl is-active 'emotion-bot-hardware-core@${ROBOT_USER}.service'; systemctl is-active 'emotion-bot-hardware-telemetry@${ROBOT_USER}.service'; source /opt/ros/noetic/setup.bash; source ~/emotion_bot_lite3_hw_ws/devel/setup.bash; test \"\$(rosparam get /emotion_bot/hardware/safety/transmit_enabled)\" = false; test \"\$(rosparam get /emotion_bot/hardware/safety/dynamic_actions_enabled)\" = false; test -x ~/emotion_bot_lite3_hw_ws/devel/lib/emotion_bot_lite3_hw/direct_gate_stream.py"

echo "Opening the loopback-only hardware tunnel..."
ssh "${SSH_OPTIONS[@]}" -J "${MOTION_SSH}" -M -S "${CONTROL_SOCKET}" -fN \
  -L "127.0.0.1:${TUNNEL_PORT}:127.0.0.1:${TUNNEL_PORT}" "${PERCEPTION_SSH}"

for _attempt in $(seq 1 30); do
  if bash -c "exec 3<>/dev/tcp/127.0.0.1/${TUNNEL_PORT}" 2>/dev/null; then
    break
  fi
  sleep 0.1
done
if ! bash -c "exec 3<>/dev/tcp/127.0.0.1/${TUNNEL_PORT}" 2>/dev/null; then
  echo "Hardware receiver did not become reachable through the tunnel." >&2
  exit 1
fi

if [[ "${NEUTRAL_BREATHING}" == "true" ]]; then
  echo "Starting default neutral breathing (50% height bound, ${MINIMUM_BATTERY}% battery floor)."
  ROBOT_MOTION_SSH="${MOTION_SSH}" ROBOT_PERCEPTION_SSH="${PERCEPTION_SSH}" \
    HARDWARE_MINIMUM_BATTERY="${MINIMUM_BATTERY}" \
    python3 -u "${ROOT_DIR}/scripts/run-neutral-breathing-hardware.py" &
  BREATHING_PID=$!
elif [[ "${NEUTRAL_BREATHING}" != "false" ]]; then
  echo "HARDWARE_NEUTRAL_BREATHING must be true or false." >&2
  exit 2
fi

echo "Hardware link ready; launching the development-computer brain (${CHAT_BACKEND})."
docker exec -t "${CONTAINER_NAME}" bash -lc \
  "echo \$\$ > '${BRAIN_PID_FILE}' && source /opt/ros/noetic/setup.bash && source /workspaces/lite3-noetic/ws/Lite3_VMC/devel/setup.bash && exec roslaunch emotion_bot_ros hardware_brain.launch chat_backend:=${CHAT_BACKEND} chat_model:=${CHAT_MODEL} uplink_port:=${TUNNEL_PORT}" &
BRAIN_PID=$!

set +e
if [[ -n "${BREATHING_PID}" ]]; then
  wait -n "${BRAIN_PID}" "${BREATHING_PID}"
  status=$?
else
  wait "${BRAIN_PID}"
  status=$?
fi
set -e
exit "${status}"
