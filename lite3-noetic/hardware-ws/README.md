# Lite3 hardware workspace

This is a separate catkin workspace for the perception computer. It does not
modify or overlay the vendor `~/lite_cog` workspace or `qnx2ros`. The normal
`lite3-noetic` simulation image does not launch this package.

The legacy diagnostic package starts fail closed:

- `transmit_enabled: false` and `dynamic_actions_enabled: false`;
- all nine posture amplitudes and rates are zero;
- its posture and dynamic-action transmit flags are false;
- STOP-preemption is unverified and all action trajectories are empty.

Consequently neither legacy posture nor action output can arm from the
checked-in configuration. The separate official runtime uses the reviewed
Deeprcs `2.0.153` layout and an independent lease; its default commissioned
allowlist contains only the already proven neutral profile.

## Initial installation and redeployment after source changes

Connect the development computer to the robot Wi-Fi, keep the robot sitting, and
run the installer from the wrapper root:

```bash
cd /home/dimitarbez/Dev/ROS
make -C lite3-noetic setup-emotion-hardware
```

The installer verifies the observed host identities and interfaces, copies this
package into the separate `~/emotion_bot_lite3_hw_ws` workspace, builds it
against ROS Noetic and the existing `message_transformer` package, and installs
three narrowly scoped systemd services. It never copies into or edits
`~/lite_cog`, `jy_exe`, or `network.toml`.

Run the same setup target again after changing anything under
`lite3-noetic/hardware-ws/tools/` or the robot-side hardware package. The normal
`run-emotion-hardware` target uses the already installed aarch64 runner; it does
not build or deploy local source changes. Keep the robot sitting during setup.

Only the perception telemetry service and motion-host STOP observer receive
`CAP_NET_RAW`; the ROS core remains unprivileged. A generated HMAC key
authenticates the STOP/status relay and is installed outside Git under
`/etc/emotion-bot`. Staged copies of that key are removed after installation.

## Architecture and interfaces

- `emotion_receiver.py` binds only `127.0.0.1:8767`, limits frames to 2 KiB,
  validates transport schema 1.0 and emotion schema 1.1, and rejects replayed
  sequences within a session.
- `telemetry_tap.py` accepts only UDP from `192.168.1.120` to port `43897`,
  decodes the documented 200-byte `0x0901`, and puts 368-byte `0x0906` bodies in
  a versioned sequence-locked shared-memory file only after reviewed layout and
  value validation.
- `motion_stop_observer.py` runs without ROS on the motion host, passively watches
  `p2p0`, and sends only authenticated STOP/axis observations to the perception
  host on UDP `43910`. It never targets the robot command port and never changes
  `jy_exe`.
- `stop_relay_receiver.py` accepts frames only from
  `192.168.1.120:43911`, verifies their HMAC/session/sequence, and publishes the
  local ROS STOP and Retroid-health topics. A stale or lost relay removes the
  arming gate.
- The mapper, supervisor, posture bridge, and action controller are excluded
  from the official core launch. The explicit `hardware_diagnostic.launch`
  retains them fail closed under `/emotion_bot/hardware/...`; `/simple_cmd` is
  that diagnostic graph's sole posture path.
- The passive telemetry tap publishes each perception-to-motion-host UDP command
  observed on `eth0` as inspectable JSON on
  `/emotion_bot/hardware/wire_command`. This topic proves interface-level packet
  emission; it does not by itself prove that `jy_exe` accepted or executed the
  command. The exact 240-byte official MotionSDK joint command `0x0111` is the
  sole exception: it runs at 1 kHz and is consumed without ROS publication so
  diagnostic serialization cannot starve the shared-memory `0x0906` safety feed.
- The deployed vendor `/ros2qnx` subscriber has a queue depth of one. The posture
  bridge therefore serializes heartbeat, Pose-mode, and commissioned-axis
  messages and emits only axes with nonzero commissioned limits/rates. Sending
  four back-to-back axis messages lets zero-valued uncommissioned axes evict the
  active height command before the vendor callback runs.

Diagnostic services are `/emotion_bot/hardware/set_armed` (`std_srvs/SetBool`),
`/emotion_bot/hardware/set_dynamic_actions_enabled` (`std_srvs/SetBool`), and
`/emotion_bot/hardware/neutral` (`std_srvs/Trigger`). Status is latched JSON on
`/emotion_bot/hardware/status`; robot state is the typed
`emotion_bot_lite3_hw/RobotState` message.

The hardware neutral profile is static by default. An optional, disabled-by-
default height-only neutral breather reproduces the Gazebo profile's 0.25-second
quintic entrance and 1.375-second idle cadence without enabling attitude,
locomotion, or dynamic actions. It may be enabled only in a temporary reviewed
commissioning configuration. The posture bridge independently expires stale
posture intentions to exact zero using `posture.intent_timeout`, so a failed
mapper cannot leave a nonzero target active while the AI link remains fresh.

### Retroid-compatible posture framing

A 2026-09-19 direct diagnostic finally produced measured body motion after
matching the full Lite3 app's captured session and right-stick framing. From the
development computer's Windows interface `192.168.2.28:43897` to motion host
`192.168.2.1:43893`, the successful order was:

1. four heartbeat commands (`0x21040001`) at 2 Hz;
2. one Move command (`0x21010D06`), then a 2.0-second wait;
3. one Pose command (`0x21010D05`), then a 1.5-second wait;
4. at 50 Hz, a paired `0x21010135` value `32768` immediately followed by the
   `0x21010102` height value; and
5. on every exit path, five pairs of yaw `0` followed by height `0`, 20 ms apart.

The `32768` value is an observed Retroid companion value, outside the documented
signed yaw posture range. Treat it as opaque compatibility framing: do not scale
it, interpret it as a requested yaw angle, or generalize it to another firmware.
The diagnostic host-side bridge owns this exact handshake only when
`run-emotion-hardware-retroid-diagnostic` is explicitly active. It refuses to
coexist with an official or legacy direct-joint ownership marker, starts the
separate fail-closed diagnostic graph, and acts only while the received emotion
is `neutral` and every read-only gate remains fresh. A 250 ms relay watchdog and
every normal exit send five paired yaw/height zeros.

Phase-isolated testing showed that the earlier `+10000..-3900` waveform spent
its periodic range inside the deployed controller's effective deadband; its
`0.2544403 rad` whole-run span included the Move-to-Pose transition and was not
proof of a continuing breath. A later fixed `-10000` trial reset measurement
after that handshake and measured `0.2484894 rad` joint span and `0.0078653` IMU
orientation-component span during height actuation. The default neutral cycle
therefore ramps smoothly between `0` and `-10000`, never exceeding 50% of the
documented `20000` magnitude, at most `4000 units/s` with a 10-second period.

The operator explicitly selected a 25% runtime floor; output stops below it and
resumes only after all gates hold continuously for two seconds. The Pro manual's
75% start recommendation remains documented separately.

The older action coordinator still expects a separately configured
`~sdk_sender_path`, but it is now diagnostic-only and absent from the official
launch. Do not configure it with the official expression executable: that
would create a second owner and violate the continuous-session design.

A 2026-09-19 hoisted direct-joint commissioning attempt additionally proved
that `Sender::ControlGet(SDK)` (`0x0114`) is not a seamless handoff from the
vendor standing controller. A measured-position hold and a 10%-scale neutral
request both produced large acquisition transients before tracking-error release.
`direct_joint.takeover_transition_commissioned` therefore remains false and is
an independent hard gate even when a launch supplies `transmit_enabled=true`.
Do not bypass it to command small offsets from a vendor-controlled standing
pose. Direct-joint work now requires a reviewed vendor-supported zero-to-stand
SDK transition before expression trajectories can be commissioned.

The official `motion_sdk_expression_runner` uses the commissioned
two-stage feedback watchdog for that zero-to-stand path. A `0x0906` age above
100 ms freezes trajectory time and repeatedly sends the last validated joint
positions with zero desired velocity and torque. It does not advance the stand
or breathing profile while feedback is stale. Motion resumes only after 20
distinct finite feedback ticks arrive at no more than 20 ms age. If feedback
reaches 250 ms, or if values or tracking are invalid, the runner still executes
the vendor return-to-robot sequence and never retries automatically. This
bounded hold addresses measured 100-166 ms passive-tap scheduling gaps without
turning a dead stream into an unbounded blind stand; a persistent fault can
still make the vendor release sequence lower the robot.

The commissioned animal-like neutral cycle remains a deliberately slower physical
translation of Gazebo's two-sided height/roll/pitch loop. It takes 0.75 seconds
from exact neutral to an expanded endpoint, 1.25 seconds through the compressed
endpoint, and 1.25 seconds back to exact neutral. Base HipY compression ranges
from `-0.008 rad` extension to `+0.020 rad` compression; knee motion is twice
that scalar. Opposing left/right `0.004 rad` and front/rear `0.003 rad` biases
produce bounded roll and pitch while all four feet remain planted. The combined
per-leg HipY scalar remains inside `-0.015..+0.027 rad`, HipX stays fixed, and
yaw remains zero because Gazebo neutral also commands zero yaw. Every segment is
quintic with zero velocity at its endpoints and the 3.25-second loop seam.

The passive tap also publishes each validated raw `0x0901` body through the
sequence-locked `/dev/shm/emotion_bot_lite3_robot_state` record. The standalone
runner checks this independent safety channel throughout initialization,
standing, holding, and breathing. Basic state `8` (lose-control protection) or
any nonzero error flags immediately stop trajectory advancement and enter the
same no-retry MotionSDK release path. As with the high-rate reader, a transient
sequence-lock collision retains the last fully validated state record rather
than being misclassified as a safety fault.

The legacy hard gate is deployed on the perception computer and defaults false
in both configuration and the bridge executable. The 2026-09-20 offline gate
covers a clean Release build, 29 Python tests, four ROS-independent C++ suites,
exact packet codes, exclusive sender ownership, graceful release, independent
crash-watchdog release, and launch enumeration. A live full-amplitude symmetric
run completed 19 five-second cycles before fresh state `8` triggered the
intended no-retry return-to-robot path. A subsequent animal-like run completed
at least 27 3.25-second cycles and remained active at the documented handoff;
four measured telemetry gaps paused and recovered without a tracking or state
fault. See `docs/HARDWARE_APP_CONTROL.md` for the complete dated attempt history
and measurements.

Validated emotion state is copied into
`/dev/shm/emotion_bot_lite3_emotion_state` with the state-1.1 fields plus the
transport session/sequence and monotonic receive time. The ROS-independent
runner is its only actuator consumer. Runner status is copied back through
`/dev/shm/emotion_bot_lite3_expression_status` and published read-only as JSON
schema `1.0` on `/emotion_bot/hardware/expression_status`. Additive fields expose
the transport/state sequence, session and turn ID, valence/arousal, requested
emotion, resolved emotion/profile, active profile, pending request/profile, and
an explicit fallback flag/reason. Thus an unsupported `affection` request remains
truthfully visible while `neutral_animal_breath` is active; status never claims
that EmotionEngine itself changed to Neutral.

The normal selector is table-driven for all nine EmotionEngine categories.
Neutral resolves to the accepted animal breath; allowlisted Joy resolves to the
accepted alternating 50 mm front-paw gesture; allowlisted Sadness resolves to
the accepted front-only 48 mm bow; allowlisted Fear resolves to the accepted
all-feet-planted flinch/recoil/cower/freeze loop; and allowlisted Anger resolves
to the accepted canonical-reset controlled paw placements. Affection, Curiosity,
Disgust, and Surprise always resolve to neutral with reason
`physical_reaction_not_accepted`, even if mistakenly named in the environment
allowlist. An accepted category omitted from the normal allowlist also resolves
to neutral with reason `not_enabled_in_normal_allowlist`.

The runtime allowlist remains `neutral` by default. Set
`HARDWARE_COMMISSIONED_EMOTIONS` only to categories with complete live normal
chat evidence. Every category change first finishes the active phase and, for
Joy or Anger, lowers and settles a raised paw. It then returns to exact stand
for 1.5 seconds and holds it for 0.35 seconds. Rapid changes replace one pending
request without restarting that reset. Unsupported-to-unsupported updates do
not create needless motion but still update requested-category and sequence
status. True airborne hops and forceful strikes remain unimplemented.
The live SDK-owned feedback reports zero in the vendor contact array, so lifted
paw gates use the separate guarded torque-derived estimate below.

The runner now also computes a **read-only torque-derived foot-load estimate**
from the joint positions and torques in the same official SDK feedback. It uses
the maintained Lite3 leg Jacobian and solves `J(q)^T F = tau`; it does not use
the all-zero vendor contact array. During the exact-stand hold it collects at
least 50 distinct samples and accepts a baseline only when every foot and the
total load are plausible and stable. The additive `estimated_contact` object in
`/emotion_bot/hardware/expression_status` exposes validity, baseline validity,
support count, total and per-foot vertical load, and the per-foot baseline.
`motion_gate_enabled` is deliberately `false`: this diagnostic cannot yet
authorize a lift or landing. Its deterministic test reproduces a recorded
standing sample at `23.44/24.34/31.23/35.73 N` (total `114.74 N`, versus
`116.15 N` configured static weight) and exercises fail-closed invalid,
singular, weak-baseline, unload, and landing cases. See
[`TICKET_JOY_FRONT_PAW.md`](../../tickets/TICKET_JOY_FRONT_PAW.md) for the
separate calibration and lifted-paw movement plan.

The Anger candidate is isolated behind explicit commissioning flags and is not
part of the normal category allowlist. `HARDWARE_ANGER_SINGLE_STOMP_TEST=true`
runs five seconds of accepted neutral breathing followed by the new planted
glare and one 35 mm front-paw controlled placement.
`HARDWARE_ANGER_SUITE_TEST=true` runs the full
left/right sequence, and `HARDWARE_ANGER_FIRST_PAW=left|right` selects its
order. The 2026-09-22 development choreography adds a 1.10-second planted
forward-body bias (20 mm negative body-frame X), 40 mm front-body sink, and
10 mm stance widen, then a 0.35-second hold and 1.10-second exact-stand reset.
The paw gestures remain 35 mm controlled placements. The previously accepted
short rearward support shifts during paw unloading are unchanged; this is not
walking or world-frame forward travel. Before lifting a paw it requires four
restored estimated supports. This revised choreography has only offline validation, not physical
acceptance; the deployed checksum and live results below refer to the earlier
version. Each placement still uses a 0.35-second quintic lowering and a
0.25-second stationary landing dwell. The commissioned implementation then returns shift,
brace, and stance width to canonical stand over 1.0 second and holds it for
0.35 seconds. The unchanged four-support gate is evaluated after that support
reset, and the second placement cannot start before it passes. See
[`TICKET_ANGER.md`](../../tickets/TICKET_ANGER.md). The 2026-09-20 bounded
single-placement and complete alternating physical suites both passed with
confirmed unload and four-foot landing, no safety fault, and clean release.
The operator subsequently reported that the motion looked good, providing
visual acceptance. Normal physical Anger selection and two consecutive repeated
loops now pass after a bounded left support-transfer correction. The
newest-request retarget path still uses 1.5 seconds of recovery plus a
0.35-second exact-neutral hold. Its live Anger-to-Disgust test now passes: the
runner completed the active placement and support restoration, returned
through exact neutral, and activated the observable Neutral fallback without
releasing ownership. The checked-in allowlist remains Neutral-only pending the
cross-emotion ticket's consolidated operator verdict.

Commissioning suite repetition is explicitly capped at three with
`HARDWARE_ANGER_SUITE_CYCLES=1..3`. The first 15-second run passed two complete
cycles, then failed closed on cycle 3 because the final right landing restored
only three estimated supports after its dwell. Recovery and SDK release still
completed without a safety fault. Do not weaken the four-support gate or retry
the repeated run without reviewing that support redistribution. The positive
operator visual verdict does not override this failed telemetry gate.

The first source fix did not lower any load threshold: it added a 0.30-second
planted x/y recenter before the gate. It clean-built against the actual aarch64
MotionSDK, passed all seven tests, and was installed as SHA-256
`37793e3eb35f7dbc8e99bbd0d21619a74b0a52558572dea315e387c85169b3ee`.
Its authorized live repeat passed cycle 1, then failed closed on cycle 2's final
right landing at 13.066 N because support count remained three. It recovered to
four loaded feet, released with no safety fault, and ended in state `1/0/0`.

The replacement uses the full canonical support reset described above and logs
all four landing forces. It passed the seven local and aarch64 native tests and
the authorized three-cycle physical suite. Its installed SHA-256 is
`a77433ace5afb56a4bbd204df28c167cf7d46022d2b3155568086ac78fdd630c`.
The 722-sample baseline measured 121.912 N; all six placements restored four
supports. Cycle 1 left/right unload-to-landing values were
`3.901/29.767 N` and `1.386/29.509 N`; cycle 2 values were
`1.849/26.950 N` and `0.466/29.667 N`; cycle 3 values were
`2.085/26.949 N` and `0.160/29.563 N`. There was no safety fault or feedback
pause, ownership released, and fresh post-run state was `1/0/0` with battery
77%, zero errors, STOP false, centered axes, and no owner.

The normal allowlist remains `neutral`. Do not add `anger` to
`HARDWARE_COMMISSIONED_EMOTIONS` until the complete cross-emotion sweep receives
its consolidated operator verdict.

The Sadness candidate is likewise isolated from the normal allowlist.
`HARDWARE_SADNESS_BODY_VISUAL_TEST=true` runs a separate all-feet-planted
7.8-second head-down animation after the five-second Neutral window. The
accepted revision lowers only the front corners by 48 mm over 1.50 seconds,
settles, performs three 14 mm crying heaves with alternating 4 mm side tilt,
holds the deep bow, and recovers exactly over 1.50 seconds. Rear corners remain
at neutral and every corner stays within the 50 mm Cartesian workspace. Set
`HARDWARE_SADNESS_BODY_VISUAL_CYCLES=2` for the bounded 15.6-second repeat;
only `1..2` is accepted. It does not bypass or weaken the paw contact gates.
`HARDWARE_SADNESS_SINGLE_HOVER_TEST=true` runs the five-second accepted Neutral
window followed by one complete six-second lowered-paw loop.
`HARDWARE_SADNESS_SUITE_TEST=true` runs two complete loops, alternating front
paws; `HARDWARE_SADNESS_FIRST_PAW=left|right` controls the order, and
`HARDWARE_SADNESS_LIFT_METERS` is restricted to `0.015..0.025`.

Each loop uses a 20 mm common crouch, 5 mm front droop, 2 mm support-side roll
bias, 4 mm widened stance, and a default 20 mm hover. Its fixed phases are
1.20-second sink, 1.00-second withdrawal, 0.80-second lift, 1.00-second pause,
0.55-second gentle lower, 0.25-second landing dwell, and 1.20-second exact
recovery. Lowering removes the x/y support transfer before the landing gate;
recovery then removes every remaining Cartesian offset and requires four
supports. A new category cannot abandon a lifted paw and uses the global
1.5-second canonical return plus 0.35-second exact-neutral hold. A request
arriving during normal recovery causes a fresh complete neutral transition
rather than shortening it.

The complete local hardware gate and all nine clean aarch64 suites pass. The
current installed runner SHA-256 is
`7c2980b02b432a62d18c546853574a36efb4f0b4add91536836f4f0fdb4e648a`.
The first physical single-hover candidate completed its phases but failed
front-left unload at `8.875 N`; recovery restored four supports and release
completed without a safety fault. The operator rejected that visual read.

The revised planted bow then passed a bounded physical run. Its 732-sample
baseline measured `122.757 N`; the low pose retained four supports at
`26.917/27.331/23.078/27.284 N`. The 3.60-second still hold and exact recovery
completed with no feedback pause or safety fault, ownership released, and
fresh postflight was state `1/0/0`, battery `49%`, errors zero, STOP false, and
no owner. The operator rejected it as too subtle. Two increasingly animated
planted versions also passed telemetry but were rejected as insufficiently
bowed. A 70 mm differential trial then reached `10.006 deg` during its initial
sink and tripped the hard 10-degree attitude gate before any heaves. It
released safely; postflight was `1/0/0`, battery `44%`, errors zero, STOP false,
four supports, and no owner. The operator judged that partial bow too deep.
The 56 mm midpoint also failed closed during its initial sink at `10.005 deg`
pitch, before any heave. One `100.646 ms` feedback-age pause recovered first.
Four supports remained latched, release completed without retry, and fresh
postflight was `1/0/0`, battery `42%`, errors zero, STOP false, and no owner.
Do not retry either rear-extension geometry. The replacement therefore removed
rear extension to preserve attitude margin. See
[`TICKET_SADNESS.md`](../../tickets/TICKET_SADNESS.md), and keep Sadness
suite-only.

The final front-only 48 mm redesign passed a single physical run with every
heave retaining four supports, no feedback pause or safety fault, exact
recovery, and clean release. The operator reported that it looked good. Its
unchanged two-cycle `15.6 s` repeat also passed all six heaves. The second
recovery safely held through one `100.208 ms` feedback-age event and resumed
after 20 fresh samples in `74.994 ms`; no safety fault occurred. Fresh
postflight was `1/0/0`, battery `39%`, errors zero, STOP false, four supports,
status cycle `2`, and no owner. The accepted planted path subsequently passed
normal physical selection and a Sadness-to-Neutral exact-canonical retarget.
Keep the global allowlist `neutral` until the full cross-emotion sweep closes.

The Fear candidate is also isolated behind explicit commissioning flags and is
not part of the normal allowlist. `HARDWARE_FEAR_SINGLE_HOVER_TEST=true` runs
one guarded front-paw placement after the five-second neutral window;
`HARDWARE_FEAR_SUITE_TEST=true` runs the complete five-second flinch, recoil,
alternating 25 mm guards, freeze, and recovery. Use
`HARDWARE_FEAR_FIRST_PAW=left|right` and keep
`HARDWARE_FEAR_LIFT_METERS` within `0.015..0.025`.

The rejected paw-hover candidate uses a 0.35-second quintic lower plus a
stationary 0.20-second landing dwell and remains available only through its
explicit commissioning modes. Normal allowlisted Fear never selects that
candidate; it selects the accepted planted body loop and uses the shared
newest-request, 1.5-second canonical return, and 0.35-second exact-neutral hold
contract.

The complete local hardware gate and all eight aarch64 suites pass. The current
redesigned runner SHA-256 is
`fb3eea752936de30481fb601e3df5e348613a64a9cd691866e539e00587b4797`.
The first physical candidate was visually rejected by the operator. A stronger
22 mm flinch, 30 mm recoil, 18 mm crouch, 8 mm widened stance, and 35 mm support
transfer was then tested in one bounded left-paw run. Its 725-sample baseline
was 123.871 N, but the front-left paw retained 7.264 N with four supports and
failed the unload gate. Landing restored four supports at 18.876 N; recovery
and release completed with no safety fault or feedback pause. Final state was
`1/0/0`, battery 64%, errors zero, with no runner or owner. See
[`TICKET_FEAR.md`](../../tickets/TICKET_FEAR.md); keep Fear suite-only until its
paw-hover telemetry gates pass. That discarded candidate does not replace the
accepted all-feet-planted Fear animation documented below.

`HARDWARE_FEAR_BODY_VISUAL_TEST=true` selects a separate all-feet-planted
15-second visual diagnostic. It runs three exact five-second cycles comprising
flinch, recoil, five bounded 8 mm cower transitions, freeze, and exact recovery.
It does not lift a paw or weaken/replace the failed unload gate. The authorized
physical run completed all three cycles, restored four supports each time, and
released with no safety fault. Final state was `1/0/0`, battery 60%, errors
zero, no runner, and no owner. The operator subsequently confirmed that the
Fear animation worked correctly on the robot and visually accepted it. The
integrated planted path later passed normal physical selection, repeated loops,
Fear-to-Neutral, and Fear-to-Anger retargeting. Keep the global allowlist
`neutral` until the full cross-emotion sweep closes.

## Physical chat-integration checkpoint — 2026-09-20

The physical robot, not Gazebo, completed normal validated-state selection for
Neutral, Joy, Sadness, Fear, and Anger under the sole continuous MotionSDK
owner. Joy completed two consecutive final-geometry loops plus mid-paw
Joy-to-Neutral and Joy-to-Surprise-fallback transitions. Sadness and Fear
completed exact-neutral retargets, and Fear then retargeted to Anger. Revised
Anger completed two full repeated loops and both placements in a third.

The first Anger-to-Disgust retarget was preempted when that session's explicit
75% battery floor observed 74%. The hard gate released ownership, and that
interrupted request was not counted as a pass. After the operator explicitly
accepted the remaining battery, a new session used the existing configured 25%
project floor; no source threshold or other safety gate changed. It completed
Anger-to-Disgust, Affection/Curiosity/Disgust/Surprise Neutral fallbacks, and a
rapid Anger -> Affection -> Sadness sequence in which only newest Sadness
started after exact neutral. A final session completed Sadness -> Fear and
Fear -> Neutral through the same transition contract.

Final postflight was state/gait/motion `1/0/0`, battery 54%, errors zero,
centered fresh Retroid, STOP false, released status, and no SDK owner, lock, or
runner. Those functional telemetry gates passed for that checkpoint; the later
Joy repeat-stop failure described below reopens Joy's normal-session
recovery gate. The consolidated operator verdict and resulting default-
allowlist decision also remain open.

A later live OpenAI retry first exposed intermittent WSL DNS failure and the
interactive client's shorter wait relative to the configured provider retry
window. The sidecar now uses explicit resolvers and the client derives a bounded
deadline that covers that complete window. Two independent live API smokes
passed before another physical acquisition. The physical retry used only the
planted `neutral,sadness,fear` temporary allowlist: a normal chat message
streamed an OpenAI response, printed `chat_backend=openai`, and activated the
correlated accepted Fear profile with four supports and no fault. A second live
OpenAI turn returned through the 1.5 + 0.35-second Neutral contract. Release was
clean; postflight remained `1/0/0`, battery 41%, errors zero, STOP false, and no
owner or runner. See the cross-emotion ticket for complete failure and retry
evidence.

A subsequent authorized live OpenAI session temporarily allowed all five
accepted profiles. Neutral, Sadness, Fear, Anger, and final Neutral were selected
under one owner; every observed support/landing gate passed, exact-neutral
category transitions completed, and all nine brief feedback pauses recovered.
Joy was intentionally skipped after battery declined from 37% toward 30% and the
preceding session had exposed a repeated-left unload failure. Shutdown released
cleanly with state/gait/motion `1/0/0`, battery 30%, zero errors, STOP false, no
safety fault, and no owner or runner. This evidence does not change the
Neutral-only checked-in default or substitute for the pending operator visual
verdict.

The pre-correction aarch64 runner used for that live session passed all nine
native suites and was installed at SHA-256
`c2723ef4140a1bda88d18d6bfd09494febb2f2dea8d8db3f9d32b1e683a8025a`.
It also preserves a hard robot-safety fault as the primary status cause rather
than overwriting it with a profile recovery diagnostic.

### Persistent Joy recovery hardening — deployed, live revalidation pending — 2026-09-20

A later natural-language OpenAI turn selected Joy correctly and completed one
full left/right reaction. Joy remained the current state, so the runner
correctly began another cycle. The next front-left unload retained `12.5215 N`
and was rejected by the unchanged gate. Lowering and landing completed at
`36.1785 N` with four supports, then the old policy incorrectly treated the
recovered miss as terminal and released ownership; the operator saw the robot
stop and lie down.

Normal Joy is a persistent profile, matching Neutral, Sadness, Fear, and Anger:
it repeats the accepted left/right cycle while the latest validated engine state
remains Joy. State sequences update affect and category; heartbeat traffic only
keeps the link fresh and neither starts nor stops a cycle. A missed unload can
take a non-releasing recovery only after confirmed landing and four restored
supports: the runner returns over 1.5 seconds, holds exact Neutral for 0.35
seconds, and resumes Joy under the same owner. All landing and hard-safety
failures still release fail-closed. No contact or safety threshold changed. The
full `verify-hardware-offline` target passes. A no-motion deployment began from
state/gait/motion `1/0/0`, battery 91%, zero errors, fresh centered Retroid,
STOP false, active safety services, and no owner. The aarch64 build passed all
nine native suites and installed SHA-256
`aa449b7883a3baf6ae816fc832dbf3b8b74bb5a1ac88c2e0313f3acab1c8f353`,
with the preceding binary retained as a checksum-named backup. The new behavior
has not yet been exercised physically.

### Persistent Anger contact-miss recovery — deployed, live revalidation pending — 2026-09-20

A normal Anger attempt correctly failed its front-left unload gate at
`6.24688 N`, then safely completed controlled placement, relatch, four-foot
hold, and canonical recovery. Landing was confirmed at `28.4243 N` with all
four supports and the runner reported no robot safety fault, but the old policy
still released ownership after this recoverable miss.

The development runner now distinguishes that exact normal-chat case from a
hard failure. Only after confirmed landing and four-foot support does it log
`ANGER_CONTACT_MISS_RECOVERED`, complete the unchanged 1.5-second canonical
return and 0.35-second exact-Neutral hold, and resume persistent Anger under the
same owner. Explicit suites remain fail-closed. Incomplete landing, STOP,
invalid state, stale/dead feedback, estimator or tracking failure, and every
other hard gate still release. No threshold or motion amplitude changed. The
complete `verify-hardware-offline` gate passes. After the robot returned to
state `1`, a no-motion preflight confirmed gait/motion `0/0`, battery 74%, zero
errors, fresh centered Retroid, STOP false, and no owner. The clean aarch64
build passed all nine native suites and installed SHA-256
`e72a2db71d1f569b55d5c5af10b82c6b48a6a850513bb71aa3bacd9a84d4e5cd`,
with `aa449...` preserved as a checksum-named backup. Postflight remained
`1/0/0`, battery 73%, errors zero, STOP false, and released. No motion command
was sent, so physical revalidation remains open.

## Copy-paste Joy recovery validation

Use this sequence only for an explicitly authorized physical session. Before
starting, stop any older chat client with `:quit` or `Ctrl+C`, keep the robot
sitting in basic state `1`, confirm battery above the selected floor, make STOP
available, clear the area, and ensure no other MotionSDK/Retroid sender owns the
robot.

After changing the runner, deploy it once from the development computer:

```bash
cd /home/dimitarbez/Dev/ROS
make -C lite3-noetic setup-emotion-hardware
```

For the first validation, expose only Neutral and Joy.

Terminal 1 — continuous owner, OpenAI bridge, brain, and hardware runner:

```bash
cd /home/dimitarbez/Dev/ROS
HARDWARE_COMMISSIONED_EMOTIONS=neutral,joy \
HARDWARE_MINIMUM_BATTERY=25 \
HARDWARE_EXPRESSION_SCALE=1.0 \
make -C lite3-noetic run-emotion-hardware
```

Terminal 2 — interactive chat:

```bash
cd /home/dimitarbez/Dev/ROS
make -C lite3-noetic run-emotion-chat
```

Send one Joy prompt and wait for at least two complete left/right reactions
before entering another message. The expected result is continued Joy cycles
under the same SDK owner for as long as Joy remains current. Then send
`event:neutral`; the active paw must lower, exact-Neutral return/hold must
complete, and Neutral breathing must continue without lying down. If a Joy
unload miss occurs after a confirmed landing with four restored supports, expect
`JOY_CONTACT_MISS_RECOVERED` followed by canonical Neutral and resumed Joy—not a
runner exit.

Optional terminal 3 — read-only status:

```bash
cd /home/dimitarbez/Dev/ROS
make -C lite3-noetic watch-emotion-hardware-status
```

End chat with `:quit`, then press `Ctrl+C` in terminal 1 and wait for the runner
to report release. Do not close terminal 1 first while a paw phase is active.

For the focused Anger recovery revalidation, use the same preflight and chat
terminal but start the owner with only Neutral and Anger exposed:

```bash
cd /home/dimitarbez/Dev/ROS
HARDWARE_COMMISSIONED_EMOTIONS=neutral,anger \
HARDWARE_MINIMUM_BATTERY=25 \
HARDWARE_EXPRESSION_SCALE=1.0 \
make -C lite3-noetic run-emotion-hardware
```

Enter `event:anger` once and wait for at least two complete reactions. Anger
must keep repeating while it remains current. If an unload miss is followed by
a confirmed target landing and four restored supports, expect
`ANGER_CONTACT_MISS_RECOVERED`, exact-Neutral recovery/hold, and resumed Anger
under the same owner. Any incomplete landing or hard safety failure must still
release. Finish with `event:neutral`, wait for Neutral breathing, exit chat with
`:quit`, and only then press `Ctrl+C` in the owner terminal.

Only after the bounded Joy validation passes, the previously accepted profile
set can be selected explicitly for a later authorized session:

```bash
cd /home/dimitarbez/Dev/ROS
HARDWARE_COMMISSIONED_EMOTIONS=neutral,joy,sadness,fear,anger \
HARDWARE_MINIMUM_BATTERY=25 \
HARDWARE_EXPRESSION_SCALE=1.0 \
make -C lite3-noetic run-emotion-hardware
```

Use these chat commands one at a time, waiting for the reply, printed state, and
physical transition before sending the next command:

```text
event:neutral
event:joy
event:sadness
event:fear
event:anger
event:affection
event:curiosity
event:disgust
event:surprise
```

The first five categories above have accepted physical profiles. Affection,
Curiosity, Disgust, and Surprise remain valid EmotionEngine states but
intentionally resolve to `neutral_animal_breath` with
`physical_reaction_not_accepted`; naming them in the environment allowlist does
not enable an unaccepted motion. Send `event:neutral` and wait for Neutral before
ending the chat and releasing the owner.

The checked-in default remains Neutral-only.

## Development-side commands

After the one-time installation, the normal workflow is exactly two terminals:

```bash
make -C lite3-noetic run-emotion-hardware
make -C lite3-noetic run-emotion-chat

# Optional read-only diagnostic terminal
make -C lite3-noetic watch-emotion-hardware-status
```

The first target checks that both robot services are active and both transmit
flags remain false, opens a loopback-only SSH tunnel through the motion host,
starts the ephemeral OpenAI sidecar and development-computer brain, and then
executes the official runner on the perception computer. Ctrl-C releases SDK
ownership before closing the tunnel and sidecar. Use
`run-emotion-hardware-offline` for the deterministic backend. The old height
bridge is isolated behind `run-emotion-hardware-retroid-diagnostic`.

These targets are intentionally distinct from every simulation target.
