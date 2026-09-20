# Physical Sadness: Lowered Slow Posture

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Curiosity](TICKET_CURIOSITY.md) ·
[Next: Disgust](TICKET_DISGUST.md)

## Status — planted bow accepted; normal chat integration pending

The six-second paw path and a separate planted visual path are implemented in
the official continuous MotionSDK runner. The paw candidate failed front-left
unload at `8.875 N`. Three planted iterations then passed telemetry but were
visually rejected as too static, insufficiently animated, or insufficiently
bowed. A 70 mm front-to-rear bow was visibly too deep and tripped the hard
`10 deg` attitude gate at `10.006 deg`; it aborted and released safely before
the animated phases. The 56 mm midpoint also reached `10.005 deg` during its
sink. The final front-only 48 mm bow removed rear extension, passed a single
physical run, and was visually accepted by the operator. A bounded two-cycle
`15.6 s` repeat also passed. Normal chat selection remains pending, so the
normal allowlist stays `neutral`.

## Goal and emotional read

Sadness should read as low energy and withdrawn: a long body lowering, slight
front-body droop, one slow low-clearance paw withdrawal/hover, and a delayed
return. It must remain controlled and stable, without joy's bounce, fear's
quick recoil, or affection's inviting paw presentation.

## Proposed six-second loop

Implemented targets, still subject to live acceptance and bounded tuning:

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

The current candidate uses a 20 mm common crouch, 5 mm additional front droop,
2 mm support-side roll bias, 4 mm widened stance, and a 20 mm paw hover. The
lift is restricted to `0.015..0.025 m`. Left/right support transfers retain the
previously calibrated `20/35 mm` rearward asymmetry and `20 mm` lateral shift.
The fastest paw segment is the 0.55-second lower: its analytic maximum is about
`0.0682 m/s` and `0.3817 m/s²`, below the Sadness caps and the accepted Joy
suite's 50 mm in 0.60 seconds.

The accepted visual diagnostic is deliberately planted and does not bypass the
failed paw-unload gate. It enters a front-only 48 mm bow over `1.50 s` while
the rear corners remain at neutral. After a
`0.50 s` settle it performs three visible crying heaves, each raising the front
14 mm with an alternating 4 mm side tilt and dropping back over `0.60 s`. It
holds the deep bow for `1.00 s` and recovers exactly over `1.50 s`, for a total
of `7.80 s`. Every corner stays within the 50 mm Cartesian workspace and the
analytic speed/acceleration envelope.

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
- [x] Implement slow sink, withdrawal, low-hover, placement, and recovery phases.
- [x] Define tested lower-body and paw-lift workspace limits.
- [x] Reuse the contact estimator and support/landing gates.
- [x] Add combined crouch-plus-lift IK and trajectory tests for both sides.
- [x] Verify finite samples, joint margin, low velocity/acceleration, continuous
  seams, tracking bound, and exact recovery.
- [x] Add same-category and mid-hover retargeting tests.
- [x] Wire validated `sadness` state through exact neutral.
- [x] Pass local and aarch64 suites.
- [x] Complete bounded planted-bow telemetry runs and clean release.
- [x] Redesign without rear extension and physically pass the attitude gate.
- [x] Pass a bounded two-cycle 15.6-second repeat.
- [ ] Rework and pass the optional withdrawn-paw unload gate before enabling
  that variant.
- [x] Obtain operator acceptance as sadness, distinct from neutral breathing.
- [ ] Select the accepted planted bow from normal validated `sadness` state and
  live-test retargeting before changing the allowlist, as tracked by the
  [chat-driven physical emotion integration ticket](TICKET_PHYSICAL_EMOTION_CHAT.md).

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

## Physical evidence — 2026-09-20

The first authorized single-hover run established a 717-sample, `121.799 N`
baseline and completed every commanded phase, but the front-left foot retained
`8.875 N` and all four support latches. The unload gate therefore failed
closed. Landing and recovery restored four supports at
`30.921/27.540/39.228/44.443 N`; one `129.162 ms` feedback pause recovered,
SDK ownership released, and the post-run robot state was `1/0/0` with battery
`52%`, errors zero, and STOP false. The operator rejected its visual read.

The revised planted bow used runner SHA-256
`65c4b168badefd9534ad03612c06055edc607de69a2b9c7eef84bc2274caffbf`.
Fresh preflight was state `1/0/0`, battery `50%`, errors zero, level attitude,
fresh centered Retroid input, STOP false, and no owner. The 732-sample baseline
was `122.757 N`. Four supports remained latched in the low pose at
`26.917/27.331/23.078/27.284 N`; the 3.60-second still hold and exact recovery
completed with no feedback pause or safety fault. Release succeeded, and fresh
postflight was state `1/0/0`, battery `49%`, errors zero, STOP false, four
loaded supports, and no owner. This closes the planted-bow telemetry gate only;
the operator rejected it as too subtle.

Two more planted redesigns completed cleanly. The first added two small sobbing
nods; the operator still found it insufficiently expressive. The next used a
48 mm front lowering with only 4 mm rear lowering plus three 14 mm heaves and
alternating 4 mm side tilt. Its 728-sample baseline was `121.714 N`; every
phase retained four supports, no feedback pause or safety fault occurred, and
postflight was `1/0/0`, battery `45%`, errors zero, STOP false, and no owner.
The operator found the animation improved but requested a deeper bow.

The subsequent 70 mm differential candidate kept the front at its 50 mm
workspace limit and extended the rear 20 mm. From a fresh state-`1` preflight,
its 703-sample baseline was `124.287 N`. During the initial sink, pitch reached
`10.006 deg` and the hard 10-degree attitude gate aborted the phase. The sobbing
animation was never entered. The runner released without retry; fresh
postflight was `1/0/0`, battery `44%`, errors zero, STOP false, four supports,
and no owner. The operator reported that this partial bow was too deep.

The 56 mm midpoint used rear corners 8 mm above neutral and front corners
48 mm below, with the same three heaves. Local and all nine aarch64 suites
passed; installed runner SHA-256 was
`0a1de0dfd88c8dad91a3f1797a8f666b2fdb432ed8335eab41187f462fdfeefc`.
Fresh preflight was `1/0/0`, battery `43%`, errors zero, centered controls,
STOP false, and no owner. Its 723-sample baseline measured `119.855 N`. During
the initial sink, a `100.646 ms` feedback-age event paused and recovered after
20 fresh samples in `83.021 ms`. The motion then reached `10.005 deg` pitch and
the hard attitude gate aborted before any heave. All four supports remained
latched at `29.620/30.151/28.136/28.834 N`; release completed without retry.
Fresh postflight was `1/0/0`, battery `42%`, errors zero, STOP false, four
supports, and no owner. The corrected status retained `sadness body visual
phase aborted by a live safety gate` as the primary fault.

The final candidate removed rear extension and lowered only the front corners
by 48 mm. Its first physical run used a 711-sample, `123.280 N` baseline. The
low pose retained four supports at `34.010/31.170/25.206/29.567 N`; all three
heaves and exact recovery passed with no feedback pause or safety fault. Fresh
postflight was `1/0/0`, battery `41%`, errors zero, STOP false, four supports,
and no owner. The operator reported that it looked good.

The unchanged animation then ran twice under one continuous owner for
`15.6 s`. Runner SHA-256 was
`7c2980b02b432a62d18c546853574a36efb4f0b4add91536836f4f0fdb4e648a`.
The 744-sample baseline measured `122.305 N`. Both low poses retained four
supports, at `34.128/31.223/26.030/28.745 N` and
`34.119/31.841/24.112/28.582 N`, and all six heaves passed. One `100.208 ms`
feedback-age pause during the second recovery held safely and resumed after 20
fresh samples in `74.994 ms`. Maximum feedback age/update gap was
`147.213/147.911 ms`; no safety fault occurred. Fresh postflight was `1/0/0`,
battery `39%`, errors zero, STOP false, four supports, status cycle `2`, and no
owner.
