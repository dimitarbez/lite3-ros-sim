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

The hardware target checks the installed robot services, fail-closed legacy
flags, and absence of another sender; opens the loopback-only SSH tunnel through
`192.168.2.1`; starts the ephemeral OpenAI sidecar and brain; then starts the
sole continuous official MotionSDK expression runner. The uplink revalidates emotion-state 1.1
and sends transport-schema 1.0 NDJSON at 5 Hz with a random session ID and
increasing sequence. The receiver binds only to loopback, rejects frames over 2
KiB and replays, and measures freshness with the perception host's monotonic
clock. Each accepted envelope is also written to a versioned sequence-locked
record containing category, affect, turn, session/sequence, and receive time.
Ctrl-C terminates the runner through its normal MotionSDK release path before
removing the brain, tunnel, and sidecar.

Robot-side nodes live in the separate `hardware-ws` catkin workspace. The
perception telemetry service and ROS-less motion-host STOP observer receive only
`CAP_NET_RAW`; all other nodes remain unprivileged. The authenticated relay
crosses the internal network without sending to the robot command port. The
supervisor is the only arming authority and has mutually exclusive `DISARMED`,
`POSTURE_ARMED`, `ACTION_PENDING`, `SDK_TAKEOVER`, `ACTION_ACTIVE`, `RECOVERY`,
and `FAULT` states. See `hardware-ws/README.md` before any deployment.

The legacy posture/action graph remains absent from the official launch and its
transmit flags remain false. It is available only through
`hardware_diagnostic.launch` and `run-emotion-hardware-retroid-diagnostic`.
The official runner has an independent exclusive lease and marker, requires
initial state `1`, and defaults to the commissioned allowlist `neutral`.

### Official continuous hardware sequence

The runtime accepts control only from state `1`, executes the proven vendor
zero-to-stand sequence once, and keeps one 1 kHz SDK owner. All profiles keep
HipX, yaw, planar motion, and feed-forward torque zero. Neutral is the unchanged
3.25-second commissioned animal-breath loop. The other categories translate the
Gazebo keyframe order into `-0.008..+0.020 rad` base compression,
`+/-0.004 rad` roll bias, and `+/-0.003 rad` pitch bias; joy/surprise and anger
use planted pulses instead of hop or lifted-foot stomp.

Every category change performs a 1.5-second quintic return to canonical stand
and a 0.35-second exact hold. Rapid requests replace one pending target without
restarting that reset; same-category affect updates do not restart a loop. At a
0.75-second emotion-link timeout the same reset begins, and the runner releases
after it completes if the link remains stale. STOP, state `8`, nonzero errors,
attitude outside `+/-10 degrees`, battery below the configured 25% floor,
invalid/dead feedback, tracking error, signal, or process failure enters the
existing immediate hold/release path. Read-only JSON status schema `1.0` is
published on `/emotion_bot/hardware/expression_status`.

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
