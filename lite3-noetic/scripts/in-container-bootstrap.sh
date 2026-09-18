#!/usr/bin/env bash
set -euo pipefail

WORK_ROOT="/workspaces/lite3-noetic/ws"
REPO_DIR="${WORK_ROOT}/Lite3_VMC"
REPO_URL="${LITE3_VMC_REPO_URL:-https://github.com/DeepRoboticsLab/Lite3_VMC.git}"

export ROS_MASTER_URI="${ROS_MASTER_URI:-http://localhost:11311}"
source /opt/ros/noetic/setup.bash

mkdir -p "${WORK_ROOT}"

if [ ! -e "${REPO_DIR}/.git" ]; then
  git clone "${REPO_URL}" "${REPO_DIR}"
else
  if [ -n "$(git -C "${REPO_DIR}" status --porcelain)" ]; then
    echo "Lite3_VMC has local changes; preserving them and skipping fetch/pull."
  elif ! git -C "${REPO_DIR}" symbolic-ref -q HEAD >/dev/null; then
    echo "Lite3_VMC is pinned as a submodule; preserving its recorded commit."
  else
    git -C "${REPO_DIR}" fetch --all --tags
    git -C "${REPO_DIR}" pull --ff-only
  fi
fi

cd "${REPO_DIR}"

ln -snf /opt/ros/noetic/share/catkin/cmake/toplevel.cmake src/CMakeLists.txt

rosdep update

if ! rosdep install \
  --from-paths src \
  --ignore-src \
  --rosdistro noetic \
  --reinstall \
  -r \
  -y \
  --skip-keys="eigen libpcl-all-dev"; then
  echo "rosdep could not fully resolve Noetic keys. Continuing because ROS Noetic and the required system libraries are preinstalled in the image."
fi

catkin_make -DCMAKE_BUILD_TYPE=Release
