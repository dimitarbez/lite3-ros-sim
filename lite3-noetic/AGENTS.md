# Lite3 Noetic wrapper guide

This file scopes work under `lite3-noetic/`. The workspace-root `AGENTS.md` remains authoritative for repository ownership and the physical-hardware boundary. A deeper `ws/Lite3_VMC/AGENTS.md` governs maintained catkin source.

## Role of this subtree

- This directory owns the Docker images, persistent development-container lifecycle, Make targets, operational documentation, live OpenAI sidecar, workspace bootstrap, and the separately deployed split-host hardware workspace.
- Run public workflows through `make -C lite3-noetic <target>` from the workspace root. Keep ROS Noetic and its build dependencies inside Docker.
- `ws/Lite3_VMC` and the sibling `../emotion-bot` are separate pinned repositories. The wrapper mounts the former read-write and the latter read-only at `/workspaces/emotion-bot`.
- Never edit `ws/Lite3_VMC/build`, `devel`, or `log`; they are generated and may contain stale evidence from earlier builds.

## Documentation map

- `README.md` is the short setup, run, architecture, verification, and upstream-baseline entry point.
- `docs/USAGE.md` is the terminal-by-terminal operator path and must include readiness, enable/re-enable, telemetry, safe shutdown, and offline alternatives.
- `docs/WORKFLOW.md` records architecture and operational semantics.
- `docs/TROUBLESHOOTING.md` owns symptom-based recovery instructions.
- `docs/VERIFICATION.md` is a dated evidence record; update it only with results actually rerun on a named checkout.
- `docs/HARDWARE_APP_CONTROL.md` is a dated hardware investigation and bounded experiment record. It does not authorize wrapper targets or scripts that transmit to a robot.
- `hardware-ws/README.md` owns the current split-host runtime, interfaces, installation boundary, safety gates, and commissioned-versus-experimental behavior.
- `../tickets/README.md` and the linked per-emotion tickets own current physical-expression acceptance state and remaining implementation work. Keep those statuses evidence-based.
- `ws/Lite3_VMC/src/emotion_bot_ros/README.md` is the detailed package contract for nodes, topics, schemas, mappings, safety, and tests.

When behavior changes, update every affected layer rather than leaving contradictory commands or defaults. Avoid hard-coded submodule SHAs in evergreen prose; if reproducibility requires a SHA, derive it from the current superproject gitlink and label the date/result.

## Runtime rules

- `run-emotion-sim-openai` is the normal live path; `run-emotion-sim` is the deterministic offline path. Both use the same integrated Gazebo/controller/safety graph.
- `run-emotion-chat` is only the terminal client. Trust its reported `chat_backend`; `emotion_backend=deterministic` describes appraisal, not whether the assistant reply used OpenAI.
- The OpenAI sidecar is disposable, loopback-only, Python 3.12, and separate from ROS Noetic. Pass the secret only as the `OPENAI_API_KEY` environment variable; never put its value in a command argument, image, ROS parameter, log, or tracked file.
- The local `.env` belongs at `ws/Lite3_VMC/.env`, remains ignored, and should be mode `600`.
- Keep exactly one ROS/Gazebo graph. XML-RPC connection refusal usually means stale or overlapping processes; use `make -C lite3-noetic restart`, rebuild if needed, and launch one graph.
- For an interactive simulation handoff, `/emotion_bot/status` must report `sim_ready`, `health_ok`, `stance_prepared`, and `motion_enabled` as true. Recover and re-enable after a transient watchdog instead of leaving motion disabled.
- Normal chat expressions remain centered. The explicit locomotion target is only for supervised simulator development.

## Physical-expression rules

- Hardware targets are separately named and never part of a simulation target. Running setup, a hardware target, SSH/tunnel commands, packet capture, or any physical commissioning requires explicit authorization in the current task.
- `hardware-ws` is a separate catkin workspace for the perception computer. Never install it into or edit vendor `~/lite_cog`, `qnx2ros`, `jy_exe`, or their configuration.
- The official runtime starts only from exact robot state `1`, performs the proven vendor stand sequence once, and keeps one `motion_sdk_expression_runner` as the sole 1 kHz MotionSDK owner. The Retroid bridge and legacy posture/action graph remain fail-closed diagnostic-only alternatives and may not coexist with it.
- Keep emotional reasoning and the OpenAI/deterministic chat backend on the development computer. Only validated schema-1.1 state crosses the loopback SSH tunnel; the perception-computer runner alone converts it to joint trajectories.
- Neutral is the only checked-in normal commissioned category. The accepted 50 mm alternating front-paw joy trajectory currently runs only through the explicit bounded joy suite; normal chat selection and mid-lift retargeting are unfinished. Treat the other seven final profiles as planned and the older planted profiles as prototypes/fallbacks.
- Any lifted-paw profile must use tested Cartesian IK plus torque-derived baseline, unload, support, landing, and restored-contact gates. On missed evidence it must still lower and settle before failure. Do not substitute the all-zero SDK contact array, an open-loop joint offset, a vendor action, or a second sender.
- Category changes must finish any active paw recovery, return to canonical stand over 1.5 seconds, hold exact stand for 0.35 seconds, and enter only the newest pending category. STOP, invalid state, nonzero errors, dead feedback, tracking faults, and watchdog faults retain immediate hold/release behavior.
- `/emotion_bot/hardware/expression_status` is read-only evidence. Do not infer physical success from publication, joint commands, offline tests, or a changed requested emotion; require the ticket's telemetry and operator-acceptance evidence.

## Bootstrap and repository safety

- Inspect `git status` in the wrapper, `emotion-bot`, active `Lite3_VMC`, and upstream reference before setup or update commands.
- `bootstrap` preserves dirty and detached active workspaces. It may fetch and fast-forward a clean attached Lite3 branch, repairs the Noetic catkin toplevel symlink, runs rosdep, and builds.
- Do not use `bootstrap` as a substitute for deliberate submodule updates. Commit nested changes in their repositories before updating the wrapper gitlink.
- Keep `lite3_vmc_upstream` clean and untouched.

## Editing shell, Docker, and Make workflows

- Keep scripts non-interactive unless a target explicitly documents a real TTY requirement. The upstream `run-spawn` target is intentionally interactive.
- Preserve cleanup traps for the OpenAI sidecar and integrated launch. Shutdown and failure paths must converge to zero/neutral command and terminate child processes.
- Do not expose a real-hardware executable, UDP bridge, robot address, or packet transmitter through normal simulation targets.
- Preserve the hardware ownership marker, independent crash watchdog, authenticated STOP path, state/error interlock, two-stage feedback watchdog, battery/attitude/tracking bounds, and deterministic release. Do not weaken a gate to make a commissioning run proceed.
- Preserve WSLg mounts and host-network ROS behavior unless a verified replacement covers GUI, audio, loopback sidecar access, and ROS discovery.
- Use clear Make target help text whenever a target is added or its behavior changes.

## Verification

For documentation or wrapper-only changes, at minimum run:

```bash
make -C lite3-noetic help
bash -n lite3-noetic/scripts/*.sh lite3-noetic/docker/*.sh
git diff --check
```

For source, launch, runtime, or image changes, choose the relevant targets from:

```bash
make -C lite3-noetic emotion-static-checks
make -C lite3-noetic emotion-clean-build
make -C lite3-noetic emotion-unit-tests
make -C lite3-noetic emotion-openai-bridge-test
make -C lite3-noetic emotion-ros-tests
make -C lite3-noetic emotion-gazebo-test
make -C lite3-noetic emotion-animation-gazebo-test
make -C lite3-noetic verify-emotion
```

Ordinary verification is offline and must not require a key, internet, GUI, microphone, physical robot, or paid request. `emotion-openai-live-smoke` is separate, explicit, and metadata-only.

For any `hardware-ws`, hardware script, launch, protocol, runner, IK, contact-estimator, or expression change, also run:

```bash
make -C lite3-noetic verify-hardware-offline
```

That target is offline evidence only. It does not commission a profile, prove movement, validate contact on the current robot, or authorize a live run. For physical-expression status changes, update the relevant ticket and dated evidence only from the exact run actually observed.
