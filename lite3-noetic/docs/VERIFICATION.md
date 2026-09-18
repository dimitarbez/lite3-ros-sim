# Verification record

Implementation checkpoint for Lite3_VMC upstream base `724aa54dee0cc374bd5417c86d649fb08d4d309a` and EmotionBot checkout `20c0c1361434bcf9ebaec4e8a5c9385e61c9c3e2`.

## Baseline

- Started the unmodified simulator with the original four attached targets.
- Pressed Enter once in the spawn terminal and observed all 13 controllers running.
- Received 12 joints on `/lite3_gazebo/joint_states`.
- Confirmed the keyboard publishes `sensor_msgs/Joy` on `/joy` and moves the Gazebo model.
- Source inspection confirmed the simulation executable subscribes Joy and does not instantiate its `/cmd_vel` receiver. Only the real executable starts the motion SDK receive path.

## Integrated implementation

- The read-only sibling EmotionBot checkout supplies the reusable `EmotionEngine`; no CLI/stdin automation or copied emotional core is used.
- A separate coordinator provides deterministic or OpenAI chat, immediate accepted events, incremental deltas, bounded context/requests, retries, deterministic fallback, cancellation, and turn ordering.
- The current official OpenAI SDK runs in an isolated Python 3.12 loopback sidecar because its dependency floor is incompatible with Noetic Python 3.8. The ROS image remains headless and compatible.
- State contract 1.1 includes turn correlation. Only accepted user and current completed assistant events update emotional state.
- The fixed-rate expression controller filters affect, blends and rate-limits motion, and resets every non-neutral category change through a 0.25-second exact-neutral return plus a 0.35-second canonical hold before starting the new entrance. A downstream bridge clamps again, arbitrates bounded manual input, applies watchdogs, and defaults disabled/zero.
- The integrated launch avoids the stdin spawner, loads all controllers through services, waits for readiness, and keeps the safety bridge required.

## Latest verified results

- Static shell and Python checks: passed in both runtime images.
- Clean Release `catkin_make`: passed; only pre-existing upstream Gazebo deprecation/CMake warnings.
- Unit suites: 58 Python tests and 5 controller-side C++ trajectory tests passed.
- Offline OpenAI process-boundary stream: 1 passed without key/network.
- ROS integration: 1 passed, 0 failures/errors, including generation-aware cancel-before-start and queued stance transitions.
- Both isolated headless Gazebo sweeps passed with 13 controllers and 12 joint states.
- Every chat emotion is centered. In the latest two-loop animation sweep, maximum per-profile contact-model drift remained below 0.025 m; the guard rejects more than 0.10 m.
- Anger emitted three full stomp occurrences and completed three recoveries across its entrance and two-loop hold, with the adopted 2.25-2.65-second recurring cadence. Joy and surprise also executed their controller-side hops in stance.
- Both Gazebo sweeps enforced a 0.30-rad anti-splay limit on all four HipX joints. A separate 15-second live anger sample after eight chat-driven switches peaked at 0.228 rad, versus the previous hard-limit drift near 0.523 rad. The correction is a simulation-only 40 Nm/rad abduction centering term; physical-robot control is unchanged. Mirrored stomp, recovery, height, tilt, finite-value, and containment checks passed.
- Every direct non-neutral profile change produced at least five consecutive exact-zero mapper samples before the new entrance. Disabled/final stop speeds were 0.0250/0.0021 m/s in the animation run. Watchdog status reached exact zero, manual priority/source were observed, and health permission stayed enabled throughout commanded phases.
- Teardown contained no configured controller/process failure markers.
- No `example_lite3_real`, `message_transformer`, motion-host UDP port, or hardware target address was used. The only inspected UDP use was benign loopback ROS/Gazebo traffic.
- WSLg GUI launch has been checked with both `gzserver` and `gzclient`, using the close orbit camera in `earth.world`. Repeated chat-driven joy/anger/fear/surprise/sadness/disgust switches were captured and inspected after the anti-splay change; the robot stayed upright, returned through the fresh stance, and kept its feet separated.
- The exact natural-language sequence `I am extremely happy and excited that this finally works!`, `I am frightened and uncertain about what will happen.`, and `This is making me very angry.` produced `joy`, `fear`, and `anger` through `run-emotion-chat`. The live controller logged the hop and stomp, a repeated anger turn started another stomp without freezing, and final status was healthy/non-stale at 0.0058 m from home.
- A real `gpt-5-mini` Responses stream passed through the disposable sidecar: 14 delta events, 47 response characters, 2.999 seconds. Output was metadata-only and the key was not persisted or printed.

The repeatable offline authority is:

```bash
make -C lite3-noetic verify-emotion
```

The intentionally separate live check is `make -C lite3-noetic emotion-openai-live-smoke` with `OPENAI_API_KEY` in the current shell. Gazebo measurements can vary slightly while remaining inside the harness thresholds.

## Cartoon-expression Gazebo sweep

The current simulation-only sweep is run from the repository root with:

```bash
make -C lite3-noetic emotion-clean-build
make -C lite3-noetic emotion-static-checks
make -C lite3-noetic emotion-unit-tests
make -C lite3-noetic emotion-ros-tests
make -C lite3-noetic emotion-gazebo-test
make -C lite3-noetic emotion-animation-gazebo-test
```

`emotion-gazebo-test` starts the complete headless stack with `allow_locomotion:=false` so its centered-profile assertions remain deterministic. It enables motion only after the prepared stance, injects `event:<emotion>` for every profile, measures torso pose and all 12 joints through Gazebo for both the entrance and idle phases, then checks rapid changes, disabled actuation, command watchdog expiry, mapper loss, adapter loss, manual priority, stop, controllers, joint-state publication, and the no-hardware boundary. A normal GUI launch additionally enables the 0.09 m/0.035 m simulator-only auto-recenter with a 0.20 m hard boundary.

`emotion-animation-gazebo-test` holds every emotion through two idle cycles. It additionally checks that every profile remains within a 0.10 m planted envelope, joints keep a 0.05 rad URDF margin, all HipX joints remain inside 0.30 rad, front hips remain mirrored during stomps, recovery returns near the prior stance, every category change contains an exact-neutral hold, and no NaN/fall/controller error or stale residual motion occurs. The latest measured maximum idle joint excursion for neutral/joy/sadness/anger/fear/surprise/disgust/curiosity/affection was respectively 0.1081/0.2378/0.1644/0.4017/0.1884/0.2932/0.2525/0.1026/0.1104 rad.

| Emotion | Gazebo-observed designed result |
| --- | --- |
| neutral | Soft 1.375-second breathing and alternating tilt while state remains fresh; exact zero after staleness. |
| joy | One full centered hop, large high/low entrance rocks, then a 1.40-second four-pose bounce loop. |
| sadness | Deep lowered bow with a slow 1.875-second low sway. |
| anger | Forceful symmetric multi-impact stomp, tense low rocking, and a stomp on every 2.4375-second idle loop. |
| fear | Sharp recoil into a full crouch and a fast 0.45-second side-to-side tremble. |
| surprise | Full centered hop into a tall pitched-back alert stance and a 1.60-second attentive loop. |
| disgust | Strong one-sided down-and-away recoil alternating between full and partial avoidance. |
| curiosity | Broad alternating head/body tilts on a 1.20-second loop. |
| affection | Broad, slow warm sway on a 2.75-second loop. |

The planted sweep rejects a profile unless it produces measurable Gazebo torso or joint movement in both its entrance and idle phases, maintains a standing torso, keeps x/y/yaw at zero, stays within the posture command and no-drift envelopes, and settles to zero on disable/shutdown. The normal runtime keeps the dynamic model centered with bounded simulator-only recovery rather than the unstable Lite3 walk gait.
