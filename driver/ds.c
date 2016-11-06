/*
MIT License

Copyright (c) 2016 Benjamin "Nefarius" Höglinger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "fireshock.h"


//
// Sends the "magic packet" to the DS3 so it starts its interrupt endpoint.
// 
NTSTATUS Ds3Init(WDFDEVICE hDevice)
{
    // "Magic packet"
    UCHAR hidCommandEnable[DS3_HID_COMMAND_ENABLE_SIZE] =
    {
        0x42, 0x0C, 0x00, 0x00
    };

    return SendControlRequest(
        hDevice,
        SetReport,
        Ds3StartDevice,
        0,
        hidCommandEnable,
        DS3_HID_COMMAND_ENABLE_SIZE,
        Ds3EnableRequestCompleted,
        hDevice);
}

//
// Periodically sends the DS3 enable control request until successful.
// 
VOID Ds3EnableEvtTimerFunc(
    _In_ WDFTIMER Timer
)
{
    NTSTATUS            status;
    WDFDEVICE           hDevice;

    KdPrint((DRIVERNAME "Ds3EnableEvtTimerFunc called\n"));

    hDevice = WdfTimerGetParentObject(Timer);

    status = Ds3Init(hDevice);

    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "Ds3Init failed with status 0x%X", status));
    }
}

//
// Sends output report updates to the DS3 via control endpoint.
// 
VOID Ds3OutputEvtTimerFunc(
    _In_ WDFTIMER Timer
)
{
    WDFDEVICE           hDevice;
    NTSTATUS            status;

    hDevice = WdfTimerGetParentObject(Timer);

    status = SendControlRequest(
        hDevice,
        SetReport,
        USB_SETUP_VALUE(Output, One),
        0,
        Ds3GetContext(hDevice)->OutputReportBuffer,
        DS3_HID_OUTPUT_REPORT_SIZE,
        Ds3OutputRequestCompleted,
        NULL
    );

    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "SendControlRequest failed with status 0x%X\n", status));
    }
}

//
// Returns customized configuration descriptor.
// 
VOID Ds3GetConfigurationDescriptorType(PUCHAR Buffer, ULONG Length)
{
    UCHAR ConfigurationDescriptor[DS3_CONFIGURATION_DESCRIPTOR_SIZE] =
    {
        0x09,        // bLength
        0x02,        // bDescriptorType (Configuration)
        0x29, 0x00,  // wTotalLength 41
        0x01,        // bNumInterfaces 1
        0x01,        // bConfigurationValue
        0x00,        // iConfiguration (String Index)
        0x80,        // bmAttributes
        0xFA,        // bMaxPower 500mA

        0x09,        // bLength
        0x04,        // bDescriptorType (Interface)
        0x00,        // bInterfaceNumber 0
        0x00,        // bAlternateSetting
        0x02,        // bNumEndpoints 2
        0x03,        // bInterfaceClass
        0x00,        // bInterfaceSubClass
        0x00,        // bInterfaceProtocol
        0x00,        // iInterface (String Index)

        0x09,        // bLength
        0x21,        // bDescriptorType (HID)
        0x11, 0x01,  // bcdHID 1.11
        0x00,        // bCountryCode
        0x01,        // bNumDescriptors
        0x22,        // bDescriptorType[0] (HID)
        DS3_HID_REPORT_DESCRIPTOR_SIZE, 0x00,  // wDescriptorLength[0]

        0x07,        // bLength
        0x05,        // bDescriptorType (Endpoint)
        0x02,        // bEndpointAddress (OUT/H2D)
        0x03,        // bmAttributes (Interrupt)
        0x40, 0x00,  // wMaxPacketSize 64
        0x01,        // bInterval 1 (unit depends on device speed)

        0x07,        // bLength
        0x05,        // bDescriptorType (Endpoint)
        0x81,        // bEndpointAddress (IN/D2H)
        0x03,        // bmAttributes (Interrupt)
        0x40, 0x00,  // wMaxPacketSize 64
        0x01,        // bInterval 1 (unit depends on device speed)

                     // 41 bytes

                     // best guess: USB Standard Descriptor
    };

    RtlCopyBytes(Buffer, ConfigurationDescriptor, Length);
}

//
// Returns customized HID report descriptor.
// 
VOID Ds3GetDescriptorFromInterface(PUCHAR Buffer)
{
    UCHAR HidReportDescriptor[DS3_HID_REPORT_DESCRIPTOR_SIZE] =
    {
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
        0x75, 0x08,        //     Report Size (8)
        0x95, 0x30,        //     Report Count (48)
        0x09, 0x01,        //     Usage (Pointer)
        0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0x75, 0x08,        //     Report Size (8)
        0x95, 0x30,        //     Report Count (48)
        0x09, 0x01,        //     Usage (Pointer)
        0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
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

                           // 176 bytes
    };

    RtlCopyBytes(Buffer, HidReportDescriptor, DS3_HID_REPORT_DESCRIPTOR_SIZE);
}

//
// Scales up DS axis (1 byte, unsigned) to XUSB axis (2 bytes, signed)
// 
SHORT ScaleAxis(SHORT value, BOOLEAN flip)
{
    value -= 0x80;
    if (value == -128) value = -127;

    if (flip) value *= -1;

    return (SHORT)(value * 258);
}

