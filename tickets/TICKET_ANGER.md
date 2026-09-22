# Physical Anger: Controlled Alternating Front-Paw Stomps

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Surprise](TICKET_SURPRISE.md)

## Status — prior gesture accepted; forward-lean revision offline-only

The 2026-09-22 development checkout adds an **uncommissioned** planted,
forward-leaning glare before the previously accepted paw sequence: over
1.10 s the body-frame foot targets shift 20 mm toward negative X (forward
body bias), the front targets move 40 mm in Z, and the stance widens 10 mm.
After a 0.35 s hold it returns to exact stand over 1.10 s. All four feet
remain commanded planted; the existing 35 mm paw lift, touchdown
speed/acceleration, landing dwell,
canonical between-paw reset, support thresholds, and ownership gates are
unchanged. Four estimated supports must be restored after the new display
before the accepted brace or either paw lift can begin. This changed full
profile has **no physical observation or operator acceptance** yet: earlier
acceptance and deployed checksums below describe the previous choreography.

The controlled placement state machine, normal chat selection, newest-request
retargeting, bounds, contact gates, and offline tests are implemented. The
hardened sequence does not weaken a threshold. A bounded single placement and
the earlier alternating loop physically passed on 2026-09-20, and the operator
visually accepted the choreography. The original repeat failed its third-cycle
final landing gate, and a first x/y-only recenter failed the same right-paw gate
in cycle 2. The canonical-reset revision was then clean-built against the
robot's actual aarch64 MotionSDK and passed its authorized three-cycle physical
suite: all six placements restored four-foot support, with no safety fault,
feedback pause, or ownership leak. Normal state-driven selection was then
tested on the physical robot. After one bounded support-transfer correction,
two complete repeated loops passed under the continuous owner. A first mid-
motion Anger-to-Disgust fallback attempt was safely preempted by its explicit
75% battery floor at 74%. After the operator accepted the remaining battery, a
new session used the configured 25% project floor and completed the same
retarget without changing any source threshold or other hard gate.

The currently installed integrated runner SHA-256 is
`e72a2db71d1f569b55d5c5af10b82c6b48a6a850513bb71aa3bacd9a84d4e5cd`.
The earlier explicit canonical-reset suite runner was
`a77433ace5afb56a4bbd204df28c167cf7d46022d2b3155568086ac78fdd630c`.
The previously accepted joy runner remains preserved under its checksum-named
backup, and the normal allowlist remains `neutral`.

## Goal and emotional read

Anger should read as assertive, deliberate alternating front-paw stomps: lower
the body, transfer support, lift one paw, place it firmly under a bounded
trajectory, recover contact, then mirror the other paw. It must be heavier and
more deliberate than joy, without becoming a jump, gait, kick, or torque strike.

## Canonical-reset 10.25-second development loop

Initial targets, subject to offline bounds and physical commissioning:

1. **Planted glare — 2.55 s:** lean the body forward, sink the forebody, and
   widen the stance; hold, return to exact stand, and require four supports.
2. **Brace — 0.55 s:** lower into a broad four-foot stance without X travel.
3. **First transfer/lift — 0.65 s:** transfer away and lift the first front paw
   approximately 35–50 mm.
4. **First controlled place — 0.60 s:** lower with bounded vertical speed and
   dwell 0.25 s without further downward drive.
5. **First support reset — 1.35 s:** keep the paw down, return every Cartesian
   offset to canonical stand over 1.0 s, hold for 0.35 s, then require all four
   support latches.
6. **Second transfer/lift — 0.65 s:** mirror only after four-foot support is
   restored.
7. **Second controlled place/reset — 1.95 s:** lower, dwell, return to canonical
   stand, hold, and confirm all four supports again.
8. **Hold — 0.65 s:** maintain a low assertive four-foot pose.
9. **Recover — 1.30 s:** return to the neutral entrance pose.

Exact phase timing may change after measured touchdown review, but total cadence
must remain slower and heavier than joy's 2.5-second-per-paw cycle.

## Contact and touchdown requirements

- Reuse joy's baseline, target unload, two-strong-support/70 N aggregate load,
  landing hysteresis, and restored four-foot support checks.
- Add an anger-specific maximum downward paw velocity and acceleration; no
  torque impulse or discontinuous command is permitted.
- Treat landing confirmation as evidence of placement, not permission to drive
  farther downward.
- Require a four-foot landing dwell before transferring to the opposite paw.
- Bound combined brace posture, transfer, and lift in Cartesian IK and joint
  space.
- Keep planar motion, yaw, gait/action, torque feed-forward, and external wrench
  commands zero.
- The planted negative-X display is a body-frame posture target, not walking
  or a world-frame translation. The previously accepted short rearward support
  transfers during single-paw unload remain necessary and unchanged.

## Transition behavior

A new emotion request while a paw is raised cannot interrupt the controlled
placement. Complete landing and four-foot dwell, recover the brace, return to
canonical stand over 1.5 seconds, hold exact neutral for 0.35 seconds, and then
start only the newest target. STOP and safety faults retain immediate verified
hold/release behavior.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [x] Implement brace, alternating lift/place, landing dwell, hold, and recovery.
- [x] Add anger-specific lift, downward velocity, acceleration, and dwell limits.
- [x] Reuse contact/load gates under the single official owner.
- [x] Test both paw orders, combined workspace, joint bounds, finite samples,
  touchdown velocity, acceleration, seams, and exact recovery.
- [x] Prove the second stomp cannot begin without confirmed landing and dwell.
- [x] Test retargeting during both raised-paw and placement phases offline.
- [x] Wire validated `anger` state through exact neutral behind the allowlist.
- [x] Add the planted forebody display with a four-support gate before paw lift.
- [ ] Physically revalidate the revised full profile and obtain a new visual
  verdict before treating this choreography as accepted or deploying it.
- [x] Pass the complete local offline gate, including full-runner compilation.
- [x] Compile the first hardened runner against the robot's actual aarch64
  MotionSDK and pass all seven suites.
- [x] Commission a bounded single stomp before the full alternating loop.
- [x] Complete the full physical loop with state, IMU, joint, contact, STOP,
  tracking, feedback, touchdown, ownership, and release evidence.
- [x] Obtain operator acceptance as anger without harsh impact.
- [x] Revalidate the canonical-reset three-cycle suite on hardware.
- [x] Validate normal chat selection and repeated loops on hardware.
- [ ] Validate a mid-motion normal chat retarget before adding `anger` to the
  normal allowlist, as tracked by the
  [chat-driven physical emotion integration ticket](TICKET_PHYSICAL_EMOTION_CHAT.md).

## Acceptance criteria

- Both front paws visibly unload and land one at a time.
- Each landing is decisive but bounded, with no commanded drive after contact.
- Four-foot support is restored between stomps and before recovery.
- The motion reads as anger rather than fast joy taps.
- Any normal emotion change passes through exact neutral.
- No impact fault, slip, body instability, stale continuation, or owner conflict.

## Physical commissioning evidence — 2026-09-20

Both sessions started from state/gait/motion `1/0/0`, battery above the 75%
runtime floor, zero errors, normal attitude, fresh centered Retroid axes, STOP
false, active services, and no ownership marker.

The bounded single left-front session established a 753-sample, 120.512 N
four-foot baseline. The paw unloaded to 4.116 N with two strong supports and
landed at 17.987 N with all four supports restored. Feedback age/update gap
maxima were 8.017/8.624 ms with no pause. The runner reported no safety fault,
released ownership, and the post-run state remained `1/0/0`, battery 90%, with
zero errors.

The subsequent complete loop established a 706-sample, 120.681 N baseline.
Front-left unloaded to 3.992 N and landed at 18.101 N; front-right then unloaded
to 0.603 N and landed at 14.071 N. Both landings restored all four supports.
One 148.120 ms telemetry-age event occurred during the preceding neutral window;
the watchdog froze the trajectory and recovered after 20 fresh frames before
Anger began. Neither placement paused. The session reported no safety fault,
released ownership, and ended at state/gait/motion `1/0/0`, battery 89%, zero
errors, STOP false, centered axes, and no owner.

These measurements establish bounded execution. The operator subsequently
reported that the 15-second choreography looked good, providing the required
visual verdict. Normal validated `anger` selection still requires physical
revalidation of the hardened landing/re-latch and chat-retarget paths.

### Fifteen-second observation

At the operator's request, the commissioning runner was extended with a
hard-capped three-cycle mode and rerun for 15 seconds of Anger under one owner.
The baseline was valid with 702 samples and 121.990 N total load. The first two
five-second cycles passed both paws completely:

- cycle 1 left unload/landing `4.201/17.720 N`, right `0.872/14.988 N`;
- cycle 2 left unload/landing `1.255/18.477 N`, right `0.121/14.606 N`.

Cycle 3 confirmed left unload/landing at `1.270/17.760 N` and right unload at
`0.007 N`. Its final right paw reached `13.133 N`, but only three supports were
re-latched at the end of the 0.25-second dwell, so the four-foot landing gate
correctly failed. The runner still completed the hold/recovery, released SDK
ownership, and did not report a safety fault. Feedback had no pause; maximum
age/update gap was `82.340/11.489 ms`. Post-run state was `1/0/0`, battery 87%,
errors zero, STOP false, centered axes, active services, and no owner.

This endurance run is a failed acceptance result even though the robot remained
safe. Do not increase impact, weaken the four-support rule, or retry blindly.
Review the observed choreography and the third-cycle support redistribution
before another repeated run or normal chat integration.

The operator then reported that the motion looked good. This accepts the visual
choreography, but does not override the failed third-cycle telemetry gate or
commission normal chat-driven Anger.

## First hardening and live revalidation

The first failure was evaluated while the body was still held in the right-paw
unloading offset. Revision
`37793e3eb35f7dbc8e99bbd0d21619a74b0a52558572dea315e387c85169b3ee`
preserved the dwell, added a 0.30-second planted x/y recenter, and retained every
load threshold. A clean temporary aarch64 build against the actual MotionSDK and
all seven suites passed before installation.

The authorized live repeat began from the guarded state-`1` path with a valid
751-sample, 120.606 N baseline. Cycle 1 passed left unload/landing at
`4.298/19.680 N` and right at `0.396/11.317 N`, with four supports at both
landings. Cycle 2 passed left at `1.648/14.527 N`; its right paw unloaded to
`0.001 N` and landed at `13.066 N`, but support count remained three after the
recenter. Cycle 3 did not start. The runner recovered and released with no
safety fault or feedback pause; feedback age/update-gap maxima were
`49.891/50.736 ms`.

Post-release state was `1/0/0`, battery 79%, errors zero, normal attitude, STOP
false, centered axes, all four recovered loads at
`23.327/26.329/40.331/33.006 N`, and no owner. This proves x/y recentering alone
does not resolve the repeated right-side support redistribution.

The next revision keeps the paw planted but returns body shift, brace, and
stance width to exact canonical stand over 1.0 second, holds it for 0.35
seconds, and only then evaluates the unchanged four-support latch. This
canonical support reset is used between paws and loops. It passed the seven
local and aarch64 native tests and the physical three-cycle suite recorded
below.

The normal runner now selects this contact-gated state machine when `anger` is
explicitly present in the commissioned allowlist. A newer chat category during
brace, lift, placement, dwell, hold, or recovery is latched without starting
another stomp. Any raised paw is lowered and re-latched first; the runner then
returns to canonical stand over 1.5 seconds, holds exact neutral for 0.35 seconds,
and starts only the newest pending commissioned category. Link staleness follows
the same recovery and then releases ownership. STOP and hard safety faults keep
their immediate fail-closed release behavior.

`make -C lite3-noetic hardware-expression-tests` passes the seven native C++
tests for the canonical-reset revision, including a full runner compile harness.
The installed runner is now the canonical-reset binary identified above. The
normal allowlist remains `neutral` because the chat selection/retarget path has
not yet been exercised on hardware.

## Canonical-reset three-cycle acceptance — 2026-09-20

With fresh authorization, the canonical-reset runner was clean-built against
the actual aarch64 MotionSDK, passed all seven suites, and was installed with
SHA-256
`a77433ace5afb56a4bbd204df28c167cf7d46022d2b3155568086ac78fdd630c`.
The normal allowlist stayed `neutral`; only the explicit three-cycle suite was
selected with a 75% battery floor, left paw first, and the 35 mm lift.

The baseline used 722 samples and measured 121.912 N total load. Every landing
restored all four support latches:

- cycle 1: left unload/landing `3.901/29.767 N`; right `1.386/29.509 N`;
- cycle 2: left unload/landing `1.849/26.950 N`; right `0.466/29.667 N`;
- cycle 3: left unload/landing `2.085/26.949 N`; right `0.160/29.563 N`.

The session completed with no safety fault and no feedback pause or recovery.
Maximum feedback age/consecutive update gap was `12.291/10.382 ms`. Ownership
was released. Fresh post-run evidence was state/gait/motion `1/0/0`, battery
77%, errors zero, roll/pitch `0.392/0.204 deg`, STOP false, fresh centered axes,
no runner or ownership marker, and four loaded feet at
`24.131/30.575/43.193/38.545 N`.

This closes the repeated canonical-reset commissioning gate. It does not prove
the normal chat-driven selector or a retarget received while a paw is active;
those remain disabled pending a separately authorized supervised live test.

## Implemented commissioning envelope

- Brace: 8 mm body lowering plus 6 mm per-side stance widening over 0.55 s.
- Paw target: 35 mm, below joy's accepted 50 mm target.
- Placement: 0.35 s quintic lowering followed by a stationary 0.25 s landing
  dwell; no requested downward continuation after the paw reaches zero lift.
- Support reset: 1.0 s planted return of body shift, brace, and stance width to
  canonical stand plus a 0.35 s exact hold after every dwell; the unchanged
  four-support gate is evaluated only after that hold.
- Analytic Cartesian caps at 35 mm: 0.1875 m/s downward velocity and about
  1.65 m/s^2 acceleration, below the hard 0.190 m/s and 1.70 m/s^2 limits.
- Support: target-paw unload, two strong non-target supports, at least 70 N
  aggregate non-target load, target landing, and all four supports restored.
- Order: `left` and `right` are both explicit commissioning choices. The full
  suite cannot begin the second paw until the first has passed landing and the
  complete dwell.
- Repetition: explicit suite mode is capped at three cycles. Every cycle repeats
  all unload/landing gates; a failed cycle prevents success and releases after
  bounded recovery.
- Recovery: commissioning loops use the 0.65 s assertive hold and 1.30 s return.
  A chat retarget uses a 1.5 s return plus 0.35 s exact-neutral hold. Checked-in
  `anger` selection remains disabled by default until the cross-emotion
  integration ticket receives its consolidated operator verdict.

## Normal chat integration evidence — 2026-09-20

The first normal repeated run used the earlier 20 mm left rearward support
shift. Its first loop passed both paws, and the next right placement passed,
but the second left unload measured `5.806 N`, about `0.009 N` above the
unchanged baseline-relative threshold. The runner lowered, restored four
supports, and released without weakening a gate.

The correction changed only the left rearward support shift to 25 mm; the right
remains 35 mm and both lateral shifts remain 20 mm. Lift height, velocity,
acceleration, unload, aggregate-support, landing, four-foot relatch, attitude,
tracking, feedback, STOP, and ownership limits are unchanged. The clean local
and aarch64 suites passed before the physical retry.

The revised normal path established a valid 726-sample, `122.465 N` baseline
and completed two consecutive full loops under one owner:

- loop 1: left unload/landing `5.319/29.136 N`, right
  `0.836/26.034 N`;
- loop 2: right `0.327/25.891 N`, left `5.234/28.739 N`.

A third loop also completed left `4.128/28.023 N` and right
`0.235/26.321 N` with four supports. During its final assertive hold, battery
fell from the explicit 75% floor to 74%. The hard battery gate preempted the
pending Disgust retarget and released ownership immediately. Fresh postflight
was state/gait/motion `1/0/0`, errors `0`, four supports, no runner, and no
ownership marker. This is a successful normal Anger selection/repetition result
and a successful battery-interlock result, but not a completed Anger retarget.

The retarget was repeated in an independently acquired session starting at
battery 64% with a valid 728-sample, `124.697 N` baseline and the existing
`HARDWARE_MINIMUM_BATTERY=25` project floor explicitly authorized by the
operator. During normal Anger, Disgust became the pending request. The runner
completed the active paw placement and support restoration, returned over 1.5
seconds, held exact neutral for 0.35 seconds, and activated
`neutral_animal_breath`. Status preserved `requested_emotion=disgust`, reported
`fallback_active=true`, four supports, no fault, and uninterrupted ownership.
The session later released without a safety fault or ownership leak.

## Persistent Anger contact-miss regression and offline correction — 2026-09-20

After the persistent Joy correction was deployed, a normal chat-driven Anger
run reached its first front-left placement but retained `6.24688 N` on the
target paw, so the unchanged unload gate correctly reported
`ANGER_UNLOAD ... confirmed=false`. The runner nevertheless completed the
controlled placement, landing dwell, one-second canonical relatch, and
0.35-second four-foot hold. Landing was confirmed at `28.4243 N`, support count
was four, and the four filtered loads were
`28.4243/23.2969/30.0154/38.4726 N`. It then completed the 1.5-second canonical
recovery but treated the recovered unload miss as terminal and released SDK
ownership. No robot safety fault occurred. One feedback pause recovered within
the existing watchdog; maximum feedback age/update gap was
`149.276/150.027 ms` and maximum pause was `74.0034 ms`.

This is the same policy class as the earlier Joy regression, not permission to
weaken the unload gate. The development runner now returns a distinct recovered
contact-miss result only when normal chat mode has completed controlled
placement and confirmed both the target landing and all four supports. It logs
`ANGER_CONTACT_MISS_RECOVERED`, skips the assertive hold, performs the existing
1.5-second canonical return plus 0.35-second exact-Neutral hold, and resumes
Anger under the same continuous owner when Anger is still current. Explicit
commissioning suites remain fail-closed, and incomplete landing, STOP, stale or
invalid feedback, state failure, tracking failure, estimator failure, and every
other hard fault still release. No motion amplitude, contact threshold, or
safety limit changed.

The correction passes all nine native suites, 29 Python safety/protocol tests,
a clean Release catkin build, and the loopback ownership/watchdog integration
through `make -C lite3-noetic verify-hardware-offline`. After the robot returned
to basic state `1`, a fresh no-motion preflight confirmed gait/motion `0/0`,
battery 74%, errors zero, fresh centered Retroid, STOP false, and no owner. The
clean aarch64 build passed all nine native suites and installed SHA-256
`e72a2db71d1f569b55d5c5af10b82c6b48a6a850513bb71aa3bacd9a84d4e5cd`;
the previous `aa449...` binary remains as a checksum-named backup. Postflight
remained state/gait/motion `1/0/0`, battery 73%, errors zero, STOP false, and no
owner or runner. No motion command was sent, so live revalidation remains open.

## Non-goals

- Maximizing impact force, noise, or floor contact impulse.
- Torque-controlled striking, kicking, or repeated downward drive after landing.
- Using a vendor stomp/action primitive or locomotion gait.
- Calling planted compression alone a completed stomp.
