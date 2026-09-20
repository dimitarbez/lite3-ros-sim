#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOTION_SSH="${ROBOT_MOTION_SSH:-ysc@192.168.2.1}"
PERCEPTION_SSH="${ROBOT_PERCEPTION_SSH:-ysc@192.168.1.103}"
ROBOT_USER="${ROBOT_USER:-ysc}"
MOTION_SDK_DIR="${HARDWARE_MOTION_SDK_DIR:-/home/${ROBOT_USER}/Lite3_MotionSDK}"
SSH_OPTIONS=(-o ConnectTimeout=8 -o ServerAliveInterval=5 -o ServerAliveCountMax=3)
JUMP_OPTIONS=(-J "${MOTION_SSH}")

for value in "${MOTION_SSH}" "${PERCEPTION_SSH}" "${ROBOT_USER}"; do
  case "${value}" in
    *[!a-zA-Z0-9_@.:-]* ) echo "Robot SSH values contain unsupported characters." >&2; exit 2 ;;
  esac
done
case "${MOTION_SDK_DIR}" in
  /*) ;;
  *) echo "HARDWARE_MOTION_SDK_DIR must be an absolute path." >&2; exit 2 ;;
esac
case "${MOTION_SDK_DIR}" in
  *[!a-zA-Z0-9_./-]*) echo "HARDWARE_MOTION_SDK_DIR contains unsupported characters." >&2; exit 2 ;;
esac

RUNTIME_DIR="$(mktemp -d)"
cleanup() {
  rm -rf -- "${RUNTIME_DIR}"
}
trap cleanup EXIT INT TERM
umask 077
openssl rand -hex 32 >"${RUNTIME_DIR}/stop-relay.key"

echo "Checking the motion host without changing it..."
ssh "${SSH_OPTIONS[@]}" "${MOTION_SSH}" \
  'set -e; test "$(uname -m)" = aarch64; ip link show p2p0 >/dev/null; ip -4 addr show p2p0; pgrep -a -f "(/jy_exe|/deeprcs)"; ping -c 2 -W 2 192.168.1.103'

echo "Checking the perception host without changing vendor files..."
ssh "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${PERCEPTION_SSH}" \
  'set -e; test "$(uname -m)" = aarch64; test -f /opt/ros/noetic/setup.bash; test -f ~/lite_cog/transfer/devel/setup.bash; ip link show eth0 >/dev/null; systemctl is-active transfer.service; source /opt/ros/noetic/setup.bash; source ~/lite_cog/transfer/devel/setup.bash; timeout 5 rostopic echo -n 1 /joint_states >/dev/null; timeout 5 rostopic echo -n 1 /imu/data >/dev/null'

echo "Copying the separate perception workspace..."
ssh "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${PERCEPTION_SSH}" \
  'mkdir -p ~/emotion_bot_lite3_hw_ws/src/emotion_bot_lite3_hw ~/emotion_bot_lite3_hw_ws/tools ~/emotion_bot_lite3_hw_ws/bin ~/emotion_bot_install_stage/perception'
rsync -rlv --omit-dir-times --exclude '__pycache__/' --exclude '*.pyc' -e "ssh ${SSH_OPTIONS[*]} ${JUMP_OPTIONS[*]}" \
  "${ROOT_DIR}/hardware-ws/src/emotion_bot_lite3_hw/" \
  "${PERCEPTION_SSH}:emotion_bot_lite3_hw_ws/src/emotion_bot_lite3_hw/"
rsync -rlv --omit-dir-times --exclude '__pycache__/' --exclude '*.pyc' -e "ssh ${SSH_OPTIONS[*]} ${JUMP_OPTIONS[*]}" \
  "${ROOT_DIR}/hardware-ws/host-install/perception/" \
  "${PERCEPTION_SSH}:emotion_bot_install_stage/perception/"
rsync -rlv --delete --omit-dir-times --exclude 'build/' -e "ssh ${SSH_OPTIONS[*]} ${JUMP_OPTIONS[*]}" \
  "${ROOT_DIR}/hardware-ws/tools/" \
  "${PERCEPTION_SSH}:emotion_bot_lite3_hw_ws/tools/"
scp "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${RUNTIME_DIR}/stop-relay.key" \
  "${PERCEPTION_SSH}:emotion_bot_install_stage/stop-relay.key"

echo "Building the independent perception workspace..."
ssh "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${PERCEPTION_SSH}" \
  'set -e; find ~/emotion_bot_lite3_hw_ws/src/emotion_bot_lite3_hw -type f -exec touch {} +; source /opt/ros/noetic/setup.bash; source ~/lite_cog/transfer/devel/setup.bash; cd ~/emotion_bot_lite3_hw_ws; test -e src/CMakeLists.txt || catkin_init_workspace src; catkin_make clean; catkin_make -DCMAKE_BUILD_TYPE=Release; source devel/setup.bash; python3 -c "from emotion_bot_lite3_hw.msg import DirectJointCommand, DirectJointFeedback, DirectJointSafety, RobotState"; devel/lib/emotion_bot_lite3_hw/test_motion_sdk_protocol'

echo "Building the ROS-independent official MotionSDK expression runtime..."
ssh "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${PERCEPTION_SSH}" \
  "set -e; test -f '${MOTION_SDK_DIR}/include/sender.h'; cmake -S ~/emotion_bot_lite3_hw_ws/tools -B ~/emotion_bot_lite3_hw_ws/tools/build -DBUILD_EXPRESSION_RUNTIME=ON -DMOTION_SDK_DIR='${MOTION_SDK_DIR}' -DCMAKE_BUILD_TYPE=Release; cmake --build ~/emotion_bot_lite3_hw_ws/tools/build -j4; cd ~/emotion_bot_lite3_hw_ws/tools/build; ctest --output-on-failure; install -m 0755 motion_sdk_expression_runner ~/emotion_bot_lite3_hw_ws/bin/motion_sdk_expression_runner"

echo "Staging the ROS-less passive observer on the motion host..."
ssh "${SSH_OPTIONS[@]}" "${MOTION_SSH}" \
  'mkdir -p ~/emotion_bot_install_stage/motion'
rsync -rlv --omit-dir-times --exclude '__pycache__/' --exclude '*.pyc' -e "ssh ${SSH_OPTIONS[*]}" \
  "${ROOT_DIR}/hardware-ws/host-install/motion/" \
  "${MOTION_SSH}:emotion_bot_install_stage/motion/"
scp "${SSH_OPTIONS[@]}" \
  "${ROOT_DIR}/hardware-ws/motion-host/motion_stop_observer.py" \
  "${ROOT_DIR}/hardware-ws/src/emotion_bot_lite3_hw/src/emotion_bot_lite3_hw/protocol.py" \
  "${ROOT_DIR}/hardware-ws/src/emotion_bot_lite3_hw/src/emotion_bot_lite3_hw/stop_transport.py" \
  "${ROOT_DIR}/hardware-ws/src/emotion_bot_lite3_hw/src/emotion_bot_lite3_hw/__init__.py" \
  "${RUNTIME_DIR}/stop-relay.key" \
  "${MOTION_SSH}:emotion_bot_install_stage/motion/"

echo "Installing narrowly privileged services. sudo may request the robot password."
ssh -tt "${SSH_OPTIONS[@]}" "${MOTION_SSH}" \
  "sudo ~/emotion_bot_install_stage/motion/install.sh '${ROBOT_USER}' ~/emotion_bot_install_stage/motion/stop-relay.key"
ssh -tt "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${PERCEPTION_SSH}" \
  "sudo ~/emotion_bot_install_stage/perception/install.sh '${ROBOT_USER}' ~/emotion_bot_install_stage/stop-relay.key"

echo "Removing only the staged copies of the generated relay key..."
ssh "${SSH_OPTIONS[@]}" "${MOTION_SSH}" \
  'rm -f -- ~/emotion_bot_install_stage/motion/stop-relay.key'
ssh "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${PERCEPTION_SSH}" \
  'rm -f -- ~/emotion_bot_install_stage/stop-relay.key'

echo "Verifying fail-closed services and status..."
ssh "${SSH_OPTIONS[@]}" "${MOTION_SSH}" \
  "set -e; for attempt in \$(seq 1 20); do test \"\$(systemctl is-active 'emotion-bot-stop-observer@${ROBOT_USER}.service')\" = active && exit 0; sleep 0.25; done; systemctl status 'emotion-bot-stop-observer@${ROBOT_USER}.service' --no-pager; exit 1"
ssh "${SSH_OPTIONS[@]}" "${JUMP_OPTIONS[@]}" "${PERCEPTION_SSH}" \
  "set -e; for attempt in \$(seq 1 20); do if test \"\$(systemctl is-active 'emotion-bot-hardware-core@${ROBOT_USER}.service')\" = active && test \"\$(systemctl is-active 'emotion-bot-hardware-telemetry@${ROBOT_USER}.service')\" = active; then break; fi; sleep 0.25; done; test \"\$(systemctl is-active 'emotion-bot-hardware-core@${ROBOT_USER}.service')\" = active; test \"\$(systemctl is-active 'emotion-bot-hardware-telemetry@${ROBOT_USER}.service')\" = active; source /opt/ros/noetic/setup.bash; source ~/emotion_bot_lite3_hw_ws/devel/setup.bash; test \"\$(rosparam get /emotion_bot/hardware/safety/transmit_enabled)\" = false; test \"\$(rosparam get /emotion_bot/hardware/safety/dynamic_actions_enabled)\" = false; timeout 8 rostopic echo -n 1 /emotion_bot/hardware/status"

echo "Hardware hosts are installed and remain DISARMED. Use make run-emotion-hardware, then make run-emotion-chat."
