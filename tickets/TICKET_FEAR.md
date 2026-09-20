# Physical Fear: Crouch, Recoil, and Guarded Paw Hovers

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Disgust](TICKET_DISGUST.md) ·
[Next: Surprise](TICKET_SURPRISE.md)

## Status — first physical candidate rejected; redesign failed closed

Fear's final contact-gated state machine, explicit commissioning modes, normal
state selection, newest-request retargeting, and deterministic bounds/tests are
implemented. The complete local hardware gate and all eight suites on the
robot's aarch64 perception computer pass. The installed runner has
SHA-256
`fb3eea752936de30481fb601e3df5e348613a64a9cd691866e539e00587b4797`.

The first 25 mm physical candidate failed unload at `9.64 N`, then reproduced
the same result because an intended transfer edit had not reached the Fear
block. The correctly applied 30 mm transfer passed one bounded left-paw run at
`5.99 N`, but the complete suite's next baseline left `6.53 N` and failed closed
before the second paw. Every run restored four supports, recovered, released
ownership, and reported no safety fault or feedback pause. The operator also
reported that the observed choreography looked nothing like fear, so the first
candidate is visually rejected regardless of its safe recovery.

The replacement kept the five-second deadline and 25 mm guards but made the
whole-body expression conspicuous: a faster 22 mm flinch, sustained 30 mm
rearward recoil, 18 mm guarded crouch, 8 mm stance widening, symmetric 35 mm
support transfers, longer low freeze, and slower recovery. Local and aarch64
tests passed. In its bounded left-paw run the 725-sample baseline measured
123.871 N, but front-left force remained 7.264 N with all four feet latched, so
the unload gate failed. Landing restored four supports at 18.876 N; exact
recovery and ownership release completed with no safety fault or feedback
pause. The complete two-paw loop was not attempted. The operator confirmed
that the configured 25% battery floor is acceptable. The normal allowlist
remains `neutral`.

At the operator's request, a separate all-feet-planted visual diagnostic then
ran Fear for exactly 15 seconds without bypassing the failed unload gate. It
completed three five-second flinch/recoil/cower/freeze/recovery cycles, restored
four supports after every cycle, and released cleanly. A 71.017 ms feedback
pause occurred and recovered during the stand hold before the Fear window;
there was no safety fault. Final state was `1/0/0`, battery 60%, errors zero,
four supports, no runner, and no owner. This proves the planted 15-second
diagnostic, not the alternating-paw acceptance criterion; operator visual
acceptance remains open.

## Goal and emotional read

Fear should read as startled self-protection: a quick bounded crouch, rearward
recoil, small guarded shifts, and low alternating front-paw hovers. It must
remain stable and controlled. The expression should be more urgent than sadness
but smaller and less expansive than surprise, with no continuous joint chatter.

## Proposed five-second loop

Initial targets, subject to measured limits:

1. **Flinch — 0.35 s:** quick symmetric compression within tested acceleration
   bounds.
2. **Recoil — 0.55 s:** move body bias rearward while maintaining four contacts.
3. **First guard — 0.55 s:** transfer support and lift one front paw 15–25 mm.
4. **First place — 0.55 s:** lower and confirm four-foot support.
5. **Second guard — 0.55 s:** mirror a small transfer and lift the other front
   paw 15–25 mm.
6. **Second place — 0.55 s:** lower and confirm support.
7. **Freeze — 0.75 s:** hold a low guarded four-foot pose.
8. **Recover — 1.15 s:** return to the neutral entrance pose.

The loop repeats only after the full recovery. It must never alternate feet at
a rate that prevents confirmed landing between hovers.

## Contact and motion requirements

- Reuse the joy estimator, distinct-tick filtering, unload/landing hysteresis,
  strong-support, aggregate-load, and four-foot restoration gates.
- Apply explicit lower velocity/acceleration ceilings to the guarded hovers;
  urgency comes from phase timing and posture, not uncontrolled impact.
- Test combined crouch, rearward transfer, and each low lift in the IK workspace.
- Keep planar velocity, yaw, action/gait, torque feed-forward, and external
  wrench commands zero.
- Do not implement tremble as high-frequency motor oscillation.

## Transition behavior

A category change during a hover completes that paw's placement and safe loop
recovery, then performs the mandatory 1.5-second canonical return and
0.35-second exact-neutral hold. Rapid retargeting replaces the pending category
without restarting recovery. STOP and faults remain immediate.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [x] Implement flinch, recoil, two guarded hovers, freeze, and recovery.
- [x] Define fear-specific velocity, acceleration, and minimum landing dwell.
- [x] Reuse contact/load gates under exclusive ownership.
- [x] Test both paw orders, combined crouch workspace, finite samples, joint
  bounds, seams, and exact recovery.
- [x] Test that the second hover cannot start without confirmed first landing.
- [x] Test retargeting in every hover/lower/freeze phase.
- [x] Wire validated `fear` state through exact neutral.
- [x] Pass local and aarch64 suites.
- [ ] Complete a bounded physical run with both unload gates, state, IMU,
  joints, contact, STOP, tracking, feedback, and release evidence. The
  redesigned single-paw run failed closed at the first unload gate.
- [ ] Obtain operator acceptance as fear, distinct from sadness and surprise.

## Acceptance criteria

- The initial crouch reads as a flinch without exceeding trajectory bounds.
- Each small paw hover independently unloads and lands before the next begins.
- The frozen guarded pose is stable, not vibrating.
- The loop returns to four-foot support and exact neutral cleanly.
- No slip, impact, gait transition, stale continuation, or owner conflict.

## Implemented commissioning envelope

- Five-second loop: `0.35 s` flinch, `0.55 s` recoil, two
  `0.55 s` guard/`0.55 s` place pairs, `0.75 s` guarded freeze, and
  `1.15 s` exact recovery.
- Candidate paw target: 25 mm, with a checked range of 15–25 mm.
- Crouch/recoil: 22 mm initial symmetric compression, 18 mm guarded crouch,
  30 mm sustained rearward recoil, and 8 mm stance widening. Both paw guards
  use the previously tested 35 mm rearward support-transfer envelope.
- Placement: 0.35-second quintic lowering while returning to the guarded
  four-foot recoil pose, followed by a stationary 0.20-second landing dwell.
- Analytic 25 mm touchdown maxima: approximately `0.134 m/s` and
  `1.178 m/s²`, below Fear's `0.135 m/s` and `1.20 m/s²` limits and both below
  the accepted Anger envelope.
- Each hover requires target unload, two strong non-target supports, at least
  70 N aggregate non-target load, target landing, and all four support latches
  before the mirrored hover can start.
- A chat retarget finishes any current placement, suppresses later hovers,
  returns to canonical stand over 1.5 seconds, holds exact neutral for
  0.35 seconds, and starts only the newest commissioned category. STOP and
  hard safety faults still release immediately.
- Explicit commissioning controls are `HARDWARE_FEAR_SINGLE_HOVER_TEST`,
  `HARDWARE_FEAR_SUITE_TEST`, `HARDWARE_FEAR_FIRST_PAW=left|right`, and
  `HARDWARE_FEAR_LIFT_METERS=0.015..0.025`. Fear remains excluded from the
  checked-in normal allowlist until live evidence and operator acceptance pass.
- `HARDWARE_FEAR_BODY_VISUAL_TEST=true` is a separate 15-second diagnostic:
  three exact five-second cycles with all paws planted. Each cycle uses the
  bounded flinch/recoil/crouch envelope, five 0.45-second 8 mm lateral cower
  transitions, a 0.70-second freeze, and 1.15-second exact recovery. It never
  enables or substitutes for the paw-unload path.

## Non-goals

- High-frequency shaking that can excite unmodeled dynamics.
- Running away, stepping backward, or invoking a gait.
- Reusing surprise at lower amplitude.
- Skipping contact confirmation to create a faster alternation.
