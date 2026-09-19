#!/usr/bin/env bash
set -eo pipefail

source /opt/ros/noetic/setup.bash
source "${ROBOT_HOME}/lite_cog/transfer/devel/setup.bash"

for _attempt in $(seq 1 120); do
  if rosnode list 2>/dev/null | grep -Fxq /qnx2ros && \
     rosnode list 2>/dev/null | grep -Fxq /ros2qnx; then
    exit 0
  fi
  sleep 0.25
done

echo "Vendor transfer graph did not become ready within 30 seconds." >&2
exit 1
