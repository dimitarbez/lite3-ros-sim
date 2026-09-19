#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run this installer through sudo." >&2
  exit 2
fi

ROBOT_USER="${1:-ysc}"
KEY_SOURCE="${2:-}"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROBOT_HOME="$(getent passwd "${ROBOT_USER}" | cut -d: -f6)"
ROBOT_GROUP="$(id -gn "${ROBOT_USER}")"

if [[ -z "${ROBOT_HOME}" || ! -r "${KEY_SOURCE}" ]]; then
  echo "Usage: sudo install.sh ROBOT_USER KEY_FILE" >&2
  exit 2
fi
if [[ ! -f /opt/ros/noetic/setup.bash ]]; then
  echo "ROS Noetic is not installed on the perception host." >&2
  exit 2
fi
if [[ ! -f "${ROBOT_HOME}/lite_cog/transfer/devel/setup.bash" ]]; then
  echo "Vendor transfer workspace is missing; refusing to alter it." >&2
  exit 2
fi
CONFIG="${ROBOT_HOME}/emotion_bot_lite3_hw_ws/src/emotion_bot_lite3_hw/config/default.yaml"
if [[ ! -f "${ROBOT_HOME}/emotion_bot_lite3_hw_ws/devel/setup.bash" ]]; then
  echo "EmotionBot hardware workspace must be built before service installation." >&2
  exit 2
fi
if ! grep -Eq '^  transmit_enabled: false$' "${CONFIG}" || \
   ! grep -Eq '^  dynamic_actions_enabled: false$' "${CONFIG}"; then
  echo "Refusing installation: checked-in hardware configuration is not fail closed." >&2
  exit 2
fi

install -d -m 0755 /usr/local/libexec/emotion-bot /etc/emotion-bot
install -d -o "${ROBOT_USER}" -g "${ROBOT_GROUP}" -m 0755 "${ROBOT_HOME}/.ros"
install -m 0755 "${SOURCE_DIR}/run-core.sh" /usr/local/libexec/emotion-bot/run-perception-core
install -m 0755 "${SOURCE_DIR}/run-telemetry.sh" /usr/local/libexec/emotion-bot/run-perception-telemetry
install -m 0755 "${SOURCE_DIR}/wait-for-transfer.sh" /usr/local/libexec/emotion-bot/wait-for-transfer
install -o root -g "${ROBOT_GROUP}" -m 0640 "${KEY_SOURCE}" /etc/emotion-bot/stop-relay.key
install -m 0644 "${SOURCE_DIR}/emotion-bot-hardware-core@.service" /etc/systemd/system/
install -m 0644 "${SOURCE_DIR}/emotion-bot-hardware-telemetry@.service" /etc/systemd/system/

systemctl daemon-reload
systemctl enable "emotion-bot-hardware-core@${ROBOT_USER}.service"
systemctl enable "emotion-bot-hardware-telemetry@${ROBOT_USER}.service"
systemctl restart "emotion-bot-hardware-core@${ROBOT_USER}.service"
systemctl restart "emotion-bot-hardware-telemetry@${ROBOT_USER}.service"

echo "Installed fail-closed perception services for ${ROBOT_USER}."
