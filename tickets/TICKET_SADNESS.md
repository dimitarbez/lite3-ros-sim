# Physical Sadness: Lowered Slow Posture

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Curiosity](TICKET_CURIOSITY.md) ·
[Next: Disgust](TICKET_DISGUST.md)

## Status — planned

The final physical sadness animation is not implemented or commissioned. Its
older planted profile remains useful as a prototype, but does not satisfy this
ticket's distinctness, contact evidence, transition, or operator-acceptance
requirements.

## Goal and emotional read

Sadness should read as low energy and withdrawn: a long body lowering, slight
front-body droop, one slow low-clearance paw withdrawal/hover, and a delayed
return. It must remain controlled and stable, without joy's bounce, fear's
quick recoil, or affection's inviting paw presentation.

## Proposed six-second loop

Initial targets, subject to offline and live tuning:

1. **Sink — 1.20 s:** slowly compress all legs, with a small front-biased
   lowering and subdued asymmetric roll.
2. **Withdraw — 1.00 s:** transfer support away from one front paw while
   remaining low.
3. **Low hover — 0.80 s:** unload and lift that paw only 15–25 mm.
4. **Pause — 1.00 s:** hold the low withdrawn pose with minimal motion.
5. **Place — 0.80 s:** lower gently and confirm landing.
6. **Recover — 1.20 s:** restore symmetric support and return slowly to the
   neutral entrance pose.

The selected side may alternate between complete loops. The low posture must
stay above the runner's commissioned workspace and joint margins.

## Contact and motion requirements

- Establish a stable four-foot baseline before the withdrawal phase.
- Reuse joy's unload, strong-support, aggregate-load, landing, and four-foot
  recovery logic.
- Validate combined lowering plus paw IK as one Cartesian workspace problem;
  do not simply add offsets that individually pass but jointly exceed bounds.
- Use lower velocities and accelerations than joy for every phase.
- Keep world-frame velocity, yaw, gait/action, and torque feed-forward zero.

## Transition behavior

A new category during the low hover first completes paw placement and safe
four-foot recovery, then performs the global 1.5-second canonical return and
0.35-second exact-neutral hold. Same-category updates wait until the next loop.
STOP and safety faults retain immediate verified release behavior.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [ ] Implement slow sink, withdrawal, low-hover, placement, and recovery phases.
- [ ] Define tested lower-body and paw-lift workspace limits.
- [ ] Reuse the contact estimator and support/landing gates.
- [ ] Add combined crouch-plus-lift IK and trajectory tests for both sides.
- [ ] Verify finite samples, joint margin, low velocity/acceleration, continuous
  seams, tracking bound, and exact recovery.
- [ ] Add same-category and mid-hover retargeting tests.
- [ ] Wire validated `sadness` state through exact neutral.
- [ ] Pass local and aarch64 suites.
- [ ] Complete bounded physical telemetry and clean release.
- [ ] Obtain operator acceptance as sadness, distinct from neutral breathing.

## Acceptance criteria

- The robot visibly lowers and becomes still enough to read as low energy.
- Any paw hover is confirmed but restrained and lands gently.
- The pose is recognizably different from neutral without approaching sit.
- All transitions reach exact neutral before the next emotion.
- No bounce, slip, support loss, fault, or residual motion occurs.

## Non-goals

- Sitting, lying down, or invoking the vendor sit transition as choreography.
- Merely slowing the neutral breathing loop.
- Dropping body height without joint/workspace validation.
- Interpreting a low pose alone as operator-accepted sadness.
