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

## Split-host read-only hardware deployment — 2026-09-19

The fail-closed hardware working tree based on wrapper
`6de76b66b4209e1517a5b2e9e6284b1505700f8c`, Lite3_VMC
`fbe99b96395ca699bb2fbb121516b0830e08032b`, and EmotionBot
`7ae69828038c2f3563fc00d17fbfb2e3927abf57` was deployed read-only to the
connected Lite3. The changes were uncommitted at verification time.

- Existing SSH public-key authentication was installed on the motion and
  perception hosts; no credential was printed, committed, or stored in a script.
- `transfer.service`, `/joint_states`, `/imu/data`, and `/simple_cmd` were already
  live on the perception computer. No vendor workspace, service, `jy_exe`, or
  `network.toml` file was changed.
- The ROS-less motion-host observer ran with only `CAP_NET_RAW`; its authenticated
  relay reported fresh Retroid traffic, zero axes, and STOP false. The perception
  telemetry service separately held only `CAP_NET_RAW`; the ROS core was
  unprivileged.
- Live `0x0901` decoded state `1` (sitting), zero error flags, bounded attitude,
  and deployed percentage-form battery values. `0x0906` stayed incompatible and
  all high-rate direct-action gates stayed false.
- `make -C lite3-noetic run-emotion-hardware` opened the loopback tunnel and made
  the robot AI link fresh. `run-emotion-chat` produced a live `gpt-5-mini`
  Responses reply and the correlated `curiosity` state arrived on the perception
  computer.
- Throughout the run, `transmit_enabled=false`,
  `dynamic_actions_enabled=false`, supervisor state `DISARMED`, posture and
  dynamic arming false, robot state `1`, and no command was sent to UDP `43893`.
- Ctrl-C removed the OpenAI sidecar and SSH tunnel. Within the stale timeout the
  robot reported `link_fresh=false` while remaining `DISARMED`; all three passive
  services and vendor `transfer.service` remained active.

This verifies installation, passive telemetry, STOP observation, tunneled affect,
OpenAI chat, and fail-closed teardown only. It does not commission posture,
MotionSDK ownership, STOP preemption, hop, stomp, or any physical movement.

## Split-host height-only commissioning — 2026-09-19

With explicit operator authorization, the robot standing on a clear level floor,
and the Retroid STOP control ready, three posture-only joy trials were run through
the split-host mapper, supervisor, rate limiter, and vendor `/simple_cmd` path.
Dynamic actions, locomotion, roll, pitch, yaw, direct joints, and torque output
remained disabled.

The attempted height limits and rate limits were `+1000` at `500` units/s,
`+4000` at `1000` units/s, and `-20000` at `4000` units/s. The final value used
the sign observed from a passive Retroid right-stick-up capture (approximately
`-27606`) while capping it at the vendor's documented `20000` magnitude. The
operator reported **zero physical movement in all three trials**.

Trial 2 proved a nonzero `+4000` ROS `/simple_cmd` and exact return to zero.
Trial 3 retained healthy telemetry, joint/IMU feedback, centered Retroid axes,
state `6`, and no STOP or supervisor fault, but its filtered motion-host capture
did not prove delivery of the nonzero packet to `jy_exe`. This is a transport,
mode, ownership, or arbitration investigation result—not justification to raise
the amplitude.

After the final trial, `/simple_cmd` was zero, the supervisor was `DISARMED`, the
temporary `/dev/shm` commissioning configuration was removed, and the persistent
service was verified with `transmit_enabled=false` and
`dynamic_actions_enabled=false`. The exact approach, evidence boundary, and
required packet-comparison work are recorded in
[`HARDWARE_APP_CONTROL.md`](HARDWARE_APP_CONTROL.md#split-host-posture-commissioning-attempt--2026-09-19).
After the local chat and brain link were closed, a fresh read showed
`link_fresh=false`, supervisor `DISARMED`, and `robot_basic_state = 1` (sitting).
The test did not establish what caused that later state change.

## Instrumented height-command rerun — 2026-09-19

The operator then requested a standing-state joy rerun with movement enabled.
Fresh telemetry showed that the robot had already reached state `6` (standing),
so no redundant Stand/Sit toggle was sent. The temporary commissioning profile
enabled only the joy height channel, with a `+4000` limit and `1000` units/s rate;
dynamic actions, locomotion, roll, pitch, yaw, direct joints, and torque output
remained disabled.

The first instrumented rerun exposed a bridge defect rather than a missing
emotion command. Joy followed by neutral produced 58 height messages on ROS
`/simple_cmd`, but the passive perception-host wire observer saw only 16 height
datagrams and a maximum inter-packet gap of approximately 1.26 seconds. Source
inspection showed that the vendor `/ros2qnx` subscriber has queue depth 1 while
the bridge was publishing height plus three zero-valued, uncommissioned axes
back-to-back. Those unnecessary messages could evict the height command before
the vendor bridge transmitted it.

The posture bridge was changed to serialize one command per timer tick and to
emit only commissioned axes. `make -C lite3-noetic verify-hardware-offline` then
passed its static checks, clean Release build, launch enumeration, and all 14
protocol/safety tests before the corrected bridge was deployed. In the post-fix
rerun:

- Joy and the following neutral state both reached the hardware mapper.
- `/simple_cmd` reached height `+4000` and returned to exact zero.
- The wire observer saw 39 height datagrams and 10 heartbeats, with no
  uncommissioned roll, pitch, or yaw datagrams. The median nonzero-height interval
  was approximately 54 ms and the maximum was approximately 206 ms.
- It observed the final exact-zero height datagram (`0x21010102`, value `0`) from
  the perception host to the configured motion-host endpoint.
- The robot remained in state `6` with zero error flags. Maximum measured joint
  span was approximately `0.00099` rad and IMU changes were similarly negligible;
  no meaningful physical movement was measured or visually confirmed.

This proves the EmotionBot state, mapper, supervisor, `/simple_cmd`, vendor packet
format, and perception-host network emission through exact neutral recovery. It
does **not** prove that motion-host `jy_exe` accepted or applied the command. The
remaining investigation boundary is motion-host-side receive observation plus
the full app's Auto-mode/controller-ownership behavior. A larger posture
amplitude is not justified until that boundary is resolved.

The temporary configuration and deterministic brain were removed after the
trial. The persistent service was restored fail-closed with
`transmit_enabled=false`, `dynamic_actions_enabled=false`, and supervisor
`DISARMED`; both hardware services were active with `Restart=always`. Final live
telemetry showed the robot still standing in state `6`, battery `37%`, and zero
error flags. The stale brain link after tunnel teardown was expected. No password
or other credential was written into the repository or verification record.

## Auto-mode neutral-breathing trial — 2026-09-19

With Auto mode enabled by the operator, the hardware mapper was extended with a
disabled-by-default height-only analogue of Gazebo's neutral breathing loop. It
uses the same 0.25-second quintic entrance, 1.375-second idle period, and
`+0.65/-0.39` idle endpoint ratios. A new 0.25-second posture-intent watchdog
forces exact zero if the mapper disappears while the AI heartbeat remains live;
the mapper and posture bridge are now required launch nodes.

The physical trial used a temporary `400` height limit and `3000 units/s` rate,
with every other posture axis, locomotion, and dynamic action disabled. Preflight
showed state `6`, zero errors, fresh feedback, centered Retroid input, STOP false,
and no `/cmd_vel` publisher. Offline verification had passed a clean Release
build, launch enumeration, static checks, and all 15 protocol/safety tests.

During the 5.5-second neutral sequence, `/simple_cmd` and the perception wire
observer both ranged from `-156` to `+260`. Wire height cadence had approximately
51 ms median and 104 ms maximum intervals. All captured robot states remained
`6` with zero errors, while the maximum joint-position span was only
`0.00084 rad`; no meaningful physical breathing movement was measured. Auto mode
therefore did not make this small height sequence observable.

The nonzero motion-host capture was inconclusive, so `jy_exe` acceptance remains
unproven. A separate exact-neutral observation did prove value `0` on
`/simple_cmd`, on the perception wire observer, and arriving as a correctly
encoded 12-byte height datagram on motion-host `eth1`. The run ended `DISARMED`
with persistent transmission, dynamic actions, and neutral breathing disabled.
The robot remained standing in state `6` with zero errors. Battery fell from
`34%` to `26%` during the trial and read `23%` after teardown, so no further
nonzero trial was performed; the next test requires a charged replacement and
reliable nonzero motion-host ingress capture.

## Retroid-compatible direct neutral-height trial — 2026-09-19

A later charged-battery trial bypassed the unproven `/simple_cmd` acceptance
boundary and sent bounded high-level UDP from Windows
`192.168.2.28:43897` directly to `192.168.2.1:43893`. It reproduced the captured
full-app session sequence: four heartbeats at 2 Hz, Move, 2.0 seconds, Pose,
1.5 seconds, then 50 Hz pairs of yaw code `0x21010135` value `32768` followed by
height code `0x21010102`.

The 16.2-second waveform used a 4-second entrance, 10-second period,
`4000 units/s` rate limit, and height bounds `-3900..+10000`. The positive bound
is 50% of the documented `20000` magnitude; the negative bound preserves the
neutral profile's `-0.39` ratio. Evidence recorded:

- 810 height samples, with observed minimum `-3900` and maximum `+10000`;
- maximum 12-joint position span `0.2544403 rad`;
- maximum IMU orientation-component span `0.00856887`;
- basic state `6` and error flags `0` throughout;
- battery 62% at completion and 61% in a later fresh check; and
- five final yaw-`0`/height-`0` pairs at 20 ms spacing on unconditional teardown.

The separate Retroid reference was 387 packets over 19 seconds, with captured
height range `-29332..267` and yaw companion mainly `32768`. Those app values
were evidence for framing only; the custom test did not replay the app's
full-range height. The operator chose a 25% abort floor for this session despite
the Pro manual's 75% start recommendation. This does not revise the normal
battery guidance.

The whole-run span was later separated from the mode transition. A complete
post-handshake waveform window measured only `0.0011444 rad`, so the first
`+10000..-3900` periodic range was ineffective. A fixed post-handshake
`-10000` trial then measured `0.2484894 rad` maximum joint span and `0.0078653`
maximum IMU orientation-component span during the active height interval, with
state `6`, zero errors, and five acknowledged zero pairs on exit. This proves
bounded height actuation at the 50% negative limit.

The maintained launcher now starts an exclusive neutral-only host bridge by
default. It uses the captured handshake, a 250 ms Windows relay watchdog,
per-topic freshness, neutral emotion, a two-second stable dwell, and a 25%
operator-selected runtime floor. The final `0..-10000` periodic profile was not
physically re-verified because telemetry crossed below that floor; output paused
as designed. Persistent robot-side services remain fail-closed, and the packet
capture is not committed.

## Official MotionSDK continuous neutral breathing — 2026-09-19/20

An explicitly authorized restrained hardware run commissioned the official
aarch64 MotionSDK path from sitting state `1`. It used
`Sender::RobotStateInit()`, the vendor `PreStandUp` and `StandUp` trajectories,
the symmetric `0/-42/+78 degree` HipX/HipY/knee target, and a one-second stable
neutral hold. It did not acquire SDK control from a vendor-controlled standing
pose and did not bind the `qnx2ros` UDP port.

The final profile was a continuous five-second quintic cycle at the full
commissioned amplitude: HipX fixed, HipY `-0.0120 rad`, and knee `+0.0240 rad`,
with exact neutral and zero velocity at each loop seam. Nineteen full cycles
(95 seconds) completed. Maximum observed family spans were `0.0011444 rad`
HipX, `0.0124359 rad` HipY, and `0.0234222 rad` knee.

The reviewed passive `0x0906` stream produced a maximum `148.442 ms` feedback
age and `148.665 ms` consecutive update gap. Five gaps crossed 100 ms; each froze
trajectory time and held the last validated positions with zero velocity/torque,
then recovered after 20 fresh frames. No gap reached the 250 ms hard-release
threshold.

During cycle 20, the independent sequence-locked `0x0901` feed reported basic
state `8`, error flags `0`, at `0.501792 ms` age. The new state interlock stopped
the trajectory and executed the no-retry MotionSDK return-to-robot sequence. It
did not send STOP. Final telemetry showed state `1`, gait/motion `0/0`, battery
`41%`, and error flags `0`; the runner had exited and the SDK ownership marker
was absent.

Supporting verification passed 27 offline hardware tests, the aarch64 Release
build, and the standalone C++ state-safety test. Earlier logs also prove two
fail-closed stand aborts under the former 100 ms hard freshness rule, followed
by a 12-cycle run that exercised the gap pause/recovery behavior. The complete
implementation and attempt history are recorded in
`docs/HARDWARE_APP_CONTROL.md`.

### Animal-like Gazebo translation — 2026-09-20

The physical neutral was updated from a one-sided five-second compression to a
3.25-second, three-segment quintic inhale/exhale modeled on Gazebo's alternating
height/roll/pitch neutral. Hardware commands base HipY from `-0.008 rad`
extension through `+0.020 rad` compression, knee at twice that scalar, roll bias
at `+/-0.004 rad`, and pitch bias at `+/-0.003 rad`. HipX and yaw remain exactly
zero; Gazebo neutral also has no yaw. Combined per-leg compression is bounded to
`-0.015..+0.027 rad` (maximum knee offset `0.054 rad`).

The profile bounds/endpoints test passed both in the Noetic environment and on
the aarch64 robot host. The full offline gate passed a clean Release build, all
27 Python tests, packet verification, lease/release/crash-watchdog integration,
and launch enumeration.

One restrained live attempt began from state `1`, battery `37%`, errors `0`,
with no existing owner. Initialization, both vendor stand phases, and the stable
neutral hold completed. At the operator-accepted handoff checkpoint, 27 full
cycles (87.75 seconds) had completed with no tracking or robot-state fault.
Maximum measured spans were `0.0019074 rad` HipX, `0.0481415 rad` HipY, and
`0.0708771 rad` knee. Maximum feedback age/update gap was
`150.050/151.049 ms`; four pauses all recovered after 20 fresh frames, with a
maximum pause of `90.9427 ms`. The continuous runner and ownership marker were
still active at handoff, with the independent state-8 and dead-stream release
paths armed.

## Nine-category physical expression runtime — offline verification 2026-09-20

This software-only change was verified in the wrapper working tree based on
`a3fee6db1ed5e133db1bc83a86cead4c7d7f37a9`, with the unchanged active
Lite3_VMC checkout at `c405287e1ac19bbbe9deedbf3493a0bb94cfab09` and
EmotionBot checkout at `7ae69828038c2f3563fc00d17fbfb2e3927abf57`. The
changes were uncommitted at verification time. No robot host was contacted and
no physical command was sent.

The final offline authority completed successfully:

```bash
make -C lite3-noetic verify-hardware-offline
```

It passed shell/Python static checks, a clean Release build of the hardware
catkin workspace, all 29 Python protocol/mapping/shared-memory tests, exact
packet checks, the loopback fail-closed acquisition/lease/graceful-release and
crash-watchdog integration test, and official/diagnostic launch enumeration.
The separate ROS-independent CMake suite passed all three targets:

- profile/safety checks for nine unique planted profiles, finite samples,
  neutral seams, endpoint derivatives, joint bounds and motion limits;
- transition behavior for category changes, rapid retargeting, same-category
  affect updates, neutral-only commissioning and stale-link release; and
- a fake-Sender session with one acquisition/release, uninterrupted simulated
  1 kHz ownership, exact reset timing and inspectable status output.

The existing planted Gazebo end-to-end sweep also passed after the wrapper
changes. It exercised all nine categories, rapid/repeated changes, disabled and
watchdog stops, manual arbitration, 13 controllers, 12 joint states and the
no-hardware boundary. Its maximum per-profile planar displacement was
`0.0362 m`, below the `0.10 m` guard; teardown reported no controller failure
markers. This was regression evidence only: no Lite3_VMC simulation source or
submodule commit changed in this implementation.

Two checks are deliberately not claimed. A real build of
`motion_sdk_expression_runner` against the official aarch64 MotionSDK was not
available because the development shell could not resolve `github.com`, no
local SDK checkout or aarch64 cross-compiler was present, and the disconnected
robot hosts were intentionally not contacted. The edited runner did pass a
host syntax build against interface-compatible stub headers, but that is not a
substitute for the required official aarch64 Release build. The longer
two-idle-cycle Gazebo animation sweep was tried twice from clean process state;
both attempts stopped during neutral idle because simulated time stalled. The
normal all-category sweep above passed, so this is recorded as a Gazebo-clock
runtime failure rather than animation evidence.

The broader unchanged simulation gate was not fully green: its build, 60
Python tests, five controller-side C++ tests and offline OpenAI boundary test
passed, but `emotion_bot_core.test` twice observed `bounded.linear.z` above its
`0.035` assertion (`0.047560...` and `0.037623...`). No simulator source was
altered to mask that independent timing/bound failure.

Only `neutral` remains commissioned by default. The eight new categories,
physical scale steps, direct transitions, STOP response and chat-driven sweep
still require a separately authorized restrained robot session. Airborne hops
and lifted-foot stomps remain out of scope and unimplemented.

### ARM deployment preflight — 2026-09-20

After explicit authorization to reconnect, the same working tree was staged on
the perception host without starting a motion owner. The retained clean
official MotionSDK checkout was commit
`b30a3ec09619e1dd0f3cfa6c49a50eb59670f141`; its aarch64 library SHA-256 was
`133349ceb6efdae1987f1d7787c7658e7be9cf32467e4ae123ee0d77934f62fb`.
The first real ARM compile found that the official `RobotCmd::joint_cmd` is a C
array, unlike the interface stub used by the earlier syntax check. Replacing
the non-portable `.size()` call with an array-size expression fixed the build.

The clean aarch64 Release build then completed and all three standalone suites
passed on the perception host. The installed runner was an ARM aarch64 ELF with
SHA-256 `057babd3738f8ea240a0ccf1bbc5a96bb0999fa4e745a849ffdd2120baffcd97`
and resolved the official aarch64 SDK library from
`/home/ysc/Lite3_MotionSDK/lib`. The local full offline gate also passed again
before deployment.

The unprivileged core was restarted to load the new receiver and read-only
expression-status publisher. Live preflight showed a fresh authenticated
Retroid observer, centered axes, STOP false, errors zero, compatible SDK
layout, no competing sender or ownership marker, and battery `71%`. The robot
reported undocumented basic state `98`, not required sitting state `1`.
Accordingly a preflight-only runner invocation rejected the session before
constructing a `Sender` and printed `official expression runtime requires
initial robot state 1; observed 98`. No robot command or motion was issued.

### Restrained neutral-to-joy test at 25% — 2026-09-20

After the operator returned the robot to state `1` and reconfirmed that the
physical safety setup was ready, one continuous session was run with the
temporary allowlist `neutral,joy` and profile scale `0.25`. Final preflight was
state/gait/motion `1/0/0`, battery `69%`, error flags `0`, roll `0.004 deg`,
pitch `0.312 deg`, compatible SDK layout, fresh authenticated Retroid observer,
centered axes, STOP false, and no existing owner.

`RobotStateInit`, the one-second `PreStandUp`, the 1.5-second `StandUp`, and the
one-second canonical hold all completed. Feedback ages at their phase
boundaries were at most `0.530 ms`. Neutral then completed at least 41 full
profile cycles with continuous ownership and no fault. The deterministic input
`I am thrilled and joyful!` produced emotion-state `1.1` joy with valence
`0.7`, arousal `0.6`, sequence `1`, and turn `legacy-000001`. The runner
transitioned to active joy and completed at least 33 joy cycles before operator
termination; sampled status had no pending target, fault, or feedback pause.

An eight-second joy window captured 762 decoded `0x0906` samples. Maximum
measured spans by joint family were `0.0013733 rad` HipX, `0.0067139 rad` HipY,
and `0.0080109 rad` knee. Maximum measured absolute velocity was
`0.2363205 rad/s`. State remained `1`, errors remained zero, high-rate feedback
remained healthy, roll stayed within `-0.015..+0.107 deg`, and pitch stayed
within `-0.233..-0.095 deg`. The temperature fields were zero throughout, so
they are not treated as measured motor-temperature evidence.

Ctrl-C entered the normal release path. Across the full session the maximum
feedback age/update gap was `150.120/151.024 ms`; all eight pauses recovered
after the required fresh frames and the longest pause was `74.037 ms`. No
robot-safety fault occurred. Post-release state was `1`, battery `66%`, errors
`0`, STOP false, high-rate feedback inactive as expected, no runner process,
and no official or legacy ownership marker. The command returned status 130
because it was operator-terminated after release.

This is partial joy commissioning evidence at `25%`, not approval for the
default allowlist. Visual confirmation of four planted feet and the required
`50%`, `75%`, and `100%` steps remain outstanding, so the checked-in default
continues to be neutral-only.

### Operator-accepted neutral-to-joy test at 100% — 2026-09-20

The operator reported that the 25% joy motion was not visually distinguishable
from breathing and explicitly authorized a second session at `100%`. Fresh
preflight was state/gait/motion `1/0/0`, battery `65%`, errors `0`, roll
`0.043 deg`, pitch `0.259 deg`, compatible SDK layout, fresh authenticated
Retroid observation, centered axes, STOP false, and no owner. The runner again
completed initialization, both vendor stand phases, and the canonical hold,
then completed seven neutral cycles before the joy request.

Joy became active under uninterrupted ownership and reached cycle 18 before
the bounded test ended. An eight-second full-scale joy window captured 760
decoded `0x0906` samples. Maximum measured spans were `0.0018311 rad` HipX,
`0.0418091 rad` HipY, and `0.0580597 rad` knee; maximum absolute joint velocity
was `0.3550339 rad/s`. State stayed `1`, errors stayed zero, high-rate feedback
stayed healthy, roll stayed within `-0.460..+0.458 deg`, and pitch stayed within
`-0.881..-0.017 deg`. The operator then reported that the motion looked good,
providing visual acceptance of the full-scale joy expression.

Ctrl-C again completed the normal release. Maximum feedback age/update gap was
`150.478/145.420 ms`; all three pauses recovered and the longest was
`78.007 ms`. No robot-safety fault occurred. Post-release state was `1`,
battery `64%`, errors `0`, STOP false, high-rate feedback inactive as expected,
with no runner and no ownership marker.

The explicitly requested jump from `25%` to `100%` means the original `50%`
and `75%` commissioning points were not run. The checked-in allowlist therefore
remains neutral-only even though the full-scale joy profile was operator-
accepted. Adding joy to the default requires either completing those two
recorded steps or an explicit decision to waive them.

### Joy readability revision and airborne-hop gate — 2026-09-20

After the accepted first full-scale run, the operator requested a more obvious
joy loop and asked to enable the Gazebo hop. The physical joy idle was revised
to replay its existing planted crouch-rise-neutral hop proxy on every cycle,
then rock through the full already-bounded stage-one endpoints: compression
remains `-0.008..+0.020 rad`, roll remains `+/-0.004 rad`, pitch remains
`+/-0.003 rad`, and HipX/yaw remain zero. Rock duration increased to `0.80 s`
so the larger endpoints continue to satisfy the existing velocity and
acceleration assertions. No joint bound was widened.

`make -C lite3-noetic verify-hardware-offline` passed after the change,
including all three standalone C++ suites, 29 Python tests, a clean Release
catkin build, the packet/lease/crash-watchdog integration test, and launch
enumeration. The real aarch64 MotionSDK build also passed all three suites; the
installed runner SHA-256 was
`76334b507fdd07885c3ca49b2d71d653cfddc3746fad9f7943f74899c19814e4`.

A fresh full-scale `neutral,joy` session started from state/gait/motion
`1/0/0`, battery `61%`, errors `0`, STOP false, centered Retroid axes, and no
owner. It completed the vendor initialization/stand sequence and at least six
revised joy cycles. Sampled status reported active joy, no pending target,
healthy high-rate feedback, and no fault; state remained `1` with errors `0`.
Normal Ctrl-C release completed with one telemetry pause and one recovery,
maximum feedback age/update gap `143.725/144.128 ms`, maximum pause
`72.9974 ms`, no safety fault, and no remaining ownership marker. Post-release
state was `1`, battery `59%`, errors `0`, STOP false, and centered axes.

The session also disproved the current contact gate under official ownership:
all 12 decoded contact channels were exactly zero while the robot was visibly
standing, and `all_contacts_healthy` was false. This differs from an earlier
passive standing capture with negative vertical channels. The checked-in
`contact_feedback_available` flag is therefore now false. The operator stated
that STOP preemption works, but this run did not produce a new measured STOP
latency trace. More importantly, neither the official MotionSDK nor the
continuous runner provides a physical hop primitive, and the simulator's
`0.55 s` metre-based body-height trajectory cannot be copied into raw joint
angles. A true airborne hop remains disabled until a calibrated physical
trajectory and mode-valid flight/landing sensing are implemented and tested.

### Visually rejected planted front-paw experiment — 2026-09-20

The operator clarified that joy should resemble an excited dog's alternating
front-paw stomps rather than a hop. The physical idle loop was changed to
alternate front-left and front-right shoulder emphasis with exact-neutral
beats. At each target, the emphasized front combined compression was
`+0.020 rad`, its same-side rear remained at `+0.014 rad`, all other legs also
remained at or above neutral compression, and HipX/yaw/torque feed-forward
stayed zero. This deliberately produced no foot lift.

The full offline hardware gate passed. The real aarch64 build and all three C++
suites passed; the installed runner SHA-256 was
`135cad0d909ca13e10865bb9a5860538a9069e8087af64f8eb125f9955dc6894`.
A fresh full-scale run began from state/gait/motion `1/0/0`, battery `57%`,
errors `0`, STOP false, centered controls and no owner. The deterministic joy
turn completed at least 24 loops with no sampled runner fault. State remained
`1`, errors remained `0`, roll/pitch were `-0.047/+0.087 deg` in the sampled
window, and high-rate feedback was healthy. Four telemetry pauses all
recovered; maximum age/update gap was `155.202/155.775 ms`, and maximum pause
was `77.9819 ms`. Ctrl-C released cleanly with no safety fault. Post-release
state was `1`, battery `55%`, errors `0`, STOP false, centered controls, and no
owner.

The operator reported that nothing read as joy and that the feet were still
fully planted, so this is a failed visual result rather than accepted
choreography. Read-only follow-up confirmed that the vendor ROS bridge exposes
no foot-contact topic and `/joint_states` contains empty velocity and effort
arrays. Together with the all-zero MotionSDK contact block under SDK ownership,
there is no mode-valid unload/landing signal. No open-loop foot-lift trajectory
was sent. The visually rejected front-shoulder profile was reverted after the
test, so the source and installed runner retain the previously operator-
accepted stage-one joy choreography. The checked-in default allowlist remains
neutral-only.

### Torque-derived contact estimator — offline verification 2026-09-20

The next implementation step is diagnostic only. The official runner now
estimates each foot force from SDK joint position and torque by solving
`J(q)^T F = tau` with the maintained physical Lite3 geometry and joint
directions. It collects a distinct-tick exact-stand baseline, filters each
vertical load, and reports baseline validity, support count, total load,
per-foot load, and per-foot baseline under `estimated_contact` in expression
status. The published `motion_gate_enabled` value is hard-coded false, and the
estimator is not consumed by any trajectory or safety decision.

The deterministic recorded standing fixture estimates front-left/front-right/
hind-left/hind-right vertical loads of `23.44/24.34/31.23/35.73 N`, totaling
`114.74 N`. The configured 11.84 kg robot weighs approximately `116.15 N`, so
this one recorded sample differs by about 1.2%. The native test also verifies
that repeated ticks cannot inflate the baseline, unload and landing require
consecutive frames with hysteresis, and non-finite input, singular kinematics,
or an implausibly weak baseline fail closed.

`make -C lite3-noetic hardware-expression-tests` passed all four C++ suites.
`make -C lite3-noetic verify-hardware-offline` then passed the same four suites,
29 Python tests, a clean Release catkin build, packet/lease/crash-watchdog
integration, and launch enumeration. This is offline evidence only: the new
estimator was not run on the robot and no new physical command was sent.

The source was then compiled against the robot's actual aarch64 MotionSDK and
all four native suites passed on the perception computer. The runner was
installed with SHA-256
`4f4e9393a951d965cc1b603974fa21e996f78d395241ddbc7382ab4a2d9abc97`;
the previously installed SHA-256
`057babd3738f8ea240a0ccf1bbc5a96bb0999fa4e745a849ffdd2120baffcd97`
was retained as a checksum-named backup. No runner process was started, no SDK
ownership marker appeared, and no estimator values were collected from this
new build. The calibration and movement requirements are tracked separately in
[`TICKET_JOY_FRONT_PAW.md`](../../tickets/TICKET_JOY_FRONT_PAW.md).

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
## Anger controlled-placement implementation — 2026-09-20

The Anger ticket's explicit commissioning path was implemented without adding
`anger` to the normal hardware allowlist. It uses an 8 mm low brace, 6 mm
per-side stance widening, the accepted joy support transfers, a 35 mm paw
target, a 0.35-second quintic lowering, a 0.25-second zero-lift landing dwell,
and four-foot recovery. Its analytic maximum Cartesian lowering speed and
acceleration are 0.1875 m/s and approximately 1.65 m/s^2. A separate gate
prevents the second paw from starting before confirmed four-foot landing and
the complete dwell.

`make -C lite3-noetic verify-hardware-offline` passed: six ROS-independent C++
suites, a clean Release catkin build, 29 Python tests, the packet test, the
lease/release/crash-watchdog integration test, and launch enumeration. The same
six native suites and the official runner compiled and passed on the aarch64
perception computer against its installed MotionSDK. The installed candidate
runner SHA-256 is
`2501f3bbb819c636a04f599b9630332ffd3a2d1d79b71a6d6d84116828e1e27d`;
the accepted joy binary was retained as a checksum-named backup.

A no-command preflight then observed state/gait/motion `98/0/0`, battery `91%`,
zero error flags, a compatible SDK layout, normal attitude, and no official or
legacy ownership marker. Because only exact sitting state `1` permits
acquisition, the runner rejected the attempt before constructing a sender. No
MotionSDK acquisition or Anger movement occurred in that preflight.

After the operator restored exact state `1` and reconfirmed the clear area and
STOP readiness, the bounded single left-front placement ran with a 75% battery
floor. Its 753-sample baseline was valid at 120.512 N. Unload was confirmed at
4.116 N with two strong supports; landing was confirmed at 17.987 N with four
supports after the 0.25-second dwell. Feedback age/update-gap maxima were
8.017/8.624 ms with `0/0` pauses/recoveries. The runner reported no safety
fault, released its marker, and left the robot at state/gait/motion `1/0/0`,
battery 90%, zero errors, STOP false, and centered axes.

The complete left-then-right loop then ran from a fresh state-`1` preflight. Its
706-sample baseline was valid at 120.681 N. Front-left unload/landing were
3.992/18.101 N; front-right unload/landing were 0.603/14.071 N. Both landings
restored all four supports, so the second-paw gate passed only after the first
dwell. A 148.120 ms feedback-age event occurred in the preceding neutral
window; the runner froze and recovered after 20 fresh frames before Anger began.
Neither placement paused. Final feedback statistics reported one pause and one
recovery, 148.120/148.250 ms maximum age/update gap, no safety fault, and clean
release. Post-run state remained `1/0/0`, battery 89%, zero errors, STOP false,
centered axes, active services, and no owner. Normal chat/retarget integration
remained pending.

The operator then requested 15 seconds of continuous Anger. A hard-capped
three-cycle commissioning option was compiled and all six suites passed locally
and on aarch64. Installed runner SHA-256 was
`1bedfbf71d30a8dae52d403b8ed4eb346ef3e5835be549cd74a3e8ec553f597c`.
The session baseline was valid with 702 samples and 121.990 N. Cycles 1 and 2
passed both unload/landing pairs. Cycle 3 passed the left pair and right unload,
but its final right landing ended at 13.133 N with only three estimated supports
re-latched. The required four-support landing check therefore failed.

The runner completed hold/recovery, released ownership, reported no safety
fault, and exited nonzero as designed. Feedback had zero pauses/recoveries with
82.340 ms maximum age and 11.489 ms maximum consecutive update gap. Post-run
state was `1/0/0`, battery 87%, errors zero, STOP false, centered axes, active
services, and no owner. This is a failed endurance acceptance result and must
not be converted into a pass by weakening the contact gate.

After observing the run, the operator reported that the motion looked good.
This supplies positive visual acceptance of the Anger choreography. It does not
change the failed endurance result: the third-cycle landing issue and normal
chat/retarget integration remain open.

## Anger landing and chat-path hardening — 2026-09-20

No robot connection or command was used for this follow-up. The third-cycle
failure was addressed without changing the load thresholds: after each
0.25-second zero-lift dwell, the planted body now recenters over 0.30 seconds and
only then evaluates the same four-support latch. A following paw always starts
from center, so an unloading offset cannot accumulate across paws or loops.

The normal expression loop now routes an allowlisted `anger` state to the
contact-gated paw state machine rather than the planted prototype. It observes
new sequence-locked chat state throughout every phase. A new category suppresses
the next stomp, finishes any current lowering/dwell/re-latch, returns to exact
stand over 1.5 seconds, holds it for 0.35 seconds, and activates only the newest
pending commissioned category. A stale link performs the same safe reset and
then releases. STOP and hard faults retain immediate release semantics. The
checked-in allowlist remains `neutral`.

`make -C lite3-noetic verify-hardware-offline` passed with seven native C++
tests, including raised-paw and placement retarget cases plus compilation of the
complete runner against SDK-shaped stubs; a clean Release catkin build; 29 Python
tests; and the protocol/lease/graceful-release/crash-watchdog integration test.
At that stage the hardened source had not been compiled against the robot's
actual aarch64 MotionSDK, deployed, or physically revalidated; the earlier
checksum and endurance failure therefore remained the current live evidence.

### First hardening live result and canonical-reset follow-up

With fresh operator confirmation of a clear level area and Retroid STOP
readiness, the x/y-recenter revision was clean-built in a new aarch64 temporary
directory against the installed MotionSDK. All seven tests passed, the installed
artifact matched the clean artifact, and its SHA-256 was
`37793e3eb35f7dbc8e99bbd0d21619a74b0a52558572dea315e387c85169b3ee`.
The checked-in runtime allowlist remained `neutral`; only the explicit bounded
three-cycle commissioning mode was selected.

The baseline was valid with 751 samples and 120.606 N total load. Cycle 1 passed
left unload/landing at 4.298/19.680 N and right at 0.396/11.317 N, each restoring
four supports. Cycle 2 passed left at 1.648/14.527 N. Its right paw unloaded to
0.001 N and landed at 13.066 N, but only three supports were latched after the
0.30-second x/y recenter. The runner failed closed, did not start cycle 3,
completed recovery, and released SDK ownership. It reported no safety fault and
no feedback pause; maximum feedback age/consecutive update gap was
49.891/50.736 ms.

Fresh post-release evidence was state/gait/motion `1/0/0`, battery 79%, errors
zero, roll/pitch `0.346/0.399 deg`, STOP false, fresh centered axes, no runner or
ownership marker, and four recovered loads at
23.327/26.329/40.331/33.006 N. No retry was made.

That result disproves the assumption that x/y recentering alone is sufficient.
The next local-only revision instead returns body shift, brace, and stance width
to exact canonical stand over 1.0 second, holds it for 0.35 seconds, and then
applies the unchanged four-support latch. It also logs all four forces at every
landing. Seven native tests, including the full-runner compile harness, pass.
At this point in the sequence it was not installed and had no physical evidence.

### Canonical-reset three-cycle physical result

After a new explicit authorization, the canonical-reset revision clean-built
against the actual aarch64 MotionSDK and passed all seven suites. The installed
artifact matched SHA-256
`a77433ace5afb56a4bbd204df28c167cf7d46022d2b3155568086ac78fdd630c`.
The normal category allowlist remained `neutral`; the run used only the explicit
bounded three-cycle suite, a 75% battery floor, left-paw-first order, and the
35 mm target.

Its 722-sample baseline measured 121.912 N. All six placements passed the
unchanged unload, support, touchdown, and restored-four-support gates:

- cycle 1 left `3.901/29.767 N`, right `1.386/29.509 N`;
- cycle 2 left `1.849/26.950 N`, right `0.466/29.667 N`;
- cycle 3 left `2.085/26.949 N`, right `0.160/29.563 N`.

The runner reported no safety fault, feedback pause, or recovery. Maximum
feedback age/consecutive update gap was `12.291/10.382 ms`; the ownership marker
was absent after release. Fresh post-run state was `1/0/0`, battery 77%, errors
zero, roll/pitch `0.392/0.204 deg`, STOP false, fresh centered axes, no runner,
and four loaded feet at `24.131/30.575/43.193/38.545 N`.

This physically validates the canonical-reset repeated commissioning path. It
does not validate normal chat-driven selection or a retarget received during an
active paw sequence. The checked-in normal allowlist therefore remains
`neutral` pending that separately authorized live test.

## Fear implementation and bounded physical trials — 2026-09-20

Fear was implemented as a separate contact-gated state machine rather than the
older planted fallback. The redesigned explicit five-second suite contains a
0.35-second flinch, 0.55-second rearward recoil, two 0.55-second low guards
separated by 0.35-second lowering plus 0.20-second landing dwells, a 0.75-second
four-foot freeze, and a 1.15-second exact recovery. The 25 mm candidate has
analytic quintic touchdown maxima of about 0.134 m/s and 1.178 m/s². The second
hover is impossible until the first landing has restored all four support
latches.

`make -C lite3-noetic verify-hardware-offline` passed: eight native C++ suites,
including the complete runner compile harness and Fear bounds/order/retarget
tests; a clean Release catkin build; 29 Python tests; packet and
lease/release/crash-watchdog integration; and launch enumeration. A clean
aarch64 build against the perception computer's installed MotionSDK also passed
all eight suites. The installed redesign is an ARM aarch64 executable with
SHA-256
`c4250e670d2c1a2f543777559fe1a368576bbda15b6c80b9fb94cf3d87ce5c84`;
the previously accepted Anger binary remains preserved by checksum.

The connected read-only preflight found active core/telemetry/STOP services,
legacy transmit flags false, no runner, no official/direct-joint ownership
marker, battery 70%, zero error flags, compatible SDK layout, and level
attitude. It also found basic state `98`, Retroid `fresh=false` and
`axes_zero=false`, and joint/IMU readiness false. Those conditions fail the
exact state-`1`, fresh-centered manual input, and healthy telemetry gates. No
runner was started, no SDK sender was constructed, and no physical motion
occurred. At that preflight stage, physical Fear commissioning and operator
visual acceptance remained open; the later planted run below closed those
physical-animation gates. The normal allowlist remains `neutral`.

The operator then explicitly accepted the existing 25% configured battery
floor for Fear commissioning. A fresh read-only check showed battery 69%, zero
errors, and normal roll/pitch, so battery was not a blocker. State remained
`98`, Retroid readiness remained stale/non-centered, and joint/IMU readiness
remained false. Those independent hard gates were preserved; no SDK acquisition
or motion occurred.

After the robot returned to state `1`, four first-candidate runs were bounded by
the same gates. Two 20 mm-transfer left-paw attempts failed unload at 9.644 N
and 9.617 N. A correctly applied 30 mm transfer passed one single-paw unload at
5.986 N and landed at 25.865 N, but the complete suite's next baseline retained
6.531 N and failed before the second paw. All runs landed, recovered, released,
and reported no safety fault or feedback pause. The operator rejected that
choreography because it looked nothing like fear.

The replacement used a 22 mm flinch, 30 mm rearward recoil, 18 mm crouch, 8 mm
widened stance, symmetric 35 mm transfers, longer low freeze, and slower
recovery. Fresh preflight was state/gait/motion `1/0/0`, battery 65%, zero
errors, normal attitude, STOP observer active, a 2.144 ms fresh centered Retroid
record, and no owner. The 725-sample baseline measured 123.871 N. The left-paw
hover retained 7.264 N and all four support latches, so the unload gate failed
closed; landing restored four supports at 18.876 N. Recovery and release
completed with maximum feedback age 7.353 ms, maximum consecutive update gap
7.703 ms, no pause, and no safety fault. Fresh post-run state was `1/0/0`,
battery 64%, errors zero, no runner, and no owner. No second paw or complete
redesigned loop was attempted. At that stage physical and visual acceptance
remained open; the later planted run below supplied the accepted Fear animation.
The normal allowlist remains `neutral`.

The operator then requested 15 seconds of Fear. Because the paw unload had
failed, the runner did not bypass or retry that gate. A separate all-feet-
planted visual diagnostic was added: three exact five-second cycles, each with
a 0.35-second flinch, 0.55-second recoil, five 0.45-second 8 mm lateral cower
transitions, 0.70-second freeze, and 1.15-second exact recovery. Native and
aarch64 test suites both passed all eight tests. The installed runner SHA-256
was `fb3eea752936de30481fb601e3df5e348613a64a9cd691866e539e00587b4797`.

Fresh preflight was state/gait/motion `1/0/0`, battery 61%, errors zero, normal
attitude, active STOP observer, fresh centered Retroid input, STOP false, and no
owner. All 15 seconds and all three exact recoveries completed. One stale
`0x0906` interval occurred during the pre-expression stand hold: maximum age
146.177 ms, 71.017 ms pause, one recovery. The watchdog held the last validated
command and resumed after 20 fresh samples. There was no safety fault. Final
state was `1/0/0`, battery 60%, errors zero; all four supports measured
30.051/26.932/37.012/35.635 N, and no runner or ownership marker remained. This
validates the physical Fear animation. The operator subsequently confirmed that
it worked correctly on the robot and visually accepted it. Normal chat
selection and retargeting remain unvalidated, so the normal allowlist stays
`neutral`.
