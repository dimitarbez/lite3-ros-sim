#!/usr/bin/env bash
set -euo pipefail

MOTION_SSH="${ROBOT_MOTION_SSH:-ysc@192.168.2.1}"
PERCEPTION_SSH="${ROBOT_PERCEPTION_SSH:-ysc@192.168.1.103}"
ROBOT_USER="${ROBOT_USER:-ysc}"
TUNNEL_PORT="${HARDWARE_TUNNEL_PORT:-8767}"
CONTAINER_NAME="${LITE3_CONTAINER_NAME:-lite3-noetic-dev}"
CHAT_BACKEND="${CHAT_BACKEND:-openai}"
CHAT_MODEL="${CHAT_MODEL:-gpt-5-mini}"
COMMISSIONED_EMOTIONS="${HARDWARE_COMMISSIONED_EMOTIONS:-neutral}"
PROFILE_SCALE="${HARDWARE_EXPRESSION_SCALE:-1.0}"
MINIMUM_BATTERY="${HARDWARE_MINIMUM_BATTERY:-25}"
JOY_PAW_TEST="${HARDWARE_JOY_PAW_TEST:-false}"
JOY_SUITE_TEST="${HARDWARE_JOY_SUITE_TEST:-false}"
PAW_LIFT_METERS="${HARDWARE_PAW_LIFT_METERS:-0.005}"
ANGER_SINGLE_STOMP_TEST="${HARDWARE_ANGER_SINGLE_STOMP_TEST:-false}"
ANGER_SUITE_TEST="${HARDWARE_ANGER_SUITE_TEST:-false}"
ANGER_SUITE_CYCLES="${HARDWARE_ANGER_SUITE_CYCLES:-1}"
ANGER_FIRST_PAW="${HARDWARE_ANGER_FIRST_PAW:-left}"
ANGER_LIFT_METERS="${HARDWARE_ANGER_LIFT_METERS:-0.035}"
SADNESS_SINGLE_HOVER_TEST="${HARDWARE_SADNESS_SINGLE_HOVER_TEST:-false}"
SADNESS_SUITE_TEST="${HARDWARE_SADNESS_SUITE_TEST:-false}"
SADNESS_BODY_VISUAL_TEST="${HARDWARE_SADNESS_BODY_VISUAL_TEST:-false}"
SADNESS_BODY_VISUAL_CYCLES="${HARDWARE_SADNESS_BODY_VISUAL_CYCLES:-1}"
SADNESS_FIRST_PAW="${HARDWARE_SADNESS_FIRST_PAW:-left}"
SADNESS_LIFT_METERS="${HARDWARE_SADNESS_LIFT_METERS:-0.020}"
FEAR_BODY_VISUAL_TEST="${HARDWARE_FEAR_BODY_VISUAL_TEST:-false}"
FEAR_SINGLE_HOVER_TEST="${HARDWARE_FEAR_SINGLE_HOVER_TEST:-false}"
FEAR_SUITE_TEST="${HARDWARE_FEAR_SUITE_TEST:-false}"
FEAR_FIRST_PAW="${HARDWARE_FEAR_FIRST_PAW:-left}"
FEAR_LIFT_METERS="${HARDWARE_FEAR_LIFT_METERS:-0.025}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SSH_OPTIONS=(-o ConnectTimeout=8 -o ExitOnForwardFailure=yes -o ServerAliveInterval=5 -o ServerAliveCountMax=3)
CONTROL_DIR="$(mktemp -d)"
CONTROL_SOCKET="${CONTROL_DIR}/perception-ssh"
EXPRESSION_PID=""
BRAIN_PID=""
BRAIN_PID_FILE="/tmp/emotion-bot-hardware-brain-$$.pid"
REMOTE_RUNNER_PID_FILE="/tmp/emotion-bot-expression-${ROBOT_USER}.pid"

case "${COMMISSIONED_EMOTIONS}" in
  ''|*[!a-z,]*) echo "HARDWARE_COMMISSIONED_EMOTIONS must be a comma-separated lowercase list." >&2; exit 2 ;;
esac
case "${PROFILE_SCALE}" in
  ''|*[!0-9.]*) echo "HARDWARE_EXPRESSION_SCALE must be numeric." >&2; exit 2 ;;
esac
case "${MINIMUM_BATTERY}" in
  ''|*[!0-9.]*) echo "HARDWARE_MINIMUM_BATTERY must be numeric." >&2; exit 2 ;;
esac
case "${JOY_PAW_TEST}" in
  true) JOY_PAW_TEST_ARGUMENT="--joy-paw-test" ;;
  false) JOY_PAW_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_JOY_PAW_TEST must be true or false." >&2; exit 2 ;;
esac
case "${JOY_SUITE_TEST}" in
  true) JOY_SUITE_TEST_ARGUMENT="--joy-suite-test" ;;
  false) JOY_SUITE_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_JOY_SUITE_TEST must be true or false." >&2; exit 2 ;;
esac
case "${PAW_LIFT_METERS}" in
  ''|*[!0-9.]*) echo "HARDWARE_PAW_LIFT_METERS must be numeric." >&2; exit 2 ;;
esac
case "${ANGER_SINGLE_STOMP_TEST}" in
  true) ANGER_SINGLE_STOMP_TEST_ARGUMENT="--anger-single-stomp-test" ;;
  false) ANGER_SINGLE_STOMP_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_ANGER_SINGLE_STOMP_TEST must be true or false." >&2; exit 2 ;;
esac
case "${ANGER_SUITE_TEST}" in
  true) ANGER_SUITE_TEST_ARGUMENT="--anger-suite-test" ;;
  false) ANGER_SUITE_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_ANGER_SUITE_TEST must be true or false." >&2; exit 2 ;;
esac
case "${ANGER_SUITE_CYCLES}" in
  ''|*[!0-9]*) echo "HARDWARE_ANGER_SUITE_CYCLES must be an integer." >&2; exit 2 ;;
esac
if ((ANGER_SUITE_CYCLES < 1 || ANGER_SUITE_CYCLES > 3)); then
  echo "HARDWARE_ANGER_SUITE_CYCLES must be in [1, 3]." >&2
  exit 2
fi
if [[ "${ANGER_SUITE_TEST}" != true && "${ANGER_SUITE_CYCLES}" != 1 ]]; then
  echo "HARDWARE_ANGER_SUITE_CYCLES above 1 requires HARDWARE_ANGER_SUITE_TEST=true." >&2
  exit 2
fi
case "${ANGER_FIRST_PAW}" in
  left|right) ;;
  *) echo "HARDWARE_ANGER_FIRST_PAW must be left or right." >&2; exit 2 ;;
esac
case "${ANGER_LIFT_METERS}" in
  ''|*[!0-9.]*) echo "HARDWARE_ANGER_LIFT_METERS must be numeric." >&2; exit 2 ;;
esac
case "${SADNESS_SINGLE_HOVER_TEST}" in
  true) SADNESS_SINGLE_HOVER_TEST_ARGUMENT="--sadness-single-hover-test" ;;
  false) SADNESS_SINGLE_HOVER_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_SADNESS_SINGLE_HOVER_TEST must be true or false." >&2; exit 2 ;;
esac
case "${SADNESS_BODY_VISUAL_TEST}" in
  true) SADNESS_BODY_VISUAL_TEST_ARGUMENT="--sadness-body-visual-test" ;;
  false) SADNESS_BODY_VISUAL_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_SADNESS_BODY_VISUAL_TEST must be true or false." >&2; exit 2 ;;
esac
case "${SADNESS_BODY_VISUAL_CYCLES}" in
  ''|*[!0-9]*) echo "HARDWARE_SADNESS_BODY_VISUAL_CYCLES must be an integer." >&2; exit 2 ;;
esac
if ((SADNESS_BODY_VISUAL_CYCLES < 1 || SADNESS_BODY_VISUAL_CYCLES > 2)); then
  echo "HARDWARE_SADNESS_BODY_VISUAL_CYCLES must be in [1, 2]." >&2
  exit 2
fi
if [[ "${SADNESS_BODY_VISUAL_TEST}" != true &&
      "${SADNESS_BODY_VISUAL_CYCLES}" != 1 ]]; then
  echo "HARDWARE_SADNESS_BODY_VISUAL_CYCLES above 1 requires HARDWARE_SADNESS_BODY_VISUAL_TEST=true." >&2
  exit 2
fi
case "${SADNESS_SUITE_TEST}" in
  true) SADNESS_SUITE_TEST_ARGUMENT="--sadness-suite-test" ;;
  false) SADNESS_SUITE_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_SADNESS_SUITE_TEST must be true or false." >&2; exit 2 ;;
esac
case "${SADNESS_FIRST_PAW}" in
  left|right) ;;
  *) echo "HARDWARE_SADNESS_FIRST_PAW must be left or right." >&2; exit 2 ;;
esac
case "${SADNESS_LIFT_METERS}" in
  ''|*[!0-9.]*) echo "HARDWARE_SADNESS_LIFT_METERS must be numeric." >&2; exit 2 ;;
esac
case "${FEAR_BODY_VISUAL_TEST}" in
  true) FEAR_BODY_VISUAL_TEST_ARGUMENT="--fear-body-visual-test" ;;
  false) FEAR_BODY_VISUAL_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_FEAR_BODY_VISUAL_TEST must be true or false." >&2; exit 2 ;;
esac
case "${FEAR_SINGLE_HOVER_TEST}" in
  true) FEAR_SINGLE_HOVER_TEST_ARGUMENT="--fear-single-hover-test" ;;
  false) FEAR_SINGLE_HOVER_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_FEAR_SINGLE_HOVER_TEST must be true or false." >&2; exit 2 ;;
esac
case "${FEAR_SUITE_TEST}" in
  true) FEAR_SUITE_TEST_ARGUMENT="--fear-suite-test" ;;
  false) FEAR_SUITE_TEST_ARGUMENT="" ;;
  *) echo "HARDWARE_FEAR_SUITE_TEST must be true or false." >&2; exit 2 ;;
esac
case "${FEAR_FIRST_PAW}" in
  left|right) ;;
  *) echo "HARDWARE_FEAR_FIRST_PAW must be left or right." >&2; exit 2 ;;
esac
case "${FEAR_LIFT_METERS}" in
  ''|*[!0-9.]*) echo "HARDWARE_FEAR_LIFT_METERS must be numeric." >&2; exit 2 ;;
esac
COMMISSIONING_MODES=0
if [[ "${JOY_PAW_TEST}" == true || "${JOY_SUITE_TEST}" == true ]]; then
  ((COMMISSIONING_MODES += 1))
fi
if [[ "${ANGER_SINGLE_STOMP_TEST}" == true || "${ANGER_SUITE_TEST}" == true ]]; then
  ((COMMISSIONING_MODES += 1))
fi
if [[ "${SADNESS_BODY_VISUAL_TEST}" == true ||
      "${SADNESS_SINGLE_HOVER_TEST}" == true ||
      "${SADNESS_SUITE_TEST}" == true ]]; then
  ((COMMISSIONING_MODES += 1))
fi
if [[ "${FEAR_BODY_VISUAL_TEST}" == true ||
      "${FEAR_SINGLE_HOVER_TEST}" == true || "${FEAR_SUITE_TEST}" == true ]]; then
  ((COMMISSIONING_MODES += 1))
fi
if ((COMMISSIONING_MODES > 1)); then
  echo "Joy, anger, sadness, and fear commissioning modes are mutually exclusive." >&2
  exit 2
fi

cleanup() {
  trap - EXIT INT TERM
  if [[ -n "${EXPRESSION_PID}" ]] && kill -0 "${EXPRESSION_PID}" 2>/dev/null; then
    ssh "${SSH_OPTIONS[@]}" -J "${MOTION_SSH}" "${PERCEPTION_SSH}" \
      "if test -r '${REMOTE_RUNNER_PID_FILE}'; then read -r pid <'${REMOTE_RUNNER_PID_FILE}'; case \"\$pid\" in (*[!0-9]*|'') ;; (*) kill -TERM \"\$pid\" 2>/dev/null || true ;; esac; fi" \
      >/dev/null 2>&1 || true
    for _attempt in $(seq 1 30); do
      kill -0 "${EXPRESSION_PID}" 2>/dev/null || break
      sleep 0.1
    done
    kill -KILL "${EXPRESSION_PID}" 2>/dev/null || true
  fi
  wait "${EXPRESSION_PID}" 2>/dev/null || true
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
  "set -e; systemctl is-active 'emotion-bot-hardware-core@${ROBOT_USER}.service'; systemctl is-active 'emotion-bot-hardware-telemetry@${ROBOT_USER}.service'; source /opt/ros/noetic/setup.bash; source ~/emotion_bot_lite3_hw_ws/devel/setup.bash; test \"\$(rosparam get /emotion_bot/hardware/safety/transmit_enabled)\" = false; test \"\$(rosparam get /emotion_bot/hardware/safety/dynamic_actions_enabled)\" = false; test -x ~/emotion_bot_lite3_hw_ws/bin/motion_sdk_expression_runner; test ! -e /dev/shm/emotion_bot_lite3_direct_joint.owned"

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

echo "Hardware link ready; launching the development-computer brain (${CHAT_BACKEND})."
docker exec -t "${CONTAINER_NAME}" bash -lc \
  "echo \$\$ > '${BRAIN_PID_FILE}' && source /opt/ros/noetic/setup.bash && source /workspaces/lite3-noetic/ws/Lite3_VMC/devel/setup.bash && exec roslaunch emotion_bot_ros hardware_brain.launch chat_backend:=${CHAT_BACKEND} chat_model:=${CHAT_MODEL} uplink_port:=${TUNNEL_PORT}" &
BRAIN_PID=$!

echo "Starting the continuous official MotionSDK expression owner (allowlist: ${COMMISSIONED_EMOTIONS}, scale: ${PROFILE_SCALE})."
ssh "${SSH_OPTIONS[@]}" -J "${MOTION_SSH}" "${PERCEPTION_SSH}" \
  "set -e; umask 077; echo \$\$ >'${REMOTE_RUNNER_PID_FILE}'; exec ~/emotion_bot_lite3_hw_ws/bin/motion_sdk_expression_runner --execute --continuous --commissioned-emotions='${COMMISSIONED_EMOTIONS}' --profile-scale='${PROFILE_SCALE}' --minimum-battery='${MINIMUM_BATTERY}' --pid-file='${REMOTE_RUNNER_PID_FILE}' ${JOY_PAW_TEST_ARGUMENT} ${JOY_SUITE_TEST_ARGUMENT} --paw-lift-meters='${PAW_LIFT_METERS}' ${ANGER_SINGLE_STOMP_TEST_ARGUMENT} ${ANGER_SUITE_TEST_ARGUMENT} --anger-suite-cycles='${ANGER_SUITE_CYCLES}' --anger-first-paw='${ANGER_FIRST_PAW}' --anger-lift-meters='${ANGER_LIFT_METERS}' ${SADNESS_BODY_VISUAL_TEST_ARGUMENT} --sadness-body-visual-cycles='${SADNESS_BODY_VISUAL_CYCLES}' ${SADNESS_SINGLE_HOVER_TEST_ARGUMENT} ${SADNESS_SUITE_TEST_ARGUMENT} --sadness-first-paw='${SADNESS_FIRST_PAW}' --sadness-lift-meters='${SADNESS_LIFT_METERS}' ${FEAR_BODY_VISUAL_TEST_ARGUMENT} ${FEAR_SINGLE_HOVER_TEST_ARGUMENT} ${FEAR_SUITE_TEST_ARGUMENT} --fear-first-paw='${FEAR_FIRST_PAW}' --fear-lift-meters='${FEAR_LIFT_METERS}'" &
EXPRESSION_PID=$!

set +e
wait -n "${BRAIN_PID}" "${EXPRESSION_PID}"
status=$?
set -e
exit "${status}"
