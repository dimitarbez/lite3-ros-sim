#!/usr/bin/env bash
set -euo pipefail

MOTION_SSH="${ROBOT_MOTION_SSH:-ysc@192.168.2.1}"
PERCEPTION_SSH="${ROBOT_PERCEPTION_SSH:-ysc@192.168.1.103}"
ROBOT_USER="${ROBOT_USER:-ysc}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SSH_OPTIONS=(-o ConnectTimeout=8 -o ServerAliveInterval=5 -o ServerAliveCountMax=3)
REMOTE_PID_FILE="/tmp/emotion-bot-retroid-diagnostic-${ROBOT_USER}.pid"
DIAGNOSTIC_PID=""

cleanup() {
  trap - EXIT INT TERM
  ssh "${SSH_OPTIONS[@]}" -J "${MOTION_SSH}" "${PERCEPTION_SSH}" \
    "if test -r '${REMOTE_PID_FILE}'; then read -r pid <'${REMOTE_PID_FILE}'; case \"\$pid\" in (*[!0-9]*|'') ;; (*) kill -INT \"\$pid\" 2>/dev/null || true ;; esac; rm -f -- '${REMOTE_PID_FILE}'; fi" \
    >/dev/null 2>&1 || true
  if [[ -n "${DIAGNOSTIC_PID}" ]] && kill -0 "${DIAGNOSTIC_PID}" 2>/dev/null; then
    kill -TERM "${DIAGNOSTIC_PID}" 2>/dev/null || true
  fi
  wait "${DIAGNOSTIC_PID}" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

ssh "${SSH_OPTIONS[@]}" -J "${MOTION_SSH}" "${PERCEPTION_SSH}" \
  'test ! -e /dev/shm/emotion_bot_lite3_official_sdk.owned && test ! -e /dev/shm/emotion_bot_lite3_direct_joint.owned'

echo "Starting the separate fail-closed legacy diagnostic graph."
ssh "${SSH_OPTIONS[@]}" -J "${MOTION_SSH}" "${PERCEPTION_SSH}" \
  "exec 9>/dev/shm/emotion_bot_lite3_direct_joint.lock; flock -n 9 || { echo 'another hardware sender owns the shared lease' >&2; exit 2; }; echo \$\$ >'${REMOTE_PID_FILE}'; source /opt/ros/noetic/setup.bash; source ~/lite_cog/transfer/devel/setup.bash; source ~/emotion_bot_lite3_hw_ws/devel/setup.bash; exec roslaunch emotion_bot_lite3_hw hardware_diagnostic.launch" &
DIAGNOSTIC_PID=$!
sleep 2

ROBOT_MOTION_SSH="${MOTION_SSH}" ROBOT_PERCEPTION_SSH="${PERCEPTION_SSH}" \
  python3 -u "${ROOT_DIR}/scripts/run-neutral-breathing-hardware.py"
