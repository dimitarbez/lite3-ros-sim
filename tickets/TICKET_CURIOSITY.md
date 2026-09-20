# Physical Curiosity: Asymmetric Lean and Paw Hover

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Affection](TICKET_AFFECTION.md) ·
[Next: Sadness](TICKET_SADNESS.md)

## Status — planned

The motion concept is defined; implementation, tests, normal state selection,
physical commissioning, and operator acceptance remain open. The prior planted
curiosity profile is a prototype only.

## Goal and emotional read

Curiosity should look like the robot noticed something and is cautiously
investigating it: one asymmetric body lean, a single front-paw hover, a clear
still moment, then a measured return. Since the robot has no actuated head, the
lean direction, pause, and side alternation must replace a canine head tilt.

It must be more alert and asymmetric than affection, slower and less repetitive
than joy, and neither crouched nor trembling like fear.

## Proposed five-second loop

Initial targets, subject to offline bounds and physical commissioning:

1. **Orient — 0.65 s:** rise slightly from neutral and bias roll/pitch toward
   the side of interest while keeping all four contacts.
2. **Transfer — 0.75 s:** shift load away from the selected front paw.
3. **Investigate — 0.70 s:** lift the selected paw approximately 25–35 mm.
4. **Observe — 1.10 s:** hold the asymmetric pose nearly still; do not add joy
   bouncing or affection sway.
5. **Place — 0.75 s:** lower and confirm the selected paw landed.
6. **Resolve — 1.05 s:** restore symmetric four-foot support and return to the
   neutral entrance pose.

Alternate the side only after a complete loop. A repeated same-category update
must not interrupt the observe phase or reset its timer.

## Contact and motion requirements

- Use the proven joy baseline, unload/landing hysteresis, strong-support count,
  aggregate support, and recovery gates.
- Generate roll-support transfer and paw hover with Cartesian IK inside tested
  HipX/HipY/knee limits.
- Preserve zero planar velocity, yaw, gait/action command, and torque
  feed-forward.
- Ensure the still observe phase has bounded tracking noise and does not become
  high-frequency balance chatter.
- If unload is not confirmed, lower and settle before failing the loop.

## Transition behavior

If another category arrives while the paw is raised, retain only the newest
pending category, complete place and resolve, return to exact canonical stand
over 1.5 seconds, hold neutral for 0.35 seconds, then enter the new profile.
STOP and safety faults use immediate hold/release instead.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [ ] Implement the alternating-side curiosity state machine.
- [ ] Add tunable lean, lift, observe, placement, and recovery parameters.
- [ ] Reuse contact/load gates and exclusive MotionSDK ownership.
- [ ] Test both side variants for IK workspace, joint bounds, finite samples,
  velocity/acceleration, seams, and exact recovery.
- [ ] Test side alternation only after complete loops and no restart on
  same-category updates.
- [ ] Test retargeting during transfer, hover, observe, and placement.
- [ ] Wire validated `curiosity` state through the neutral transition.
- [ ] Pass local and aarch64 suites.
- [ ] Complete a bounded live run with synchronized state, joint, IMU, contact,
  STOP, tracking, and ownership evidence.
- [ ] Obtain operator acceptance as curiosity rather than affection or fear.

## Acceptance criteria

- One paw visibly unloads, hovers, and lands per loop with confirmed support.
- The body holds a readable asymmetric “investigating” pose.
- Alternating loops mirror correctly without left/right drift.
- Direct category changes pass through exact neutral.
- No step, slip, chatter, fault, stale continuation, or ownership conflict.

## Non-goals

- Claiming a head tilt that the mechanism cannot perform.
- Reusing affection with a shorter hold or joy with one omitted paw.
- Turning or translating toward an object.
- Enabling untested sensor-driven object following.

