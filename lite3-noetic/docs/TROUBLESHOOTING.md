# Troubleshooting

## Missing emotion-bot checkout

`make start` requires `/home/dimitarbez/Dev/ROS/emotion-bot/.git`. For a new clean setup:

```bash
git clone https://github.com/dimitarbez/emotion-bot.git emotion-bot
git -C emotion-bot checkout 20c0c1361434bcf9ebaec4e8a5c9385e61c9c3e2
```

Do not run checkout or pull over existing local work. Inspect `git -C emotion-bot status` first.

## Container has stale mounts or runtime settings

An older `lite3-noetic-dev` may lack the read-only `/workspaces/emotion-bot` mount or Docker’s init/reaper. `make start` detects both. Recreate only the container:

```bash
make -C lite3-noetic restart
```

The workspace and sibling checkout are bind mounts and remain on the host.

## Gazebo GUI does not display

Check WSLg variables on the host:

```bash
echo "$DISPLAY"
echo "$WAYLAND_DISPLAY"
test -d /mnt/wslg && echo WSLg-present
```

Recreate the container if its display mounts predate the wrapper. Validate the system independently of display infrastructure with:

```bash
make -C lite3-noetic run-emotion-sim-headless
```

## Motion enable is rejected

The integrated launch sets `require_sim_ready=true`. Wait until the Lite3 controller has completed its internal initialization. Inspect:

```bash
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash && rostopic echo -n 1 /emotion_bot/sim_controller_ready'
```

Do not bypass this guard. If it never becomes true, inspect controller state and the launch log.

## Emotion changes but the robot does not move

This is expected while motion is disabled. Check `/emotion_bot/status`, then explicitly enable motion through `/emotion_bot/set_motion_enabled`. A new semantic state starts or blends the matching pattern only when the adapter publishes a greater `sequence`; send a new input after enabling if the previous expression has already returned to neutral. A same-category state updates affect intensity while continuing its existing loop; changing categories performs a short neutral reset before the next entrance.

The simulator does not consume `/cmd_vel` in this build. Inspect `/emotion_bot/safe_cmd` for the clamped intention and `/emotion_bot/joy_out` for the actual arbitrated simulator input.

## Robot stops immediately

Inputs are intentionally short-lived:

- mapper state timeout: 1.0 s without adapter heartbeat
- expression command timeout: 0.5 s
- manual Joy timeout: 0.5 s
- expression segment durations: 0.5–4.0 s by default, followed by a 0.70 s neutral return

Check `stale`, `selected_source`, `sim_ready`, and `last_safety_action` on `/emotion_bot/status`. Do not lengthen timeouts merely to mask a dead publisher.

## Keyboard appears ignored

Use `make run-emotion-keyboard` with the integrated stack; it remaps the keyboard node to `/emotion_bot/manual_joy`. Motion must be enabled. Manual activity then has priority over emotion output. Do not run the unremapped keyboard target against the integrated launch because `/joy` is not its selected input.

## Controller or joint-state failure

Confirm `/lite3_gazebo/joint_states` and all 13 running controllers:

```bash
make -C lite3-noetic joint-states
docker exec lite3-noetic-dev bash -lc \
  'source /opt/ros/noetic/setup.bash && rosservice call /lite3_gazebo/controller_manager/list_controllers'
```

If controller types are missing, rebuild the image, recreate the container, and bootstrap. The integrated launch uses a one-shot controller loader and does not depend on stdin.

## Original model spawn cleans up immediately

The upstream spawner reads stdin. EOF can make it proceed directly to controller teardown and model deletion. Run `make run-spawn` in a real interactive terminal and press Enter only once until your baseline test is over. This limitation does not apply to `run-emotion-sim`.

## Live OpenAI launch says the key is absent

The live target first reads `OPENAI_API_KEY` from the persistent, Git-ignored `lite3-noetic/ws/Lite3_VMC/.env` file. Populate it without putting the value on a command line:

```dotenv
OPENAI_API_KEY=replace-with-your-key
```

Protect it and retry:

```bash
chmod 600 lite3-noetic/ws/Lite3_VMC/.env
make -C lite3-noetic run-emotion-sim-openai
```

An already-exported environment value is also supported. Do not add the value to YAML, launch files, Dockerfiles, shell scripts, or a committed `.env`. The sidecar receives only the named environment variable and is removed on shutdown.

## OpenAI chat falls back to offline

An `offline_fallback` event means the bounded live request failed after its retry. Robot state remains safe: the accepted user event can still update EmotionBot, and the deterministic response completes the current turn. Check that the sidecar is healthy:

```bash
curl -fsS http://127.0.0.1:8765/health
```

The health payload reports only key presence and test mode, never the key. Rebuild after changing the sidecar:

```bash
make -C lite3-noetic build
```

Run `make -C lite3-noetic emotion-openai-live-smoke` to isolate API access from Gazebo. Output contains only model, event count, response length, and elapsed time. Provider errors are deliberately generic in ROS and logs.

## Streaming response is superseded

This is intentional when a newer message arrives before the current turn completes. The coordinator publishes `cancelled` for the old turn, closes its stream when possible, and accepts the new turn immediately. The adapter rejects any late completion whose turn ID/index is no longer active. Increase neither retries nor timeouts to hide normal cancellation.

## Offline backend or dependency errors

The default chat and appraisal backends are `deterministic`; they do not require `OPENAI_API_KEY`, Transformers, model downloads, a microphone, or a GUI. The Noetic image deliberately does not install EmotionBot's complete newer requirements or the current OpenAI SDK into Python 3.8. The live OpenAI dependency is isolated in `lite3-openai-runtime:local` on Python 3.12.

If imports fail, confirm the read-only checkout is mounted and pinned:

```bash
docker exec lite3-noetic-dev git -C /workspaces/emotion-bot rev-parse HEAD
```

## ROS networking or Docker permission errors

Use `make start` rather than a custom `docker run`; it configures loopback ROS networking. For Docker socket permission errors, restart the WSL shell and confirm `docker info` works for the current user.

## Test failure and captured logs

`make emotion-gazebo-test` prints its captured log path. Inspect the newest log with:

```bash
docker exec lite3-noetic-dev bash -lc \
  'tail -200 "$(ls -1t /tmp/emotion_bot_gazebo_e2e_*.log | head -1)"'
```

The harness always signals its process group and escalates cleanup if needed. Before rerunning after an interrupted external terminal, check that no `roslaunch`, `gzserver`, or simulation controller remains.

## Rebuild the environment

```bash
make -C lite3-noetic stop
make -C lite3-noetic build
make -C lite3-noetic start
make -C lite3-noetic bootstrap
```

This does not delete the host workspace or emotion-bot checkout.
