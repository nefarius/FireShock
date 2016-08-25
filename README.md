# FireShock
HIDUSB filter driver for Sony DualShock controllers

[![Twitter Follow](https://img.shields.io/twitter/follow/shields_io.svg?style=social&label=Follow&maxAge=2592000)](https://twitter.com/CNefarius)

## Report structure

### Axes
| byte index | axis value                        |
|------------|-----------------------------------|
| [01]       | Left Thumb X (0 = left)           |
| [02]       | Left Thumb Y (0 = up)             |
| [03]       | Right Thumb X (0 = left)          |
| [04]       | Right Thumb Y (0 = up)            |
| [08]       | Left Trigger (L2, 0 = released)  |
| [09]       | Right Trigger (R2, 0 = released) |

### Pressure sensitive buttons/axes
| byte index | pressure value      |
|------------|---------------------|
| [09]       | D-Pad Up            |
| [10]       | D-Pad Right         |
| [11]       | D-Pad Down          |
| [12]       | D-Pad Left          |
| [13]       | Left Shoulder (L1)  |
| [14]       | Right Shoulder (R1) |
| [15]       | Triangle            |
| [16]       | Circle              |
| [17]       | Cross               |
| [18]       | Square              |
