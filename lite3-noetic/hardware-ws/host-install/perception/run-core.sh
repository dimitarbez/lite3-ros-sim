#!/usr/bin/env bash
set -eo pipefail

source /opt/ros/noetic/setup.bash
source "${ROBOT_HOME}/lite_cog/transfer/devel/setup.bash"
source "${ROBOT_HOME}/emotion_bot_lite3_hw_ws/devel/setup.bash" --extend
exec roslaunch emotion_bot_lite3_hw hardware_core.launch
