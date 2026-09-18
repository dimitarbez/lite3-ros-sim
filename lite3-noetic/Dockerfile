FROM osrf/ros:noetic-desktop-full-focal

ARG USERNAME=ros
ARG UID=1000
ARG GID=1000

ENV TZ=Etc/UTC
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8
ENV ROS_DISTRO=noetic
ENV ROS_PYTHON_VERSION=3
ENV QT_X11_NO_MITSHM=1
ENV LIBGL_ALWAYS_SOFTWARE=1
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    sudo \
    locales \
 && locale-gen en_US en_US.UTF-8 \
 && update-locale LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gdb \
    gfortran \
    libblas-dev \
    libeigen3-dev \
    libgazebo11-dev \
    libglm-dev \
    liblapack-dev \
    liblcm-dev \
    libpcl-dev \
    libpython3-dev \
    libyaml-cpp-dev \
    pkg-config \
    python3-catkin-tools \
    python3-dev \
    python3-osrf-pycommon \
    python3-pip \
    python3-pytest \
    python3-rosdep \
    python3-vcstool \
    ros-noetic-controller-manager \
    ros-noetic-effort-controllers \
    ros-noetic-joint-state-controller \
    ros-noetic-ros-control \
    ros-noetic-ros-controllers \
    x11-apps \
 && rm -rf /var/lib/apt/lists/*

RUN rosdep init || true

RUN groupadd --gid "${GID}" "${USERNAME}" \
 && useradd --uid "${UID}" --gid "${GID}" -m -s /bin/bash "${USERNAME}" \
 && usermod -aG sudo "${USERNAME}" \
 && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/${USERNAME}" \
 && chmod 0440 "/etc/sudoers.d/${USERNAME}"

RUN mkdir -p /workspaces/lite3-noetic \
 && chown -R "${UID}:${GID}" /workspaces

COPY docker/ros_entrypoint.sh /ros_entrypoint.sh
RUN chmod +x /ros_entrypoint.sh

USER ${USERNAME}
WORKDIR /workspaces/lite3-noetic

ENTRYPOINT ["/ros_entrypoint.sh"]
CMD ["bash"]
