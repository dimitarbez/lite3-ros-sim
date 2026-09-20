# Physical Disgust: Recoil and Paw Withdrawal

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Sadness](TICKET_SADNESS.md) ·
[Next: Fear](TICKET_FEAR.md)

## Status — planned

The motion concept is documented, but the final runner implementation, tests,
normal state integration, physical commissioning, and operator acceptance are
all pending. The existing planted disgust profile is not final evidence.

## Goal and emotional read

Disgust should read as avoidance: a clear rearward and sideways recoil from an
imagined stimulus, withdrawal of the nearer front paw, a guarded pause, and a
controlled return. It must not look like curiosity approaching, affection
offering a paw, fear trembling, or joy alternating rapidly.

## Proposed 4.8-second loop

Initial implementation targets:

1. **Notice — 0.40 s:** brief still preparation from the neutral entrance pose.
2. **Recoil — 0.75 s:** shift the body rearward and away while all four feet
   remain loaded.
3. **Withdraw paw — 0.70 s:** unload and lift one front paw approximately
   25–35 mm, with a small inward retraction that remains inside IK bounds.
4. **Guard — 0.85 s:** hold the recoiled asymmetric pose without oscillation.
5. **Replace — 0.75 s:** reverse the retraction, lower, and confirm landing.
6. **Release recoil — 1.35 s:** restore four-foot symmetry and return to the
   neutral entrance pose.

Choose the withdrawal side deterministically from loop parity unless a future
validated stimulus direction is explicitly added. Do not infer direction from
untrusted chat text on the robot computer.

## Contact and motion requirements

- Use joy's four-foot baseline, target unload, two-strong-support/70 N support,
  landing, and restored-contact gates.
- Test the combined rearward, lateral, vertical, and inward paw target in the
  full Cartesian leg solver.
- The recoil must be body pose only: zero world-frame locomotion and no vendor
  backward step.
- Keep commanded yaw and torque feed-forward zero.
- A failure to unload or land completes safe lowering/recovery before failure.

## Transition behavior

Retargeting during withdrawal or guard queues only the newest category. The
runner first replaces the paw and releases the recoil, then returns to canonical
stand over 1.5 seconds, holds exact neutral for 0.35 seconds, and starts the new
emotion. STOP/fault paths remain immediate.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [ ] Implement recoil, paw retraction, guard, replacement, and recovery phases.
- [ ] Add parameters for side, body recoil, lift, inward withdrawal, and timing.
- [ ] Reuse contact/load gates under the single MotionSDK owner.
- [ ] Test left/right Cartesian workspace, joint bounds, velocity,
  acceleration, seams, and exact recovery.
- [ ] Test that pose recoil produces no planar command or action request.
- [ ] Add retargeting tests for every raised-paw phase.
- [ ] Wire validated `disgust` state through exact neutral.
- [ ] Pass local and aarch64 test suites.
- [ ] Complete a bounded physical telemetry run and clean release.
- [ ] Obtain operator acceptance as disgust/avoidance, not fear or curiosity.

## Acceptance criteria

- Recoil direction and withdrawn paw form one coherent asymmetric gesture.
- Unload, stable support, landing, and four-foot recovery are confirmed.
- World pose does not intentionally translate or rotate.
- The guarded pause is readable without balance chatter.
- Direct emotion changes visibly pass through exact neutral.

## Non-goals

- A walking backward step or avoidance locomotion.
- Using yaw twist to simulate turning away.
- Fast repeated paw movement that reads as fear or joy.
- Selecting a stimulus direction without a versioned validated contract.

