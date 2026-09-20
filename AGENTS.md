# Lite3 emotion robot workspace guide

## Mission and boundary

Build and verify a simulation-first ROS 1 system in which the DEEP Robotics Lite3 runs in Gazebo and EmotionBot state is expressed through bounded robot behavior.

Simulation is the normal development target. The workspace includes a dated hardware research record, but that record is not standing permission to operate a physical robot. Do not connect to a robot, capture its traffic, send motion-host UDP packets, run `example_lite3_real`, alter robot configuration, or enable real-world auto mode unless the user explicitly asks for that hardware action in the current task.

## Repository map

- This repository is the private `lite3-ros-sim` wrapper and pins three Git submodules. Commit source changes in the owning submodule first, then update the wrapper gitlink deliberately.
- `lite3-noetic/` is the maintained Ubuntu 20.04, ROS Noetic, and Gazebo Docker wrapper. Use its `Makefile`; keep ROS 1 off the host.
- `lite3-noetic/ws/Lite3_VMC/` is the maintained Lite3 fork and active catkin workspace. Its `src/CMakeLists.txt` intentionally targets the Noetic catkin toplevel. Simulator and `emotion_bot_ros` development happens here.
- `emotion-bot/` is the pinned EmotionBot checkout. Its supported integration API is the reusable, headless `emotional_core.engine.EmotionEngine`; do not drive the interactive CLI through stdin.
- `lite3_vmc_upstream/` is a pinned, clean reference checkout of `DeepRoboticsLab/Lite3_VMC`. Never develop in it, add files to it, or copy its Melodic catkin symlink into the active Noetic workspace.
- `lite3-robot-docs/` contains manufacturer PDFs and searchable Markdown conversions. Start with its `README.md`; the PDFs remain authoritative for figures, tables, and safety-critical details.
- Ignore generated catkin output under `build/`, `devel/`, and `log/` unless diagnosing a build. Never hand-edit generated files.

## Sources of truth

Use this order when facts conflict:

1. Checked-out code, launch/config files, tests, and superproject gitlinks actually in use.
2. `lite3-noetic/README.md`, `lite3-noetic/docs/USAGE.md`, `lite3-noetic/docs/WORKFLOW.md`, `lite3-noetic/docs/TROUBLESHOOTING.md`, and `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/README.md` for the implemented simulator workflow.
3. `lite3-noetic/hardware-ws/README.md` for the current split-host architecture and `tickets/README.md` plus its per-emotion tickets for current physical-expression status, acceptance gates, and remaining work.
4. `lite3-noetic/docs/VERIFICATION.md` for dated verification results. It is historical evidence, not proof that an edited checkout still passes.
5. `lite3-noetic/docs/HARDWARE_APP_CONTROL.md` for dated protocol investigations and physical commissioning evidence. It is an evidence record, not standing permission or proof that later edits remain commissioned.
6. Searchable manufacturer Markdown and the matching PDFs under `lite3-robot-docs/`.
7. Upstream Lite3 and EmotionBot documentation only for behavior not represented locally.

Do not bake a remembered submodule SHA into new guidance. Read the current wrapper gitlink and nested repository status. When documenting dated results, state the commit and date actually tested.

## Current behavior

- The supported host workflow is WSL2/Ubuntu 24.04 with Docker running Ubuntu 20.04, ROS Noetic, and Gazebo.
- Normal integrated targets start Gazebo, the model, all controllers, the Lite3 runner, chat, EmotionBot, the expression mapper, and the safety bridge. The four-terminal upstream flow is retained for baseline isolation.
- `make run-spawn` must remain attached to a real terminal. The upstream node uses one Enter to start controllers and a second Enter to stop/remove them; EOF can cause immediate cleanup.
- Simulation joint state is `/lite3_gazebo/joint_states`. Physical-robot topic names in vendor documents must not be assumed to exist in Gazebo.
- The upstream keyboard publishes `sensor_msgs/Joy` on `/joy`. The integrated controller's sole input is remapped to `/emotion_bot/joy_out` after manual/emotion arbitration and safety checks.
- The source contains a `/cmd_vel` receiver, but the current simulation executable does not instantiate it and does not apply `speed_update_mode`. Treat `/cmd_vel` control in simulation as unimplemented until a test proves otherwise.
- EmotionBot tracks valence in `[-1, 1]`, arousal in `[0, 1]`, and one of nine discrete emotions. `EmotionEngine` is headless and deterministic by default; the legacy `main.py` CLI still owns plotting and optional direct response generation.
- Do not install EmotionBot's full pinned dependency set into the ROS Noetic image. The ROS adapter imports the compatible headless core; live OpenAI replies run in the separate Python 3.12 sidecar.
- Normal simulation chat expression profiles are planted: x/y/yaw stay zero. Joy/surprise hops and anger stomps are bounded simulator-controller actions, not hardware commands or external Gazebo wrenches. The separately named physical runner has an allowlist-gated Anger paw state machine whose canonical-reset three-cycle suite is physically accepted, but normal selection remains disabled pending a live chat/retarget test.
- The official physical-expression path is separately named and requires initial robot state `1`. It owns MotionSDK continuously for one operator-started session and stands once through the vendor `RobotStateInit -> PreStandUp -> StandUp` sequence.
- Physical completion is category-specific. Neutral, Joy, Sadness, Fear, and Anger have physically and visually accepted reactions. Fear's final animation was observed working correctly on the robot. Normal chat selection and retargeting for the accepted non-neutral reactions remain incomplete or live-unvalidated, so the checked-in normal allowlist remains `neutral`. Affection, Curiosity, Disgust, and Surprise remain planned.
- Airborne hops, gait/action primitives, external wrenches, torque strikes, and open-loop foot lifts remain out of scope. Low-clearance foot gestures require the accepted IK, torque-derived unload/support/landing gates, safe lowering on failure, and explicit per-category commissioning.

## Implemented integration boundary

Keep emotional reasoning separate from actuation:

1. `EmotionEngine` owns appraisal, personality, memory, valence, arousal, and categorical emotion.
2. `emotion_bot_ros` adapts conversation events and publishes a versioned ROS-facing state contract.
3. The expression mapper converts state to declarative, rate-limited posture/action intentions.
4. The safety bridge clamps, arbitrates, expires stale sources to zero, gates readiness/health/stance, and publishes the sole simulator Joy input.
5. The Lite3 controller executes only the bounded command it receives after those gates.

Keep domain logic in `emotion-bot`, ROS contracts/orchestration in `emotion_bot_ros`, and controller mechanics in `quadruped`. `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/config/default.yaml` is the source of truth for topic names, mappings, limits, timeouts, and enable flags.

The current state/conversation/action contracts use inspectable JSON on `std_msgs/String`; motion intentions use `geometry_msgs/Twist`, and controller transport uses `sensor_msgs/Joy`. Preserve schema versions, turn correlation, generation ordering, message types, and the `/emotion_bot/...` namespace unless a coordinated migration is requested.

No emotional state may publish motor, joint-effort, UDP, or unbounded velocity commands directly. Anger, surprise, and other actions pass through the same limits, watchdogs, cancellation, and stop behavior as every other state.

On hardware, validated state is copied to a sequence-locked shared record; only `motion_sdk_expression_runner` reads it and sends joints. The final transition contract first finishes lowering and settling any raised paw, returns over 1.5 seconds to exact canonical stand, holds 0.35 seconds, and then starts only the newest pending category. The planted engine implements its portion of that contract; the Anger paw path implements it and is physically accepted in explicit suite mode, while its live chat retarget and the accepted joy paw state machine's normal integration are still pending. Link staleness performs a safe reset and release; STOP and safety faults bypass conversational timing and release immediately. `/emotion_bot/hardware/expression_status` is read-only evidence from the runner.

The torque-derived contact estimate is a guarded input, not a general permission to lift a foot. Its gate is enabled only by the explicit accepted paw suite until the normal joy integration and transition tests are complete. The Retroid height bridge and legacy posture/action nodes are diagnostic-only and must never run concurrently with the official owner.

## Development workflow

Run wrapper commands from the workspace root with `make -C lite3-noetic <target>`.

Initial setup:

```bash
make -C lite3-noetic build
make -C lite3-noetic start
make -C lite3-noetic bootstrap
```

Inspect the wrapper and all nested repository statuses before `bootstrap`. It preserves dirty or detached active workspaces; a clean attached Lite3 branch may be fetched and fast-forwarded.

Integrated simulation and chat, in two terminals:

```bash
make -C lite3-noetic run-emotion-sim-openai  # or run-emotion-sim offline
make -C lite3-noetic run-emotion-chat
```

Gate interactive handoff on `/emotion_bot/status`: `sim_ready`, `health_ok`, `stance_prepared`, and `motion_enabled` must all be true. If a watchdog disables motion, wait for health and stance recovery, call `/emotion_bot/set_motion_enabled` with true, and verify status again. Default commanded velocity remains zero; enabled means bounded expressions are permitted.

Upstream baseline simulation, in four attached terminals:

```bash
make -C lite3-noetic run-gazebo
make -C lite3-noetic run-spawn
make -C lite3-noetic run-sim
make -C lite3-noetic run-keyboard
```

After source changes, build in the container:

```bash
docker exec lite3-noetic-dev bash -lc \
  'cd /workspaces/lite3-noetic/ws/Lite3_VMC && source /opt/ros/noetic/setup.bash && catkin_make -DCMAKE_BUILD_TYPE=Release'
```

Source `/opt/ros/noetic/setup.bash` and `devel/setup.bash` in raw commands that invoke built ROS packages. Keep C++ compatible with the existing C++14 setting and ROS Python compatible with the interpreter that actually runs it.

## Change discipline

- Inspect all four Git repositories independently. A clean wrapper status does not prove its submodules are clean. Never silently advance a submodule or wrapper gitlink.
- Stay on the currently checked-out branch. Do not create or switch branches unless the user explicitly requests that branch operation in the current task.
- Make focused changes in the active Noetic fork and EmotionBot checkout. Keep `lite3_vmc_upstream` clean.
- Do not pull over local changes. `make bootstrap` may fast-forward a clean attached active checkout, so inspect status first.
- Preserve topic types and coordinate semantics. Document every new topic, parameter, node, launch file, Make target, and operator-visible recovery path.
- Keep the separate physical package under `lite3-noetic/hardware-ws`; never copy it into or modify vendor `~/lite_cog`, `qnx2ros`, `jy_exe`, or their configuration. Keep emotional reasoning on the development computer and the sole safety/trajectory owner on the perception computer.
- Keep `tickets/README.md` and the relevant per-emotion ticket aligned with implementation and evidence. Offline tests, a planted fallback, joint movement, or a successful explicit suite do not prove normal chat integration or commission another category.
- Preserve one exclusive hardware sender. The official MotionSDK runner, Retroid diagnostic bridge, and legacy posture/action nodes are mutually exclusive paths.
- Keep English `emotion-bot/README.md` and Macedonian `emotion-bot/README.mkd` aligned when changing shared user-facing behavior.
- Never commit API keys, `.env`, credentials, model caches, tokens, packet captures, generated plots, build output, or large downloaded artifacts.
- Avoid model loading during ROS package import. Mock network/OpenAI calls; ordinary tests must not require a secret, paid call, GUI, microphone, physical robot, or internet.
- Preserve EmotionBot's GPL-3.0 notices and review redistribution implications before copying or vendoring its code.

## Verification

Match checks to the change and report commands plus outcomes.

For documentation or wrapper changes:

```bash
make -C lite3-noetic help
bash -n lite3-noetic/scripts/*.sh lite3-noetic/docker/*.sh
```

For the integrated offline gate:

```bash
make -C lite3-noetic verify-emotion
```

For physical-expression software changes, also run `make -C lite3-noetic verify-hardware-offline`. This gate is offline and is not evidence of physical commissioning.

For catkin code, require a clean Release `catkin_make` in the container. For EmotionBot logic, use `requirements-dev.txt` or the wrapper's `emotion-unit-tests`; do not make pytest a production runtime dependency.

ROS integration checks must cover deterministic adapter launch without a key/GUI, state bounds and turn correlation, enabled bounded output, exact-zero stale/disable/shutdown behavior, existing keyboard and joint-state behavior, and the no-hardware boundary. A live visual claim additionally requires a close Gazebo camera plus state, action, Joy, pose, and contact telemetry.

The simulation milestone is complete only when one documented launch sequence brings up Gazebo, the controller, adapter, mapper, and safety bridge; scripted input changes emotion; Gazebo visibly reflects it; motion remains enabled after all gates pass; and shutdown leaves the robot stationary without controller errors.

## Hardware boundary

Hardware deployment requires explicit current-task direction and a fresh review of `tickets/README.md`, `lite3-noetic/hardware-ws/README.md`, `lite3-noetic/docs/HARDWARE_APP_CONTROL.md`, the Motion Development manual, Motion Host Communication Interface, Perception Development manual, and the exact model's user manual. Dated records prove only their named checkout, command path, conditions, telemetry, and operator observation. Do not generalize the Stand/Sit test, accepted neutral loop, or bounded joy suite to normal chat integration, another category, walking, airborne motion, concurrent controllers, or torque control.

The tested full Lite3 Android app is a high-level, non-ROS motion-host controller. It is not the generic DEEP Robotics gamepad-forwarding app, and neither app supplies this workspace's Gazebo `/joy` or `/cmd_vel` path. Do not reuse the generic gamepad packet format for the motion host or assume status telemetry replies to the command sender; follow the inspected hardware note and do not reconfigure the deployed telemetry target ad hoc.

Before any real command, confirm robot model and deployed software, fresh current state, network target, heartbeat ownership, emergency-stop/manual-takeover path, clear area, watchdog/timeout behavior, and a human operator ready to intervene. The Stand/Sit command is a toggle: precede it with fresh state telemetry, send it exactly once from an expected stable state, and never retry blindly. Software STOP is an emergency action, not ordinary timeout recovery.
