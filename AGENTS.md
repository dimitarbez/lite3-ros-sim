# Lite3 emotion robot project guide

## Mission

Build and verify a simulation-first ROS 1 system in which the DEEP Robotics Lite3 runs in Gazebo and the `emotion-bot` emotional state can be consumed by ROS nodes and expressed through safe, bounded robot behavior.

The immediate target is simulation. Do not operate a physical robot, send UDP packets to a motion host, run `example_lite3_real`, change a robot's ROS version, or enable real-world auto mode unless the user explicitly asks for hardware work.

## Repository map

- `lite3-noetic/` is the maintained Ubuntu 20.04, ROS Noetic, and Gazebo Docker wrapper. Run its workflow through its `Makefile`.
- `lite3-noetic/ws/Lite3_VMC/` is the active catkin workspace and the place where simulator integration code is built. Its `src/CMakeLists.txt` intentionally points to the Noetic catkin toplevel file.
- `lite3_vmc_upstream/` is a clean reference checkout of `DeepRoboticsLab/Lite3_VMC`. Do not develop in it or copy the Melodic catkin symlink back into the Noetic workspace.
- `lite3-robot-docs/` contains manufacturer PDFs and searchable sibling Markdown conversions. Start with `lite3-robot-docs/README.md`.
- `emotion-bot/` is the preferred location for a checkout of <https://github.com/dimitarbez/emotion-bot>. It is not currently vendored. Record the commit used when adding it; do not silently update or copy its source into another package.
- Ignore generated catkin output under `build/`, `devel/`, and `log/` unless diagnosing a build. Do not hand-edit generated files.

## Sources of truth

Use the following order when facts conflict:

1. The checked-out code and launch/config files that are actually being built.
2. `lite3-noetic/README.md`, `lite3-noetic/docs/WORKFLOW.md`, and `lite3-noetic/docs/TROUBLESHOOTING.md` for this repository's wrapper workflow.
3. The Markdown files under `lite3-robot-docs/` for search and the matching PDFs for diagrams, tables, coordinate directions, limits, and safety-critical details.
4. Upstream Lite3 and emotion-bot repositories for behavior not represented locally.

Markdown files under `lite3-robot-docs/` are automated text extractions. Preserve the PDFs and consult them before making claims based on a figure or a visually structured table.

## Current behavior and known gaps

- The supported host workflow is WSL2/Ubuntu 24.04 with a Docker container based on Ubuntu 20.04 and ROS Noetic. Keep ROS 1 off the host.
- Normal simulator startup uses four interactive terminals: Gazebo, model/controller spawning, the Lite3 simulation controller, and keyboard control.
- `make run-spawn` must remain attached to a real terminal. The upstream spawn node waits for Enter to start controllers and another Enter to stop and remove them; EOF can cause immediate cleanup.
- Simulation joint state is `/lite3_gazebo/joint_states`. The physical robot documentation describes `/joint_states`, `/imu/data`, `/leg_odom`, and `/cmd_vel`; do not assume those real-robot names exist unchanged in Gazebo.
- Keyboard control publishes `sensor_msgs/Joy` on `/joy`.
- The source contains a `/cmd_vel` receiver, and `lite3_sim/main.yaml` contains `speed_update_mode`, but the current simulation executable does not instantiate that receiver and `qrRobotRunner` reads `speed_update_mode` without applying it. Treat `/cmd_vel` control in simulation as unimplemented until a test proves otherwise.
- The manufacturer perception manual describes `/cmd_vel` as `geometry_msgs/Twist`: positive `linear.x` is forward, positive `linear.y` is left, and positive `angular.z` turns left. It describes the ROS-to-motion-host bridge as `message_transformer`; that bridge is for hardware and is not the Gazebo controller.
- The motion-host manual specifies a UDP heartbeat of at least 2 Hz and a software emergency-stop command. These are hardware protocol requirements, not simulation APIs.
- The current emotion-bot entry point is an interactive CLI with a matplotlib plot and an OpenAI-key check. Integrate its reusable emotional-core objects; do not automate the CLI through stdin.
- Emotion-bot tracks valence, arousal, and one of nine discrete emotions. Its full pinned Python dependency set is substantially newer than ROS Noetic's Python 3.8 base, so validate interpreter and wheel compatibility before installing it in the ROS image. Prefer a separate runtime/bridge if the dependencies cannot coexist cleanly.

## Target integration shape

Keep emotional reasoning separate from robot actuation:

1. An emotion adapter owns the emotion-bot state and accepts text/events.
2. It publishes a stable ROS-facing state contract containing at least emotion name, valence, arousal, and timestamp.
3. A separate expression mapper converts that state into configurable robot behavior.
4. A safety layer clamps commands, expires stale commands to zero, and can disable all motion while leaving emotional state reporting active.

Prefer a small catkin package such as `emotion_bot_ros` under `lite3-noetic/ws/Lite3_VMC/src/` for ROS adapters and launch files. Keep domain logic in the emotion-bot checkout rather than duplicating it in callbacks. Use ROS parameters/YAML for emotion-to-motion mappings, limits, timeouts, and enable flags.

Start with a transport contract that is easy to inspect from the terminal. A JSON payload on `std_msgs/String` is acceptable for the first vertical slice; introduce custom messages only when typed consumers justify them. Namespace new topics under `/emotion_bot/...` and avoid changing upstream topic names without a compatibility reason.

Do not let the emotion engine publish motor, joint-effort, UDP, or unbounded velocity commands directly. An emotion such as anger or surprise must never bypass the same limits and stop behavior used for every other state. For integrated Gazebo and emotion-chat workflows, emotional motion must always be enabled once simulator readiness, health, and stance preparation pass; default commanded velocity remains zero. Never hand a live Gazebo/chat session back to the user with `motion_enabled: false`. If a transient watchdog event disables motion, wait for `health_ok: true` and `stance_prepared: true`, explicitly re-enable `/emotion_bot/set_motion_enabled`, and verify `motion_enabled: true` before handoff. This simulation-only rule does not authorize physical-robot, UDP, or motion-host actuation.

## Development workflow

Run wrapper commands from the repository root with `make -C lite3-noetic <target>`, or enter `lite3-noetic/` first.

Initial setup:

```bash
make -C lite3-noetic build
make -C lite3-noetic start
make -C lite3-noetic bootstrap
```

Baseline simulation, in four terminals:

```bash
make -C lite3-noetic run-gazebo
make -C lite3-noetic run-spawn
make -C lite3-noetic run-sim
make -C lite3-noetic run-keyboard
```

Press Enter in the spawn terminal once to load/start controllers. Do not send the second Enter until the test is over.

Inspect the graph from another terminal:

```bash
make -C lite3-noetic topics
make -C lite3-noetic joint-states
make -C lite3-noetic rqt-graph
```

After source changes, build in the container rather than on the Noble host:

```bash
docker exec lite3-noetic-dev bash -lc \
  'cd /workspaces/lite3-noetic/ws/Lite3_VMC && source /opt/ros/noetic/setup.bash && catkin_make -DCMAKE_BUILD_TYPE=Release'
```

Source `/opt/ros/noetic/setup.bash` and `devel/setup.bash` in every raw `docker exec` command that invokes ROS packages. Keep C++ changes compatible with the package's existing C++14 setting and Python ROS nodes compatible with the interpreter that actually runs them.

## Change discipline

- Establish that the unmodified simulator starts before debugging the emotion integration.
- Make focused changes in the active Noetic workspace. Keep unrelated upstream changes out of the patch.
- Do not run `git pull` in either Lite3 checkout when local changes exist. `make bootstrap` performs a fast-forward pull, so inspect status first.
- Preserve topic message types and coordinate semantics. Document every new topic, parameter, node, and launch file.
- Never commit API keys, `.env` files, model caches, tokens, generated plots, or large downloaded model artifacts.
- Avoid loading transformer models during ROS package import. Use lazy initialization so launch, topic inspection, and unit tests remain fast and deterministic.
- Mock OpenAI/network calls in tests. Unit tests must not require a secret, paid API call, GUI, microphone, physical robot, or internet connection.
- Preserve emotion-bot's GPL-3.0 notices and check redistribution implications before vendoring or copying its code.

## Verification expectations

Match verification effort to the change, and report commands plus outcomes.

For documentation or shell-wrapper changes:

```bash
make -C lite3-noetic help
bash -n lite3-noetic/scripts/*.sh lite3-noetic/docker/*.sh
```

For catkin code, require a clean `catkin_make` in the container. For emotion logic, run the relevant emotion-bot pytest suite in its compatible environment. The upstream repository's `requirements.txt` does not currently declare pytest, so provision it as a development dependency rather than adding it to production runtime implicitly.

For ROS integration, verify at minimum:

- the adapter launches without a GUI or API key when using a deterministic test backend;
- a known input produces the expected emotion-state message;
- valence stays in `[-1, 1]` and arousal in `[0, 1]`;
- integrated Gazebo/chat launch enables emotional motion after readiness, health, and stance gates pass;
- Gazebo emotional motion remains enabled throughout interactive verification, and any transient watchdog disable is recovered and re-enabled before handoff;
- enabled Gazebo expression output produces only bounded commands;
- stale input, node shutdown, or adapter failure produces a zero/neutral command;
- existing keyboard control and `/lite3_gazebo/joint_states` still work;
- no command is sent to real-hardware UDP endpoints.

The simulation milestone is complete only when one documented launch sequence brings up Gazebo, the Lite3 controller, the emotion adapter, and the expression mapper; a scripted input changes the published emotion; Gazebo visibly reflects the configured expression; and shutdown leaves the robot stationary without controller errors.

## Hardware boundary

Hardware deployment is a separate milestone requiring explicit user direction and a fresh safety review against the Motion Development, Motion Host Communication Interface, Perception Development, and model-specific user manuals. Before any real command, confirm model, firmware, ROS version, network target, emergency-stop path, clear operating area, command timeout, and a human operator ready to intervene.
