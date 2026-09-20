# Live OpenAI + Gazebo usage guide

This guide runs the conversational EmotionBot integration with the DEEP Robotics Lite3 in Gazebo. It is simulation-only; none of these commands starts the physical-robot executable or motion-host UDP bridge.

Run every host command from the repository root:

```bash
cd /home/dimitarbez/Dev/ROS
```

## First-time setup

Build the images, start the development container, and build the catkin workspace:

```bash
make -C lite3-noetic build
make -C lite3-noetic start
make -C lite3-noetic bootstrap
```

For live OpenAI chat, store the key in the persistent local file `lite3-noetic/ws/Lite3_VMC/.env`:

```dotenv
OPENAI_API_KEY=replace-with-your-key
```

Protect the file and confirm Git does not track it:

```bash
chmod 600 lite3-noetic/ws/Lite3_VMC/.env
git -C lite3-noetic/ws/Lite3_VMC status --short -- .env
```

The final command should print nothing. The live launch reads this file automatically; do not put the key in source, ROS parameters, YAML, launch files, or a command argument.

## Live OpenAI and Gazebo

This is the primary usage path. OpenAI generates the streamed assistant replies through the Responses API; the local EmotionBot engine appraises the conversation and maps its emotional state to safe Gazebo posture movement. The terminal is only the chat interface—it does not mean the reply is offline.

Use three terminals. An optional fourth terminal provides manual posture control.

### Terminal 1: launch the complete system

```bash
cd /home/dimitarbez/Dev/ROS
make -C lite3-noetic start
make -C lite3-noetic run-emotion-sim-openai
```

Keep this terminal attached. The target starts the OpenAI sidecar, ROS master, Gazebo GUI, Lite3 model, all 13 simulation controllers, chat adapter, emotion adapter, expression mapper, and safety bridge. Wait for the robot to stand. The launch log must contain `Chat adapter ready (backend=openai)`.

The world opens with a close orbit camera so leg alignment and body language are visible. When changing directly between non-neutral emotions, expect a deliberate 0.60-second reset beat: the previous gesture returns to exact neutral, holds briefly, then the new entrance begins from a fresh stance.

### Terminal 2: check readiness

Inspect the current safety status:

```bash
cd /home/dimitarbez/Dev/ROS
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash &&
   source /workspaces/lite3-noetic/ws/Lite3_VMC/devel/setup.bash &&
   rostopic echo -n 1 /emotion_bot/status'
```

Wait for these values:

```text
"sim_ready": true
"health_ok": true
"stance_prepared": true
"motion_enabled": true
```

Confirm that the OpenAI bridge is live and received the environment variable:

```bash
curl -fsS http://127.0.0.1:8765/health
```

The response must contain `"api_key_present": true` and `"test_mode": false`. This health check does not print the key.

The integrated launch enables bounded expression movement automatically after readiness, health, and stance checks. If motion was manually disabled or a recovered fault revoked permission, re-enable it with:

```bash
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash &&
   source /workspaces/lite3-noetic/ws/Lite3_VMC/devel/setup.bash &&
   rosservice call /emotion_bot/set_motion_enabled "data: true"'
```

If the service reports that the expression stance is still settling, wait a few seconds and repeat the command. Do not bypass the readiness or health gate.

### Terminal 3: talk to EmotionBot

```bash
cd /home/dimitarbez/Dev/ROS
make -C lite3-noetic run-emotion-chat
```

Enter ordinary messages at the `you>` prompt. The client streams the OpenAI response and then prints the matching emotion and both backend roles. A successful live turn ends with `chat_backend=openai`; `emotion_backend=deterministic` refers only to the local emotional appraisal engine. Example inputs:

```text
I have fantastic news! I'm incredibly happy!
This situation makes me furious.
I'm frightened and don't know what to do.
I love you, robot.
```

The deterministic backend's acceptance sequence is:

```text
I am extremely happy and excited that this finally works!
I am frightened and uncertain about what will happen.
This is making me very angry.
```

The printed states must be `joy`, `fear`, and `anger` in that order. Repeating a sentence in the same category updates its affect while the current idle animation continues; it does not freeze or restart the whole loop.

For repeatable movement checks, use exact deterministic events:

```text
event:joy
event:sadness
event:anger
event:fear
event:surprise
event:disgust
event:curiosity
event:affection
event:neutral
```

Use `:quit` to close only the chat client. A newer message cancels an unfinished response from the previous turn.

If an OpenAI request fails, the client prints an explicit warning before using the configured offline fallback for that turn. It will then report `chat_backend=deterministic`, so live and fallback replies are never ambiguous.

### Optional terminal 4: manual posture

```bash
cd /home/dimitarbez/Dev/ROS
make -C lite3-noetic run-emotion-keyboard
```

The reliable posture controls are:

| Keys | Movement |
| --- | --- |
| `w` / `s` | Pitch |
| `a` / `d` | Roll |
| `q` / `e` | Body height |

Manual input briefly has priority over emotional input. The keyboard target keeps its posture-oriented bindings, while normal emotional profiles remain planted.

## Monitor the running system

Continuously inspect safety decisions:

```bash
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash &&
   rostopic echo /emotion_bot/status'
```

Inspect the current emotion state:

```bash
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash &&
   rostopic echo /emotion_bot/state'
```

Inspect one joint-state sample:

```bash
make -C lite3-noetic joint-states
```

The normal chat simulation uses centered expressions. Joy performs one full bounded hop and then loops enlarged height/roll/pitch motion; the other emotions likewise request no planar travel. The Gazebo model remains dynamic rather than fixed. If contact motion carries it 0.09 m from its enable-time center, the bridge holds all four feet in stance and smoothly applies bounded simulator-only corrections until it is inside 0.035 m; 0.20 m is the hard stop. This avoids the unstable Lite3 walk gait and never leaves the model fixed or permanently constrained. The explicit `run-emotion-sim-locomotion` target remains available for supervised gait development. A health failure, stale command, explicit disable, or shutdown forces zero and revokes motion permission.

For a repeatable visual review of every entrance and looping idle animation:

```bash
make -C lite3-noetic emotion-animation-review
```

This launches the GUI stack, reviews all nine planted emotions in simulated time, and holds each through at least two idle cycles. It disables motion and tears down the launch on exit.

## Stop safely

Disable expression movement before ending the session:

```bash
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash &&
   rosservice call /emotion_bot/set_motion_enabled "data: false"'
```

Press `Ctrl+C` in the chat, keyboard, and simulation terminals. The live launch removes its OpenAI sidecar automatically. If a terminal was interrupted unexpectedly, clean up the sidecar with:

```bash
make -C lite3-noetic stop-openai-bridge
```

Stop the development container only when finished with all work:

```bash
make -C lite3-noetic stop
```

## Separately authorized physical-expression session

These commands are not part of the Gazebo workflow. Use them only in a task
that explicitly authorizes current physical operation and after completing the
manufacturer restraint, distance, STOP, state, network, and battery checks in
[the hardware record](HARDWARE_APP_CONTROL.md). The robot must begin sitting in
basic state `1`.

After the one-time install, use two attached terminals:

```bash
# Terminal 1: official continuous MotionSDK owner and brain
make -C lite3-noetic run-emotion-hardware

# Terminal 2: existing chat window
make -C lite3-noetic run-emotion-chat
```

Inspect the runner from an optional third terminal without commanding it:

```bash
make -C lite3-noetic watch-emotion-hardware-status
```

Each JSON sample correlates the chat `session_id`, `turn_id`, transport and
state sequences, valence/arousal, engine-requested category, resolved and active
physical profiles, newest pending request, phase/cycle, fallback reason, link
age, ownership, contact estimate, fault, and release state. A neutral fallback
does not rewrite the requested EmotionEngine category.

The default allowlist is only `neutral`. During separately authorized staged
commissioning, add only already approved categories and choose the requested
scale explicitly, for example
`HARDWARE_COMMISSIONED_EMOTIONS=neutral,joy HARDWARE_EXPRESSION_SCALE=1.0`.
Do not enable a category merely because its offline tests pass. Affection,
Curiosity, Disgust, and Surprise always resolve to the accepted Neutral breath
until their own final physical reactions are implemented and accepted.

Press `Ctrl+C` in terminal 1 to enter the normal SDK release path. A stale chat
link first returns to neutral for 1.5 seconds and holds for 0.35 seconds, then
releases; a safety fault releases immediately. Recovery always requires a new
operator-started session. The Retroid-compatible height path is diagnostic-only:

```bash
make -C lite3-noetic run-emotion-hardware-retroid-diagnostic
```

That target refuses to start while an official or legacy direct-joint ownership
marker exists.

## Automated checks

Run only the complete headless Gazebo integration test:

```bash
make -C lite3-noetic emotion-gazebo-test
make -C lite3-noetic emotion-animation-gazebo-test
```

Run the full offline verification gate:

```bash
make -C lite3-noetic verify-emotion
make -C lite3-noetic verify-hardware-offline
```

The verification suite does not use the API key, network, microphone, GUI, physical robot, or paid API calls.

To make one real OpenAI request without launching Gazebo, run:

```bash
make -C lite3-noetic emotion-openai-live-smoke
```

This is a connection smoke test only. It reports metadata rather than generated response text.

## Offline diagnostics only

The following targets deliberately do not use OpenAI. Use them only when isolating API, display, or networking problems:

```bash
make -C lite3-noetic run-emotion-sim
make -C lite3-noetic run-emotion-sim-headless
```

For operational details see [Workflow](WORKFLOW.md). For failures see [Troubleshooting](TROUBLESHOOTING.md).
