# Physical Fear: Crouch, Recoil, and Guarded Paw Hovers

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Disgust](TICKET_DISGUST.md) ·
[Next: Surprise](TICKET_SURPRISE.md)

## Status — planned

Fear's intended behavior is specified but not implemented or commissioned. The
older planted profile is a prototype only. No high-frequency tremble or lifted
paw behavior is accepted until its trajectory and live telemetry pass.

## Goal and emotional read

Fear should read as startled self-protection: a quick bounded crouch, rearward
recoil, small guarded shifts, and low alternating front-paw hovers. It must
remain stable and controlled. The expression should be more urgent than sadness
but smaller and less expansive than surprise, with no continuous joint chatter.

## Proposed five-second loop

Initial targets, subject to measured limits:

1. **Flinch — 0.45 s:** quick symmetric compression within tested acceleration
   bounds.
2. **Recoil — 0.65 s:** move body bias rearward while maintaining four contacts.
3. **First guard — 0.55 s:** transfer support and lift one front paw 15–25 mm.
4. **First place — 0.55 s:** lower and confirm four-foot support.
5. **Second guard — 0.55 s:** mirror a small transfer and lift the other front
   paw 15–25 mm.
6. **Second place — 0.55 s:** lower and confirm support.
7. **Freeze — 0.70 s:** hold a low guarded four-foot pose.
8. **Recover — 1.00 s:** return to the neutral entrance pose.

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
- [ ] Implement flinch, recoil, two guarded hovers, freeze, and recovery.
- [ ] Define fear-specific velocity, acceleration, and minimum landing dwell.
- [ ] Reuse contact/load gates under exclusive ownership.
- [ ] Test both paw orders, combined crouch workspace, finite samples, joint
  bounds, seams, and exact recovery.
- [ ] Test that the second hover cannot start without confirmed first landing.
- [ ] Test retargeting in every hover/lower/freeze phase.
- [ ] Wire validated `fear` state through exact neutral.
- [ ] Pass local and aarch64 suites.
- [ ] Complete a bounded physical run with state, IMU, joints, contact, STOP,
  tracking, feedback, and release evidence.
- [ ] Obtain operator acceptance as fear, distinct from sadness and surprise.

## Acceptance criteria

- The initial crouch reads as a flinch without exceeding trajectory bounds.
- Each small paw hover independently unloads and lands before the next begins.
- The frozen guarded pose is stable, not vibrating.
- The loop returns to four-foot support and exact neutral cleanly.
- No slip, impact, gait transition, stale continuation, or owner conflict.

## Non-goals

- High-frequency shaking that can excite unmodeled dynamics.
- Running away, stepping backward, or invoking a gait.
- Reusing surprise at lower amplitude.
- Skipping contact confirmation to create a faster alternation.
