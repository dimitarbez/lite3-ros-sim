#!/usr/bin/env bash
set -euo pipefail

CONTAINER_NAME="${LITE3_CONTAINER_NAME:-lite3-noetic-dev}"

docker exec -it "${CONTAINER_NAME}" bash -lc '
  cd /workspaces/lite3-noetic
  export ROS_MASTER_URI="${ROS_MASTER_URI:-http://127.0.0.1:11311}"
  export ROS_HOSTNAME="${ROS_HOSTNAME:-127.0.0.1}"
  export ROS_IP="${ROS_IP:-127.0.0.1}"
  source /opt/ros/noetic/setup.bash
  if [ -f /workspaces/lite3-noetic/ws/Lite3_VMC/devel/setup.bash ]; then
    source /workspaces/lite3-noetic/ws/Lite3_VMC/devel/setup.bash
    cd /workspaces/lite3-noetic/ws/Lite3_VMC
  fi
  exec bash -i
'
