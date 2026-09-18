# Jueying Lite3 Pro User Manual V1.0.7-0

Source: [Jueying Lite3 Pro User Manual V1.0.7-0.pdf](<Jueying Lite3 Pro User Manual V1.0.7-0.pdf>)

> Automatically extracted from the source PDF. Text, headings, and page boundaries are preserved where practical; consult the PDF for diagrams, screenshots, and exact table layout.

<!-- PDF page 1 -->

Pro
User Manual V1.0.7-0
2024/03/26

<!-- PDF page 2 -->


<!-- PDF page 3 -->

Statement
- This manual is the information asset owned by Hangzhou Yunshenchu Technology
Co.,Ltd. (hereafter referred to as DEEP Robotics) and any reproduction of part or
all of this manual is strictly prohibited without the permission of DEEP Robotics.
- This manual explains the basic components, transportation and storage, specific
operations, exception handling, and technical specifications of Jueying Lite3. Be
sure to read and understand this manual carefully before operating the robot.
- Basic information on safe use is described in detail in the "Reading Tips", so be
sure to read this part thoroughly to ensure proper use.
- The diagrams and photographs in this manual are representative examples and
may differ in detail from the product purchased.
- This manual may be modified as appropriate for product improvements,
specification changes, etc.
- The contents of this manual do not exclude the possibility of misremembering
or omission.If this manual is damaged or lost or if you have questions about the
contents of this manual, please contact us promptly.
- Failure caused by unauthorized disassembly or modification of the product by the
customer is not covered by our warranty, see "Service & Warranty" for details.

Reading Tips
Description of Symbol
Before use (installation, transportation, maintenance, inspection), please be sure to read
and master this manual, and familiarize yourself with the equipment and safety matters
before you start using it. The safety matters in this manual are divided into three kinds:
"Caution", "Mandatory" and "Prohibition". Even the contents of "Caution" may have serious
consequences depending on the situation, so any of these safety matters are extremely
important and should be strictly observed.
Usage tips or operational recommendations. Improper using or
Caution      operating the robot may cause damage to it.
Mandatory Matters that must be observed.

Prohibition Matters prohibited. Misoperation is dangerous and may cause injury to
operators or damage to the robot.

Download DEEP Robotics APP
Use "DEEP Robotics" APP to control the robot and scan the QR code to
download and install it (Download Center). "DEEP Robotics" APP supports
Android 6.0 or later, and does not yet support iOS.

Get Help
For more resources to assist you in using Jueying Lite3 proficiently, you can also visit DEEP
Robotics' corporate website: http://www.deeprobotics.cn.
1

<!-- PDF page 4 -->

Important Safety Tips

Before starting the robot, please ensure that all people and objects
present are more than 1 meter away from the robot to avoid collisions.

- When the robot passes through stairs or slopes, do not stand on the
stairs, platforms, or slopes below the robot to avoid personal injury
when it falls.
- When the robot swaying legs, shaking violently or other abnormal
phenomena occur in use, press [⑦STOP] to activate the soft
emergency stop protection, so that the moving robot enters a
protective state. The robot will automatically get down. After
identifying the problem, cancel Emergency STOP to operate the robot
normally.

EU Conformity Statement
Hereby, Hangzhou Yunshenchu Technology Co.,Ltd. declares that the radio
equipment is in compliance with Directive 2014/53/EU.

RF Exposure Information
- This device has been tested and meets applicable limits for Radio Frequency (RF)
exposure.
- This equipment must be installed and operated in accordance with provided instructions
and the antenna(s) used for this transmitter must be installed to provide a separation
distance of at least 20 cm from all persons and must not be co-located or operating in
conjunction with any other antenna or transmitter.

Other Information
- This appliance can be used by children aged from 8 years and above and persons with
reduced physical, sensory or mental capabilities or lack of experience and knowledge
if they have been given supervision or instruction concerning use of the appliance
in a safe way and understand the hazards involved, children shall not play with the
appliance, cleaning and user maintenance shall not be made by children without
supervision.
- This appliance contains battery cells that are non-replaceable.

- For the purposes of recharging the battery, only use the detachable
supply unit provided with this appliance.
- If the appliance is to be stored unused for a long period, the batteries
are removed.
- The supply terminals are not to be short-circuited.

2

<!-- PDF page 5 -->

Content                             3.3 Start                       18
### 3.4 Connection                  18

### 3.5 Reset to Zero               19

Statement1
### 3.6 Motion Control              19

Reading Tips                   1   3.7 Action Options              19

Description of Symbol          1   3.8 Voice Command               20

Download DEEP Robotics APP     1   3.9 AI Options                  20
#### 3.9.1 Auto Stop             20

Get Help                       1
#### 3.9.2 Obstable Avoidance    20

Important Safety Tips          2   3.10 Payload                    21
### 3.11 Emergency Operation        21

## 1 Introduction                 4

#### 3.11.1 Emergency STOP       21

1.1 Overview                   4       3.11.2 Fall Down & Get up   21
1.2 Product List               4       3.11.3 Overtemperature      22
#### 3.11.4 Low Battery          22

### 1.3 Part Name                  5

#### 3.11.5 Other                22

### 1.4 Main Specifications        5

### 3.12 Power Off                  22

### 1.5 Interface                  7

## 4 Precautions                   23

## 2 Functions and Status         8

### 4.1 Work Environment            23

### 2.1 Motion Mode and Gait       8

### 4.2 Battery                     23

### 2.2 Description of Lighting    8

### 4.3 Other Precautions           24

#### 2.2.1 Charger              8

2.2.2 Battery              8   4.4 Disposal                    24
#### 2.2.3 LED Eyes             9

## 5 FAQs                          25

2.3 "DEEP Robotics" APP       10
#### 2.3.1 Home Page           10   6 Transport & Storage           26

#### 2.3.2 Control Page        11

### 6.1 Transportation              26

#### 2.3.3 Flat Gait           12

2.3.4 RUG Gait            13   6.2 Storage                     26
#### 2.3.5 Action List         14   7 Service & Warranty            27

#### 2.3.6 Settings Page       14

### 7.1 After-Sales Service         27

3 Operation                   16   7.2 Warranty Policy             27
3.1 Preparation               16   7.3 Warranty Coverage           27
3.1.1 Environment         16   7.4 Repair Instructions         28
#### 3.1.2 Carrying            16

#### 3.1.3 Checking            16

### 3.2 Charging                  17

<!-- PDF page 6 -->

Jueying Lite3 User Manual

## 1 Introduction

### 1.1 Overview

Jueying Lite3 is an intelligent quadruped robot with 12 degrees of freedom, featuring
a variety of gaits and movements. Jueying Lite3 Pro provides SDK for motion control
algorithm development, source code of some perception development examples, and
communication protocol, allowing users to re-develop as needed.
370mm                                                610mm

445mm

### 1.2 Product List

Robot×1                            Transport Case×1                     Sole Kit×1
(Battery not included)

Replaceable Battery×1                        Charger×1                        Controller×1

Charging Base ×1         Qualified Certificate×1       Warranty Card×1       Fold-out Guide×1

4

<!-- PDF page 7 -->

Jueying Lite3 User Manual

### 1.3 Part Name

Depth Camera
Replaceable Battery
Wide Angle Camera                                                 Upper Leg
Ultrasonic Radar
Lower Leg

Non-slip Sole

Handle

Interface

Ultrasonic Radar

### 1.4 Main Specifications

Robot Dimensions

Standing Size (Length × Width × Height)       610mm×370mm×445mm

Sitting Size (Length × Width × Height)        680mm×370mm×175mm

Weight                                        12.7kg

5

<!-- PDF page 8 -->

Jueying Lite3 User Manual

Perception System

Wide Angle Camera                                       ×1

Ultrasonic Radar                                        ×2

Depth Camera                                            ×1

AI Computer                                             NVIDIA Jetson Xavier NX

Locomotion Parameters

Slope                                                   40°

Steps' Height                                           15cm

No-load Duration                                        1.5h~2h

Electric Parameters

Battery Capacity                                        4.4Ah

Nominal Battery Voltage                                 28.8V

Charger Input                                           100V~240V

Charger Output                                          33.6V/5A

Charging Time                                           40min~1h

Other

Operating Temperature                                   0℃ ~40℃

WiFi Frequency Band                                     5150MHz~5250MHz; 5725MHz~5850MHz

5150MHz~5250MHz: 14.87dBm
WiFi Transmitter Power
5725MHz~5850MHz:13.89dBm

※ Data above are measured under ideal conditions, and the actual results may be biased.

6

<!-- PDF page 9 -->

Jueying Lite3 User Manual

### 1.5 Interface

Ethernet   HDMI   USB3.0   5V   24V

7

<!-- PDF page 10 -->

Jueying Lite3 User Manual

## 2 Functions and Status

### 2.1 Motion Mode and Gait

Mode                Description

Move                Choose a gait and push joysticks to make the robot move

Pose                Push joysticks to change pitch, roll, yaw and body height

Gait                Description

Flat                Adjust height and velocity of the robot in Flat gait

RUG                 Choose an appropriate gait based on the actual terrain in RUG gait

### 2.2 Description of Lighting

#### 2.2.1 Charger

Charger Light

No Light      No power
Green Light   Connected to the mains (100V~240V)

#### 2.2.2 Battery

Battery LED       Power

8

<!-- PDF page 11 -->

Jueying Lite3 User Manual

OFF                     ON                   FLASHING            MOVING

Status                                          Meaning

Four lights on                          Power>75%

Three lights on                       50%<Power<75%

Two lights on                        25%<Power<50%

One light on                      20%<Power<25%

Four flashing lights                     5%<Power<20%

One flashing for 30s and off                    Power<5%

Moving light,light on in turn        Charging,lights show % charge

#### 2.2.3 LED Eyes

LED Eyes

Normal Status

Light Status                  Robot Status           Meaning

Blue moving lights            Starting               Starting up and self-checking

Waiting for
Blue breathing lights                                Started, and waiting for the APP to connect
connection

Blue lights on                Sit down               Connected, and the robot is sitting

White breathing lights        Moving                 Standing, moving, or twisting

White lights flash twice      Change                 Switching gait, action, status

Fall Down
White flashing lights                                Falling down and getting up
& Get up

Blue&purple moving lights Performing                 Performing an action in the action list

9

<!-- PDF page 12 -->

Jueying Lite3 User Manual

Abnormal States

Light Status           Robot Status       Meaning

Yellow lights          Low Power (35%)    The power of robot is below 35%

Red lights on          Low Power (20%)    The power of robot is below 20%,need charging

Yellow & red lights
Disconnection      Connection between robot and APP was broken
flash in turn
Red lights flash                          Joints of the robot were too hot, need to wait
Overtemperature
at medium speed                           until it cools down
Red lights flash
Overcurrent        Electric current of robot's joints was too high
at high speed

2.3 "DEEP Robotics" APP
#### 2.3.1 Home Page

1      2                                                           3

4

Function                    Description

① General                   View APP Version, Change Language, etc.

② Album                     View the screenshots of video streaming

③ Connect                   Connecting to the robot WiFi

④ Control                   Go to Control Page

10

<!-- PDF page 13 -->

Jueying Lite3 User Manual

#### 2.3.2 Control Page

To accesss Control Page, click [④Control] button on Home Page.
5       6           7                                  8          9           10

11                                              12

13            14         15               16     17           18

Function                   Description

⑤ Return                   Go back to Home Page
Motion Settings | Perception Settings | Other Settings
⑥ Settings
(see 2.3.3 for details)
Make the robot down immediately, generally used when the
⑦ STOP
robot's joints are uncontrollable or in an emergency

⑧ WiFi                     Show robot WiFi signal strength

⑨ Robot Battery            Show the robot's battery percentage

⑩ Remote Control Battery Show the remote controller's battery percentage

Control the robot to translate in Move Mode and change its
⑪ Left Joystick
pitch and roll angle in Pose Mode

Control the robot to rotate in Move Mode and change its
⑫ Right Joystick
height and yaw angle in Pose Mode

⑬ Stand/Sit                Switch Stand and Sit posture

⑭ Move/Pose                Choose a Motion Mode: Move Mode or Pose Mode

⑮ Flat/RUG                 Choose a Motion Gait: Flat Gait or RUG Gait

11

<!-- PDF page 14 -->

Jueying Lite3 User Manual

Function                    Description

⑯ Voice                     Execute voice control function (see 3.8 for details)

⑰ Screenshot                Capture the current picture of the video stream

⑱ Action                    Open the action list (see 2.3.5 for details)

#### 2.3.3 Flat Gait

To access Flat gait, click [⑮Flat/RUG] button.
19           20

Gaits                   Description
Adjust the body height of the robot to NRM/Crawl:
In NRM, the robot is in normal height and the velocity can be set
⑲ Body Height
to Slow, Medium or Fast; in Crawl, the robot lowers its body and
moves at a defalut velocity.
Set the velocity of the robot to Low, Medium, or Fast
⑳ Velocity
(Effective only when Body Height is in NRM)

12

<!-- PDF page 15 -->

Jueying Lite3 User Manual

#### 2.3.4 RUG Gait

To access RUG gait, click [⑮Flat/Rug] button.
21

Gaits                  Description

㉑ RUG gait
Including three gaits for different rugged terrains: General, Grip,
and H-Step

RUG                    Applicable Terrain

Suitable for general stairs and slopes:
General                steps' height ≤12cm;
slope ≤ 30° (may be biased due to the slope material)
Suitable for steep slopes:
Grip
slope ≤ 40° (may be biased due to the slope material)
Suitable for higher stairs:
H-Step
steps' height ≤15cm

13

<!-- PDF page 16 -->

Jueying Lite3 User Manual

#### 2.3.5 Action List

To access Action list, click [⑱Action] button on Control Page.

The robot can perform actions such as Hello, Twist, Moonwalk, Long Jump, Twist Jump.

#### 2.3.6 Settings Page

To access Settings, click [⑥Settings] button on Control Page.
22                                    27
23                                       28

24                                   29

25
30

26                                 31

14

<!-- PDF page 17 -->

Jueying Lite3 User Manual

33            32     34

Function                                    Description

㉓ Keep Stepping              Make the robot keep stepping

㉔ Lab Mode                   Allow users to try out experimental features

㉒ Motion
Initialize the joint motor when it loses its
㉕ Reset to Zero              position (only available when the robot is
sitting)
Save the fault information to the motion host
㉖ Save Data
for troubleshooting
㉘ Video Stream               Show the live stream of robot camera
Automatically stop when encountering
㉗ Perception                  ㉚ Auto Stop
㉙ AI Options
obstacles
㉛ Obstable
Automatically bypass obstacles
Avoidance
㉝ Speaker                    Turn on and off robot speaker
㉜ Other                                     A safety tip will appear before performing
㉞ Action Double-Check
Long Jump

15

<!-- PDF page 18 -->

Jueying Lite3 User Manual

## 3 Operation

### 3.1 Preparation

#### 3.1.1 Environment

- Please ensure that operators and non-operators present have read
the manual carefully and understand the basic operating instructions
and safety precautions.
- Before start the robot, ensure that all people or objects present are
more than 1 meter away from the robot to avoid collisions.
- Please use the robot in an environment of 0°C ~40°C.

#### 3.1.2 Carrying

Hold the back handle of the robot and lift it out of the transport box onto a flat road surface.

Handle

- Please carry the robot gently. Please do not release the robot handle
when the robot is lifted, to prevent it from falling, which may cause
the robot to lose control or even be damaged.
- Please avoid joints to prevent pinching or even scratching.

#### 3.1.3 Checking

- Press the power button once to check the battery. It is suggested to
start the robot when the battery power is at least 75%.
- Make sure the remote controller is fully charged.
- Make sure there is no visible damage to the exterior of the robot.
- Check if Emergency STOP Controller has power and STOP Button is
released.

If the robot parts are aging or damaged, please do not start the robot
and contact the after-sales staff in time.

16

<!-- PDF page 19 -->

Jueying Lite3 User Manual

### 3.2 Charging

Jueying Lite3 is powered by a ternary lithium battery that is pluggable.
User can insert the battery into the charging base for charging.

1. First lift the robot up slightly on the left side and place it firmly, and then press the button
at the bottom of the battery bin and the battery will pop out, after which the battery can be
removed from the battery bin.

2. Insert the battery into the charging base, connect the charging base to the charger, and
then connect the charger to the mains (100V~240V) for charging, and the charger light will
light up green.

3. When charging, the battery LED lights are all in a moving state, and the number of lights
on corresponds to the power already charged.
4. After finishing charging, the four LED lights on the battery will turn off.

- It is recommended charge in an environment of 5°C ~30°C.
- During charging, please always pay attention to the battery and
charger to prevent accidents, and disconnect the charging power in
time after charging is completed.

17

<!-- PDF page 20 -->

Jueying Lite3 User Manual

### 3.3 Start

Press the power button briefly and then press and hold until the LED lights flash once,
the robot starts up, and the power light shows the current battery level. Then power on
Emergency STOP Controller.

### 3.4 Connection

During the startup of the built-in wireless router in the robot, LED Eyes
lights up with blue moving lights. Connect the remote controller to the
robot after LED Eyes turns into blue breathing lights.

1. Scan the QR code to download and install "DEEP Robotics" APP. "DEEP Robotics" APP
supports Android 6.0 or later, and does not yet support iOS.

2. Open "DEEP Robotics" APP on remote controller or your phone.

3. Check the label on the Warranty Card for WiFi information and click [③Connect] button
to connect. After the connection is completed, you can click the [④Control] button to enter
the control page and control the robot.
after connection

18

<!-- PDF page 21 -->

Jueying Lite3 User Manual

### 3.5 Reset to Zero

After connected, position the robot to Ready Posture and then enter Control Page and click
the [⑬Stand] button to initializing the robot which takes about 10 seconds. The robot will
reset to zero and then automatically stand up.

[⑬Stand]

Ready Posture                      Reset to Zero                       Standing Posture

### 3.6 Motion Control

After standing up, you can select gait or terrain options in Move Mode, and push the
joysticks to make the robot move.
When the robot moves on flat terrain with Flat gait, users can choose an appropriate body
height and velocity according to needs.
When encountering lower steps, stairs, or gentle slopes or grasslands, users can choose
the General gait of RUG; when encountering steep slopes, users can choose the Grip gait
of RUG; when encountering higher steps, users can choose the H-Step gait of RUG.
After standing up, in Pose Mode, the joysticks can be pushed to make the robot twist.

When the robot passes through stairs or slopes, do not stand on the
stairs, platforms, or slopes below the robot to avoid potential personal
injury when the robot falls.

### 3.7 Action Options

When the robot is standing or lying still, user can click the [ ⑱ Action] button to open the
action list and select an action.

- To perform Long Jump, make sure that there are no obstacles within
2m in front of the robot.
- Please avoid using the robot continuously and intensively, otherwise it
may cause overheating or damage.

19

<!-- PDF page 22 -->

Jueying Lite3 User Manual

### 3.8 Voice Command

Click the [⑯Voice] button at the bottom of the Control Page and say the corresponding
command, and the robot will execute the corresponding action.

Command（EN） Command（ZH）                 Action

stand up            站起来                 Stand up

get down            趴下                  Get down

go forward          往前走                 Walk forward for 5 seconds

go backward         往后走                 Walk backward for 5 seconds

go left             往左走                 Walk to the left for 5 seconds

go right            往右走                 Walk to the right for 5 seconds

stop                停止                  Stop walking

look up             往上看                 Raise the head

look down           往下看                 Bow the head

look left           往左看                 Turn head to the left

look right          往右看                 Turn head to the right

turn left           往左转                 Turn 90° to the left and stop

turn right          往右转                 Turn 90° to the right and stop

turn around         往后转                 Turn backwards 180° and stop

say hello           恭喜发财                Greet

### 3.9 AI Options

#### 3.9.1 Auto Stop

Click the [⑥Settings] button to enter the Settings Page and select the [㉚Auto Stop] option
in [㉙AI Options] to enable the auto stop function. Return to the Control Page, and manually
push the joysticks to make the robot move. The robot will be able to detect obstacles
forward and backward and slow down.

#### 3.9.2 Obstable Avoidance

Click the [⑥Settings] button to enter the Settings Page and select the [㉛Obstable
Avoidance] option in [㉙AI Options]. When [㉛Obstable Avoidance] is turned on, return to
Control Page and pushing the joysticks to control the movement of the robot, the robot can
detect obstacles on its way and avoid them.

20

<!-- PDF page 23 -->

Jueying Lite3 User Manual

### 3.10 Payload

Users can screw devices into the thread holes on the back of robot (unit: mm).

When the weight of devices reaches or exceeds 4 kilograms, robot's
motion performance may be affected. Please consult with after-sales
personnel before adding overweight devices.

### 3.11 Emergency Operation

#### 3.11.1 Emergency STOP

When the robot swings its legs, shakes violently or other abnormal phenomena occurs
in use, push STOP Button on Emergency STOP Controller into stop to let the robot
into a protected state. The robot will automatically get down, and shut down. After
troubleshooting, release STOP Button by twisting it.

#### 3.11.2 Fall Down & Get up

When the robot falls on its back accidentally, you can choose to let the robot turn left or
turn right based on the surrounding obstacles: If there is an obstacle on the left side of the
robot, choose right. If there is an obstacle on the right side of the robot, choose left.

21

<!-- PDF page 24 -->

Jueying Lite3 User Manual

#### 3.11.3 Overtemperature

When the robot runs for a long time and cause the motor or actuator to overheat, it will
automatically turn on overtemperature protection: stop moving and sit down in place.
Please wait until it cools down and then press [⑬Stand/Sit] to continue.

#### 3.11.4 Low Battery

When the robot's power is below 35%, users should change or charge the battery as soon
as possible. When the robot's power is below 20%, low power protection will be triggered
and the robot will not respond to motion commands. Please power off the robot and replace
the battery, refering to "3.2 Charging" for specific instructions on replacing batteries.

#### 3.11.5 Other

- If the robot is out of control with AI options, press the [⑦STOP] in time.
- If you encounter a fire, do not use water to extinguish it. Please use one of the following
types of fire extinguishers nearby: foam, dry powder or carbon dioxide.
- If [⑦STOP] fails or the robot has smoke or water in it or other unexpected situations
occur, please try to cut off the robot power and remove the battery at first. And wait until
it's safe to troubleshoot the problem. Then feedback the situation to DEEP Robotics, and
we will offer help. Please pay attention to safety in use.

### 3.12 Power Off

Make sure that the robot is down before performing the following
operations.

Press the power button briefly and then press and hold until the LED lights flash once.
Then the robot LED Eyes and the battery LED lights turn off, indicating the shutdown is
completed.

22

<!-- PDF page 25 -->

Jueying Lite3 User Manual

## 4 Precautions

### 4.1 Work Environment

- Do not operate the robot in environments with strong electromagnetic
interference such as high-voltage cable, high-voltage transmission
stations, base stations and television broadcasting towers, etc.
- Please do not operate the robot in environments with strong WiFi
signal interference. Be sure to turn off all other WiFi signal source,
and then use DEEP Robotics APP to operate the robot.
- Do not operate the robot in bad weather with fog, snow, rain, lightning,
sandstorms, windstorms, tornadoes, etc.
- Keep the robot in sight and keep it at least 1 meter away from people,
water, open flames, etc. at all times.
- When using the robot on smooth surfaces such as ice, glass and tiles,
avoid voilent movements and use Grip gait to prevent the robot from
slipping and falling.
- Do not run the robot on the edge of a high place to prevent it from
falling from a height and causing damage.

### 4.2 Battery

- When water is touched inside the battery, a decomposition reaction
may occur, which may cause the battery to self-ignite or even
explode. It is strictly forbidden to expose batteries to any liquid, never
immerse them in water or get wet, and never use them in rain or wet
environment. If the battery accidentally falls into water, immediately
place the battery in a safe open area and keep it away from the battery
until it is completely dry. Drying batteries should not be reused.
- It is strictly forbidden to use drum bags, leaks, damaged batteries and
charge them. Rechargeable lithium batteries should not be used when
they appear odor, distortion, discoloration or any other abnormal
phenomenon. In case of abnormal battery condition, please contact
after-sale for further treatment.
- Disassembly of batteries without authorization is prohibited.
Once disassembled, no warranty is granted. Deep Robotics is not
responsible for battery accidents caused by the removal of batteries.
- Recharge and discharge every 3 months to maintain battery activity.
- Please refer to the battery instructions for details.

23

<!-- PDF page 26 -->

Jueying Lite3 User Manual

### 4.3 Other Precautions

- When handling the robot, pay attention to the anti-pinch label on the
robot and never put your hands into the position where the anti-pinch
label is attached!
- Pay attention to the sealing label on the robot. It is strictly forbidden
to disassemble the robot privately. Once disassembled, the warranty
will be invalid!
- The device is restricted to indoor use only when operating in the 5150
to 5250 MHz frequency range.

### 4.4 Disposal

- The disposal of waste robots and parts is to be carried out in accordance with the
corresponding national laws and regulations on the recycling of waste electrical and
electronic products.
- In particular, the use or disposal of lithium batteries contained in robots is subject to
national laws and regulations governing the disposal of batteries.

24

<!-- PDF page 27 -->

Jueying Lite3 User Manual

## 5 FAQs

Q1: Is it normal for a robot to stop moving on its own?
A: The motor or driver may be overtemperature. Please wait 10 minutes and try again. If
you still can't control the robot to move, please check if the power is sufficient.

Q2: What if the robot falls down due to the loss of control of one of its legs?
A: First click [⑦STOP] to make it down. Then restart the robot. If it does not return to
normal after restarting, please contact after-sales staff.

Q3: What if the video stream gets stuck after the robot falls?
A: Restart the robot. If the video stream is still black, please contact after-sales staff.

Q4: What if encountering a problem that cannot be solved even after consulting this
manual?
A: Click [ ㉖ Save Data] in Motion Settings and contact the after-sales staff promptly.

25

<!-- PDF page 28 -->

Jueying Lite3 User Manual

## 6 Transport & Storage

### 6.1 Transportation

The transport case is 633mm×462mm×329mm.

Before shipping the robot with transport case, remove the batteries from
the robot. And when shipping, make sure the front of the transport case
(with DEEP Robotics logo) is facing up.

### 6.2 Storage

- Jueying Lite3 requires a clean and dry storage environment of 0℃ ~40℃ .
- Robot power must be off, and if the robot will not be used for a long time, remove the
batteries from the robot.
- Do not allow water or other liquids to drench the robot.
- It is strictly forbidden to place other objects within the joint rotation range.
- It is recommended to store Jueying Lite3 in the transport case specifically designed for
it to protect it from shock and vibration.
- Jueying Lite3 must be placed in the transport case with its back facing up.
- For the precautions for battery storage, refer to the battery instructions.

26

<!-- PDF page 29 -->

Jueying Lite3 User Manual

## 7 Service & Warranty

### 7.1 After-Sales Service

Provide free training of using and oprating robot, online technical support and after-sales
service to users.

### 7.2 Warranty Policy

The warranty period for the major components of Jueying Lite3 is as below.

Component Name                                                              Warranty Period

Joint Module; Replaceable Battery                                           Six months

Wide Angle Camera, Ultrasonic Radars, Depth Camera, AI
One year
Computer, Control System

Other Electronic Components                                                 One year

Tip: Shell, foot and other fragile parts, and transport case and other accessories are not covered by
warranty. If necessary, please consult after-sales support.
The warranty period starts from the date of receipt. Products or parts that meet the
warranty period and the contents of the warranty will receive free after-sales service. If
the product you purchased is beyond the warranty period, you can also get help from us by
purchasing a separate service.

### 7.3 Warranty Coverage

Depending on the specific situation, we will repair or replace parts accordingly for the
product you purchased. However, the following cases will not be covered by the free
warranty, but you can still choose to have paid after-sales service, for which please consult
the after-sales support for details.
- Damage caused by man-made problems but not by quality problems of the product itself
occurs.
- Private modification, disassembly or opening of the shell occurs.
- Damage caused by incorrect installation, use and operation in accordance with the
manual.
- Damage caused by use in excess of safe load range.
- Damage caused by self-installation of third party products.
- Failure or damage due to force majeure factors such as typhoon, earthquake, fire,
lightning strike, abnormal voltage, etc.

27

<!-- PDF page 30 -->

Jueying Lite3 User Manual

### 7.4 Repair Instructions

- Before getting after-sales service, please make sure to backup all data and delete
important data to prevent data loss or leakage. DEEP Robotics is not responsible for the
loss or leakage of any data contained in the product.
- When you obtain after-sales service from DEEP Robotics, you authorize
to make any modification, delete data or restore factory settings for the purpose of after-
sales service.
- Before sending it for repair, please contact after-sales support, DEEP Robotics will try to
diagnose and solve your problem remotely.
- If the above methods cannot solve your problem, you can send it back to the robot for
repair after verifying with after-sales support. You need to pay for the postage first
when you send the product to DEEP Robotics. After DEEP Robotics receives the product
in need of maintenance, the product will be tested to determine the problem and
responsibility.
- If the problem is caused by defects in quality of the product itself, DEEP Robotics will be
responsible for the testing fee, material fee, labor fee and the postage for sending back.
- If the product does not meet the conditions of free repair, you can choose to pay for
repair, and the corresponding testing fee, material fee, labor fee and the postage for
sending back will be paid by you. You can also choose not to repair and to send back the
product, the corresponding postage and insurance fee will be paid by you.
- Considering environmental protection and safety, please do not send back seriously
damaged batteries. If you have sent back, DEEP Robotics will scrap such batteries and
will not return them back.
- If you provide an incorrect delivery address which results in non-delivery or rejection by
the recipient, the adverse consequences and losses shall be afforded by you.
- To ensure your rights and interests, when you sign for the after-sale products sent by
DEEP Robotics, please check carefully whether the products are intact. If there is any
abnormality, please immediately take video or photos on the spot and contact DEEP
Robotics to get the solution. If there are unresolved after-sale problems, please also
contact DEEP Robotics immediately, otherwise it is regarded as the end of this after-sale
service without dispute.

※The final interpretation of these after-sales terms and conditions belongs to DEEP Robotics.
※Please contact us if you have any questions before obtaining after-sales service.
※These after-sales terms and conditions are only used in mainland China, and the after-sales policies of
other countries or regions are subject to local laws.

28

<!-- PDF page 31 -->


<!-- PDF page 32 -->

Hangzhou Yunshenchu Technology Co.,Ltd.

ADD: Building 3, Zijin Dream Plaza, No.36 Xinran Street, Sandun Town,
Xihu District, Hangzhou, Zhejiang, China
TEL: +86 400-0559-095
WEB: www.deeprobotics.cn
