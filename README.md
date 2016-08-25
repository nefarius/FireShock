# FireShock
HIDUSB filter driver for Sony DualShock controllers

[![Twitter Follow](https://img.shields.io/twitter/follow/shields_io.svg?style=social&label=Follow&maxAge=2592000)](https://twitter.com/CNefarius)

# Documentation

## Report descriptor
```c
0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
0x09, 0x05,        // Usage (Game Pad)
0xA1, 0x02,        // Collection (Logical)
0xA1, 0x01,        //   Collection (Application)
0x85, 0x01,        //     Report ID (1)
0x09, 0x30,        //     Usage (X)
0x09, 0x31,        //     Usage (Y)
0x09, 0x32,        //     Usage (Z)
0x09, 0x35,        //     Usage (Rz)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x04,        //     Report Count (4)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x39,        //     Usage (Hat switch)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x07,        //     Logical Maximum (7)
0x35, 0x00,        //     Physical Minimum (0)
0x46, 0x3B, 0x01,  //     Physical Maximum (315)
0x65, 0x14,        //     Unit (System: English Rotation, Length: Centimeter)
0x75, 0x04,        //     Report Size (4)
0x95, 0x01,        //     Report Count (1)
0x81, 0x42,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
0x65, 0x00,        //     Unit (None)
0x05, 0x09,        //     Usage Page (Button)
0x19, 0x01,        //     Usage Minimum (0x01)
0x29, 0x0E,        //     Usage Maximum (0x0E)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x01,        //     Logical Maximum (1)
0x75, 0x01,        //     Report Size (1)
0x95, 0x0E,        //     Report Count (14)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
0x09, 0x20,        //     Usage (0x20)
0x75, 0x06,        //     Report Size (6)
0x95, 0x01,        //     Report Count (1)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x7F,        //     Logical Maximum (127)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
0x09, 0x33,        //     Usage (Rx)
0x09, 0x34,        //     Usage (Ry)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x75, 0x08,        //     Report Size (8)
0x95, 0x02,        //     Report Count (2)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0xA1, 0x01,        //   Collection (Application)
0x85, 0x01,        //     Report ID (1)
0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
0x09, 0x01,        //     Usage (0x01)
0x09, 0x02,        //     Usage (0x02)
0x09, 0x03,        //     Usage (0x03)
0x09, 0x04,        //     Usage (0x04)
0x09, 0x05,        //     Usage (0x05)
0x09, 0x06,        //     Usage (0x06)
0x09, 0x07,        //     Usage (0x07)
0x09, 0x08,        //     Usage (0x08)
0x09, 0x09,        //     Usage (0x09)
0x09, 0x0A,        //     Usage (0x0A)
0x75, 0x08,        //     Report Size (8)
0x95, 0x0A,        //     Report Count (10)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xA1, 0x01,        //     Collection (Application)
0x85, 0x01,        //       Report ID (1)
0x06, 0x01, 0xFF,  //       Usage Page (Vendor Defined 0xFF01)
0x09, 0x01,        //       Usage (0x01)
0x75, 0x08,        //       Report Size (8)
0x95, 0x1D,        //       Report Count (29)
0x15, 0x00,        //       Logical Minimum (0)
0x26, 0xFF, 0x00,  //       Logical Maximum (255)
0x81, 0x02,        //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //     End Collection
0xC0,              //   End Collection
0xC0,              // End Collection

                   // 160 bytes
```

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

### Buttons
| byte index |      bit 7     |     bit 6     |     bit 5     |     bit 4    | bit 3 |    bit 2    |    bit 1   |  bit 0 |
|------------|:--------------:|:-------------:|:-------------:|:------------:|:-----:|:-----------:|:----------:|:------:|
| [05]       |     Square     |     Cross     |     Circle    |   Triangle   | D-Pad |    D-Pad    |    D-Pad   |  D-Pad |
| [06]       | Right Shoulder | Left Shoulder | Right Trigger | Left Trigger | Start | Right Thumb | Left Thumb | Select |
| [07]       |                |               |               |              |       |             |            |   PS   |

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
