#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run this installer through sudo." >&2
  exit 2
fi

ROBOT_USER="${1:-ysc}"
KEY_SOURCE="${2:-}"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROBOT_GROUP="$(id -gn "${ROBOT_USER}")"

if [[ ! -r "${KEY_SOURCE}" ]]; then
  echo "Usage: sudo install.sh ROBOT_USER KEY_FILE" >&2
  exit 2
fi
if ! ip link show p2p0 >/dev/null 2>&1; then
  echo "p2p0 is absent; refusing to install the STOP observer on the wrong host." >&2
  exit 2
fi
if ! pgrep -x jy_exe >/dev/null 2>&1 && ! pgrep -f '/jy_exe($| )' >/dev/null 2>&1; then
  echo "jy_exe is not running; refusing installation on an unverified motion host." >&2
  exit 2
fi

install -d -m 0755 /opt/emotion-bot-motion/lib/emotion_bot_lite3_hw /etc/emotion-bot
install -m 0755 "${SOURCE_DIR}/motion_stop_observer.py" /opt/emotion-bot-motion/
install -m 0644 "${SOURCE_DIR}/protocol.py" /opt/emotion-bot-motion/lib/emotion_bot_lite3_hw/
install -m 0644 "${SOURCE_DIR}/stop_transport.py" /opt/emotion-bot-motion/lib/emotion_bot_lite3_hw/
install -m 0644 "${SOURCE_DIR}/__init__.py" /opt/emotion-bot-motion/lib/emotion_bot_lite3_hw/
install -o root -g "${ROBOT_GROUP}" -m 0640 "${KEY_SOURCE}" /etc/emotion-bot/stop-relay.key
install -m 0644 "${SOURCE_DIR}/emotion-bot-stop-observer@.service" /etc/systemd/system/

systemctl daemon-reload
systemctl enable "emotion-bot-stop-observer@${ROBOT_USER}.service"
systemctl restart "emotion-bot-stop-observer@${ROBOT_USER}.service"

echo "Installed passive motion-host STOP observer for ${ROBOT_USER}; jy_exe was not changed."
