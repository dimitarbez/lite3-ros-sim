# Workflow

## Setup

All ROS commands run in the Ubuntu 20.04/ROS Noetic container; keep ROS 1 off the Ubuntu 24.04 host. The read-only sibling checkout `/home/dimitarbez/Dev/ROS/emotion-bot` must be at `20c0c1361434bcf9ebaec4e8a5c9385e61c9c3e2`.

```bash
make -C lite3-noetic build
make -C lite3-noetic start
make -C lite3-noetic bootstrap
```

`build` creates both the Noetic image and the Python 3.12 OpenAI runtime image. `bootstrap` preserves a dirty Lite3 workspace, retains the Noetic catkin toplevel, and builds inside the container.

## Integrated simulation

Run the offline GUI system with one command:

```bash
make -C lite3-noetic run-emotion-sim
```

For live OpenAI conversation, put the key in the persistent local file `ws/Lite3_VMC/.env`:

```dotenv
OPENAI_API_KEY=replace-with-your-key
```

The file is Git-ignored and should remain mode `600`; `start-openai-bridge` sources it automatically. An already-exported environment value is also supported. Then run:

```bash
make -C lite3-noetic run-emotion-sim-openai
```

This is the normal interactive path. `run-emotion-chat` is a terminal UI, but its assistant responses come from OpenAI when the stack was launched with this target. The client reports `chat_backend=openai` after a successful live turn; the separate `emotion_backend=deterministic` label describes only the local emotional appraisal engine. Any API failure produces an explicit offline-fallback warning.

This starts a disposable loopback-only sidecar, then the same complete Gazebo stack with `chat_backend:=openai`. The sidecar uses the official OpenAI SDK 3.6.0, `gpt-5-mini`, the Responses streaming API, minimal reasoning, low verbosity, `store=false`, a 20-second request timeout, no SDK retries, and explicit upstream close on client cancellation. The coordinator waits up to 0.5 seconds for the accepted user's turn-correlated emotion before constructing the request. ROS-side policy adds one bounded retry and then deterministic fallback. Context, input, and response lengths are bounded in `config/default.yaml`; the live response cap is 300 output tokens so concise technical answers can complete without becoming unbounded.

For a display-free run:

```bash
make -C lite3-noetic run-emotion-sim-headless
```

`integrated_sim.launch` starts ROS master, Gazebo, Lite3, all 13 controllers, the simulation controller, chat coordinator, EmotionBot adapter, expression mapper, and required safety bridge. The controller loader does not use the upstream stdin-sensitive spawner. The safety bridge refuses enable until the simulation controller publishes its readiness latch.

The simulator consumes `sensor_msgs/Joy`; it does not instantiate its `/cmd_vel` receiver. Its sole input is remapped to `/emotion_bot/joy_out`, where manual and emotional commands have already passed arbitration and safety.

## Split-host hardware graph

Hardware is a different graph, workspace, configuration, and operator workflow. It never starts from `run-emotion-sim*`. With the robot sitting, install the fail-closed services once:

```bash
make -C lite3-noetic setup-emotion-hardware
```

Normal use is then two terminals:

```bash
make -C lite3-noetic run-emotion-hardware
make -C lite3-noetic run-emotion-chat
```

The hardware target checks the installed robot services and fail-closed flags,
opens the loopback-only SSH tunnel through `192.168.2.1`, starts the ephemeral
OpenAI sidecar, and launches the brain. The uplink revalidates emotion-state 1.1
and sends transport-schema 1.0 NDJSON at 5 Hz with a random session ID and
increasing sequence. The receiver binds only to loopback, rejects frames over 2
KiB and replays, and measures freshness with the perception host's monotonic
clock. Ctrl-C removes the tunnel and sidecar.

Robot-side nodes live in the separate `hardware-ws` catkin workspace. The
perception telemetry service and ROS-less motion-host STOP observer receive only
`CAP_NET_RAW`; all other nodes remain unprivileged. The authenticated relay
crosses the internal network without sending to the robot command port. The
supervisor is the only arming authority and has mutually exclusive `DISARMED`,
`POSTURE_ARMED`, `ACTION_PENDING`, `SDK_TAKEOVER`, `ACTION_ACTIVE`, `RECOVERY`,
and `FAULT` states. See `hardware-ws/README.md` before any deployment.

The repository defaults deliberately cannot move hardware: transmission and dynamic actions are false, commissioned limits/rates and posture amplitudes are zero, the exact `0x0906` layout is unset, STOP preemption is unverified, and trajectories are empty. Do not turn those fields into guessed values.

### Default neutral hardware sequence

The first measured height motion on 2026-09-19 used a direct diagnostic that
reproduced the full Lite3 app's captured protocol. The maintained
`run-emotion-hardware` target now starts the same neutral-only host bridge by
default. Its exact session order is four heartbeats at
2 Hz, Move, a 2.0-second wait, Pose, and a 1.5-second wait. Each 50 Hz height
sample was then preceded immediately by command `0x21010135` with the captured
Retroid companion value `32768`. Shutdown sent five yaw-`0`/height-`0` pairs at
20 ms spacing.

The bridge uses source port `43897`, target `192.168.2.1:43893`, a height
envelope of `0` to `-10000`, `4000 units/s` maximum rate, a 4-second entrance,
and a 10-second period. Before transmission it requires `DISARMED`,
stable basic state `6`, zero errors, fresh telemetry/joints/IMU/AI link, fresh
centered Retroid input, STOP false, and 12 finite joint values. The operator had
the Retroid STOP ready in a clear, level area. Dynamic actions, locomotion,
direct joints, and torque output remained out of scope.

The operator authorized a 25% minimum battery; that is the explicit runtime
floor and output pauses below it. It does not replace the Pro manual's 75% start
recommendation. The checked-in robot-side transmission flags remain false: the
launcher-owned bridge is exclusive, neutral-only, freshness-gated, and guarded
by a 250 ms relay watchdog plus the five-pair exact-zero fallback.

## Conversation and turn ordering

Start the terminal client in another terminal:

```bash
make -C lite3-noetic run-emotion-chat
```

Ordinary text or exact `event:<emotion>` input is accepted immediately with a turn ID. `started`, `delta`, retry/fallback, and terminal events stream on `/emotion_bot/chat/events`. The final reply is also published on `/emotion_bot/chat/response`. A new user input explicitly cancels any unfinished turn. Turn indexes and IDs prevent late, cancelled, duplicate, or out-of-order assistant completions from changing emotional state; the last accepted index is retained on the ROS parameter server so a restarted chat node continues monotonically within the same graph.

Only two semantic events feed EmotionBot: the accepted user message and the current turn's completed assistant response. EmotionBot owns appraisal, valence, arousal, categorical emotion, personality, and memory. The adapter publishes state contract 1.1 with the originating `turn_id`; the chat client reports the matching state alongside the completed response.

The repeatable offline demo remains available:

```bash
make -C lite3-noetic emotion-demo
make -C lite3-noetic emotion-demo EMOTION_MOTION=true
```

The first form never enables motion. The second enables it explicitly and disables it in a `finally` block.

## Fluid expression and safety

Motion starts disabled. Enable it only after the model is standing:

```bash
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash && rosservice call /emotion_bot/set_motion_enabled "data: true"'
```

The mapper runs at 20 Hz. It low-pass filters valence and arousal with elapsed-time constants, applies a `1.25` theatrical gain to entrances, keeps same-category conversation appraisals in the current loop, and rate-limits all commanded axes. Every non-neutral category change cancels the old action, spends 0.25 seconds returning to exact zero, holds that canonical command for another 0.35 seconds so the physical stance can settle, and only then starts the new entrance. Recurring idle keyframes run at 65% entrance amplitude and 1.25x duration so they stay readable without looking jumpy. Stale input returns to exact zero. Pattern values remain declarative per emotion.

The development default uses centered, unfixed expressions. When the simulation controller becomes ready, the bridge first enters the four-contact stance and waits four simulated seconds for the upstream transition to settle. Joy performs one full hop and then loops the enlarged body-height/roll/pitch envelope instead of selecting the unstable Lite3 walk gait. In simulation stance mode, a 40 Nm/rad position term centers every HipX abduction joint; this prevents animation changes from accumulating the crossed-leg/inclined-V pose while leaving physical-robot control untouched. At 0.09 m from the enable-time center, the simulator-only recenter holds all four feet in stance and makes bounded 0.012 m incremental world-position corrections until it is inside 0.035 m; the model is never fixed or permanently constrained. The integrated launch monitors finite model pose, torso height, tilt, and all 12 joint states; invalid data or a monitoring gap disables motion and appears in `/emotion_bot/status`. Stale state/command, malformed state, disable, readiness loss, and upstream source loss return to a zero command.

Use the explicit `run-emotion-sim-locomotion` target only for supervised gait development. Normal chat profiles publish zero planar commands even though the safety layer retains locomotion support for manual development and recenter handling.

Inspect state and decisions with:

```bash
make -C lite3-noetic topics
make -C lite3-noetic joint-states
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash && rostopic echo /emotion_bot/status'
```

## Manual keyboard control

While the integrated simulation runs:

```bash
make -C lite3-noetic run-emotion-keyboard
```

The target remaps the existing keyboard node to `/emotion_bot/manual_joy` and enables its posture mode: w/s pitch, a/d roll, and q/e height. Active manual input has priority for 0.75 seconds. Locomotion is permitted by the default runtime profile, though this keyboard target intentionally retains posture-oriented axis bindings. Both sources expire after 0.5 seconds; command timeout always causes exact zero.

## Verification

Run the complete offline gate:

```bash
make -C lite3-noetic verify-emotion
```

Individual targets are `emotion-static-checks`, `emotion-clean-build`, `emotion-unit-tests`, `emotion-openai-bridge-test`, `emotion-ros-tests`, `emotion-gazebo-test`, and `emotion-animation-gazebo-test`. The Gazebo tests record `/tmp/emotion_bot_gazebo_e2e_*.log` in the container and verify turn/state correlation, streamed deterministic response, controller/joint health, measurable entrance and looping-idle movement for all nine emotions, rapid joy→anger→fear changes, bounded stop behavior, watchdog zeroing, teardown, and the hardware boundary.

A live smoke is separate so ordinary tests remain offline:

```bash
make -C lite3-noetic emotion-openai-live-smoke
```

It prints only pass/fail metadata, never generated text or the credential.

## Original simulator workflow

For upstream baseline isolation, run in four attached terminals:

```bash
make -C lite3-noetic run-gazebo
make -C lite3-noetic run-spawn
make -C lite3-noetic run-sim
make -C lite3-noetic run-keyboard
```

Press Enter once in `run-spawn` to start controllers. Keep it attached and do not send the second Enter until the test finishes.
