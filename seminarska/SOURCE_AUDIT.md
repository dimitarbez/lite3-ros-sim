# Source audit

Овој документ ја бележи provenance-врската меѓу поголемите констатации во
семинарската и проверените извори. Референтниот пристап е направен на
2026-09-20. Датираните verification записи се историски доказ за наведениот
checkout/услов, а не општа гаранција за иден checkout.

## Референтни Git верзии и јавни URL-адреси

| Улога | URL / локален извор | Референтна верзија |
| --- | --- | --- |
| Wrapper и репродуцибилен workspace | <https://github.com/dimitarbez/lite3-ros-sim/tree/feature/emotion-robot-integration> | `c41f26ef4414e5c9a4748f3c88aee33820526c01` |
| EmotionBot | <https://github.com/dimitarbez/emotion-bot/tree/feature/emotion-robot-integration> | `f267d28552bbf46f30a263bb291e71ab91a2b91b` |
| Lite3_VMC fork | <https://github.com/dimitarbez/Lite3_VMC/tree/feature/emotion-robot-integration> | `785a3846ec2c44a0380618490f837786b3250f76` |
| Upstream comparison | <https://github.com/DeepRoboticsLab/Lite3_VMC> | local clean reference `724aa54dee0cc374bd5417c86d649fb08d4d309a` |
| Official MotionSDK | <https://github.com/DeepRoboticsLab/Lite3_MotionSDK> | official upstream; robot-side checkout is recorded separately in dated evidence |

HTTP status `200` and the three feature-branch remote SHA values were checked on
2026-09-20. The local wrapper and both maintained submodules were clean at the
requested commits before authoring.

## Architecture, environment and reproduction

| Claim area | Primary local sources |
| --- | --- |
| WSL2/Ubuntu 24.04 host, Ubuntu 20.04 Docker, Noetic/Gazebo workflow | `lite3-noetic/README.md`; `lite3-noetic/docs/USAGE.md`; `lite3-noetic/docker/`; `lite3-noetic/Makefile` |
| Wrapper/submodule ownership and clean upstream reference | root `README.md`; `.gitmodules`; root and nested `AGENTS.md`; live `git submodule status` |
| Build/run/test targets | `lite3-noetic/Makefile`; `lite3-noetic/docs/USAGE.md`; `lite3-noetic/docs/WORKFLOW.md` |
| Reproduction and recovery limitations | `lite3-noetic/docs/TROUBLESHOOTING.md`; `lite3-noetic/README.md` |

## EmotionBot, chat and ROS contracts

| Claim area | Primary local sources |
| --- | --- |
| Nine categories; valence/arousal bounds; deterministic events | `emotion-bot/emotional_core/`; `emotion-bot/tests/`; `emotion-bot/README.md`; `emotion-bot/README.mkd` |
| Headless `EmotionEngine` boundary and GPL-3.0 attribution | `emotion-bot/emotional_core/engine.py`; `emotion-bot/LICENSE`; `emotion-bot/AGENTS.md` |
| Nodes, topics, services and schema 1.0/1.1 | `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/README.md`; `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/config/default.yaml`; `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/src/emotion_bot_ros/contract.py`; launch files |
| Turn ordering, cancellation, retry/fallback and correlation fix | `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/src/emotion_bot_ros/conversation.py`; `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/scripts/chat_adapter.py`; `lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/scripts/emotion_chat.py`; package tests; `lite3-noetic/docs/VERIFICATION.md` |
| OpenAI Python 3.12 sidecar, model/config and secret handling | `lite3-noetic/openai-runtime/` where present; wrapper scripts/Makefile; package README/config; `docs/USAGE.md`; OpenAI official streaming guide |

## Gazebo mapping and safety

| Claim area | Primary local sources |
| --- | --- |
| `/emotion_bot/joy_out` is the sole simulator input; `/cmd_vel` not instantiated | package README; `integrated_sim.launch`; `src/quadruped` executable/source inspection; `docs/VERIFICATION.md` baseline |
| All nine declarative profiles and exact amplitudes/timings | `emotion_bot_ros/config/default.yaml`; `emotion_bot_ros/src/emotion_bot_ros/mapping.py` |
| Hop/stomp implementation and no external wrench | `src/quadruped/include/quadruped/controllers/qr_expression_trajectory.hpp`; controller source/tests; package README |
| Readiness, arbitration, timeouts, clamps, anti-splay and recenter | `emotion_bot_ros/src/emotion_bot_ros/safety.py`; `scripts/safety_bridge.py`; default config; integration/Gazebo tests |
| Build/test counts, measured idle excursions, drift and visual checks | `lite3-noetic/docs/VERIFICATION.md`, sections “Latest verified results” and “Cartoon-expression Gazebo sweep” |

## Physical architecture and protocol

| Claim area | Primary local sources |
| --- | --- |
| Split-host layout, services, permissions, loopback NDJSON and shared memory | `lite3-noetic/hardware-ws/README.md`; `hardware-ws/src/emotion_bot_lite3_hw/`; host-install service files |
| Full-app protocol, state-aware Stand/Sit and high-level height failures | `lite3-noetic/docs/HARDWARE_APP_CONTROL.md`, sections 2–13; `docs/VERIFICATION.md`, dated 2026-09-19 sections |
| MotionSDK packet/layout, acquisition/release and official sequence | `lite3-noetic/hardware-ws/src/emotion_bot_lite3_hw/include/emotion_bot_lite3_hw/motion_sdk_protocol.hpp`; `lite3-noetic/hardware-ws/tools/`; official MotionSDK repository; local manufacturer manuals |
| `RobotStateInit` is reset/acquisition, not current-pose reading | official MotionSDK README/example and `HARDWARE_APP_CONTROL.md`, hoisted direct-joint attempt sections |
| Feedback pause/recovery, state/error, battery, attitude, tracking, STOP and ownership gates | `hardware-ws/tools/motion_sdk_safety.hpp`; `run_motion_sdk_neutral_breathing.cpp`; hardware README; native tests |
| Torque-derived contact equation and accepted support requirements | `hardware-ws/tools/motion_sdk_contact_estimator.hpp`; `motion_sdk_paw_lift.hpp`; contact/IK native tests; Joy/Anger tickets |

## Физички профили и тековен статус

| Emotion / integration | Primary evidence |
| --- | --- |
| Neutral accepted 3.25 s breathing | `tickets/TICKET_NEUTRAL_BREATHING.md`; `docs/VERIFICATION.md` animal-like section; hardware/app record |
| Joy accepted gesture and latest recovery still live-unvalidated | `tickets/TICKET_JOY_FRONT_PAW.md`; `tickets/README.md`; `TICKET_PHYSICAL_EMOTION_CHAT.md`; hardware README persistent-recovery section |
| Sadness accepted 48 mm front-only bow | `tickets/TICKET_SADNESS.md`; hardware README; normal chat evidence in integration ticket |
| Fear accepted planted flinch/cower | `tickets/TICKET_FEAR.md`; `docs/VERIFICATION.md` Fear section; integration ticket |
| Anger accepted canonical placements and latest recovery still live-unvalidated | `tickets/TICKET_ANGER.md`; `tickets/README.md`; integration ticket; hardware README persistent-recovery section |
| Affection, Curiosity, Disgust, Surprise planned and Neutral fallback | their four `tickets/TICKET_*.md` files; ticket index; profile resolver/status code and tests |
| Default allowlist remains `neutral` | `hardware-ws/README.md`; root/wrapper README; tickets index and integration ticket; runtime scripts/config |
| 1.5 s return + 0.35 s hold + newest-only target | ticket index “Non-negotiable transition rule”; integration ticket; runner/profile engine and tests |

Последните инсталирани runner SHA-256 вредности се
`aa449b7883a3baf6ae816fc832dbf3b8b74bb5a1ac88c2e0313f3acab1c8f353`
за Joy persistent recovery и
`e72a2db71d1f569b55d5c5af10b82c6b48a6a850513bb71aa3bacd9a84d4e5cd`
за Anger persistent recovery. Тие се преземени од датираните записи во
`lite3-noetic/docs/HARDWARE_APP_CONTROL.md` и `lite3-noetic/hardware-ws/README.md`.

## Неуспешни експерименти

Сите failure бројки и граници се преземени од
`lite3-noetic/docs/VERIFICATION.md`,
`lite3-noetic/docs/HARDWARE_APP_CONTROL.md` и соодветниот per-emotion ticket.
Овде спаѓаат zero-motion height trials, queue-depth serialization defect,
ineffective periodic range, unsafe current-pose takeover, stale `0x0906`,
unload/landing misses, 10-degree Sadness aborts, 75% battery-floor abort,
OpenAI/DNS timeout, Gazebo clock stall и визуелно одбиените профили.

## Надворешна литература и официјална документација

Библиографските детали се проверени преку официјални или примарни извори:

- Rosalind W. Picard, *Affective Computing*, MIT Press, 1997,
  DOI `10.7551/mitpress/1140.001.0001`.
- James A. Russell, “A Circumplex Model of Affect,” 1980,
  DOI `10.1037/h0077714`.
- Fong, Nourbakhsh and Dautenhahn, “A Survey of Socially Interactive Robots,”
  2003, DOI `10.1016/S0921-8890(02)00372-X`.
- Cynthia Breazeal, “Emotion and Sociable Humanoid Robots,” 2003,
  DOI `10.1016/S1071-5819(03)00018-1`.
- Kerstin Dautenhahn, “Socially Intelligent Robots,” 2007,
  DOI `10.1098/rstb.2006.2004`.
- Koenig and Howard, Gazebo paper, DOI `10.1109/IROS.2004.1389727`.
- ROS Noetic official docs: <https://docs.ros.org/en/noetic/>.
- Gazebo Classic official tutorials: <https://classic.gazebosim.org/tutorials>.
- OpenAI Responses streaming guide:
  <https://developers.openai.com/api/docs/guides/streaming-responses>.

## Manufacturer manuals

Authoritative PDF copies are under `lite3-robot-docs/`; Markdown conversions are
search aids only:

- `Jueying Lite3 Motion Development Manual (beta) V2.0.1-0.pdf`;
- `Jueying Lite3 Motion Host Communication Interface(beta) V1.0.7-0.pdf`;
- `Jueying Lite3 Perception Development Manual(beta) V2.1.1-0.pdf`;
- `Jueying Lite3 Pro User Manual V1.0.7-0.pdf`.

## Visual asset audit

Нема тврдење за непостоечки screenshot или фотографија. Сите девет figure
assets во семинарската се vector TikZ дијаграми изведени од проверената
архитектура и се наоѓаат во `seminarska/figures/`. Ако подоцна се додаде
фотографија или Gazebo screenshot, мора да се наведат датотека, датум,
checkout, provenance/rights и точното доказно ограничување.
