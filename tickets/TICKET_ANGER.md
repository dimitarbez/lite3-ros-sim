# Physical Anger: Controlled Alternating Front-Paw Stomps

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Surprise](TICKET_SURPRISE.md)

## Status — planned

The desired stomp choreography is specified but is not implemented,
commissioned, or operator-accepted. Earlier planted compression pulses are
prototypes only. This ticket targets real paw unload and controlled placement,
not a forceful impact.

## Goal and emotional read

Anger should read as assertive, deliberate alternating front-paw stomps: lower
the body, transfer support, lift one paw, place it firmly under a bounded
trajectory, recover contact, then mirror the other paw. It must be heavier and
more deliberate than joy, without becoming a jump, gait, kick, or torque strike.

## Proposed five-second loop

Initial targets, subject to offline bounds and physical commissioning:

1. **Brace — 0.55 s:** lower into a broad four-foot stance and bias load rearward.
2. **First transfer/lift — 0.65 s:** transfer away and lift the first front paw
   approximately 35–50 mm.
3. **First controlled place — 0.60 s:** lower with bounded vertical speed;
   confirm landing and dwell 0.25 s before continuing.
4. **Second transfer/lift — 0.65 s:** mirror only after four-foot support is
   restored.
5. **Second controlled place — 0.60 s:** lower, confirm landing, and dwell.
6. **Hold — 0.65 s:** maintain a low assertive four-foot pose.
7. **Recover — 1.30 s:** return to the neutral entrance pose.

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

## Transition behavior

A new emotion request while a paw is raised cannot interrupt the controlled
placement. Complete landing and four-foot dwell, recover the brace, return to
canonical stand over 1.5 seconds, hold exact neutral for 0.35 seconds, and then
start only the newest target. STOP and safety faults retain immediate verified
hold/release behavior.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [ ] Implement brace, alternating lift/place, landing dwell, hold, and recovery.
- [ ] Add anger-specific lift, downward velocity, acceleration, and dwell limits.
- [ ] Reuse contact/load gates under the single official owner.
- [ ] Test both paw orders, combined workspace, joint bounds, finite samples,
  touchdown velocity, acceleration, seams, and exact recovery.
- [ ] Prove the second stomp cannot begin without confirmed landing and dwell.
- [ ] Test retargeting during both raised-paw and placement phases.
- [ ] Wire validated `anger` state through exact neutral.
- [ ] Pass local and aarch64 suites.
- [ ] Commission a bounded single stomp before the full alternating loop.
- [ ] Complete the full physical loop with state, IMU, joint, contact, STOP,
  tracking, feedback, touchdown, ownership, and release evidence.
- [ ] Obtain operator acceptance as anger without harsh impact.

## Acceptance criteria

- Both front paws visibly unload and land one at a time.
- Each landing is decisive but bounded, with no commanded drive after contact.
- Four-foot support is restored between stomps and before recovery.
- The motion reads as anger rather than fast joy taps.
- Any normal emotion change passes through exact neutral.
- No impact fault, slip, body instability, stale continuation, or owner conflict.

## Non-goals

- Maximizing impact force, noise, or floor contact impulse.
- Torque-controlled striking, kicking, or repeated downward drive after landing.
- Using a vendor stomp/action primitive or locomotion gait.
- Calling planted compression alone a completed stomp.
