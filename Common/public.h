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


// {2409EA50-9ECA-410E-AC9E-F9AC798C4D9C}
DEFINE_GUID(GUID_DEVINTERFACE_FIRESHOCK,
    0x2409ea50, 0x9eca, 0x410e, 0xac, 0x9e, 0xf9, 0xac, 0x79, 0x8c, 0x4d, 0x9c);

#pragma once

#define FILE_DEVICE_BUSENUM             FILE_DEVICE_BUS_EXTENDER
#define BUSENUM_IOCTL(_index_)          CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_READ_DATA)
#define BUSENUM_W_IOCTL(_index_)        CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_WRITE_DATA)
#define BUSENUM_R_IOCTL(_index_)        CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_READ_DATA)
#define BUSENUM_RW_IOCTL(_index_)       CTL_CODE(FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_WRITE_DATA | FILE_READ_DATA)

#define IOCTL_FIRESHOCK_BASE 0x801

// 
// IO control codes
// 

#define IOCTL_FIRESHOCK_FS3_REQUEST_REPORT       BUSENUM_RW_IOCTL (IOCTL_FIRESHOCK_BASE + 0x000)
#define IOCTL_FIRESHOCK_FS3_SUBMIT_REPORT        BUSENUM_RW_IOCTL (IOCTL_FIRESHOCK_BASE + 0x001)
#define IOCTL_FIRESHOCK_REQUEST_SETTINGS         BUSENUM_RW_IOCTL (IOCTL_FIRESHOCK_BASE + 0x002)
#define IOCTL_FIRESHOCK_SUBMIT_SETTINGS          BUSENUM_W_IOCTL  (IOCTL_FIRESHOCK_BASE + 0x003)


///-------------------------------------------------------------------------------------------------
/// <summary>Stores various options for the device.</summary>
///
/// <remarks>Benjamin, 08.10.2016.</remarks>
///-------------------------------------------------------------------------------------------------
typedef struct _FS_DEVICE_SETTINGS
{
    BOOLEAN XusbEmulationEnabled;

    BOOLEAN XusbHidInputEnabled;

    BOOLEAN XusbHidOutputEnabled;

    BOOLEAN FsHidInputEnabled;

    BOOLEAN FsHidOutputEnabled;

} FS_DEVICE_SETTINGS, *PFS_DEVICE_SETTINGS;

///-------------------------------------------------------------------------------------------------
/// <summary>Defines FireShock 3 compatible buttons.</summary>
///
/// <remarks>Benjamin, 08.10.2016.</remarks>
///-------------------------------------------------------------------------------------------------
typedef enum _FS3_BUTTON
{
    FS3_GAMEPAD_SELECT = 0x0100,
    FS3_GAMEPAD_LEFT_THUMB = 0x0200,
    FS3_GAMEPAD_RIGHT_THUMB = 0x0400,
    FS3_GAMEPAD_START = 0x0800,
    FS3_GAMEPAD_LEFT_TRIGGER = 0x1000,
    FS3_GAMEPAD_RIGHT_TRIGGER = 0x2000,
    FS3_GAMEPAD_LEFT_SHOULDER = 0x4000,
    FS3_GAMEPAD_RIGHT_SHOULDER = 0x8000,
    FS3_GAMEPAD_TRIANGLE = 0x0010,
    FS3_GAMEPAD_CIRCLE = 0x0020,
    FS3_GAMEPAD_CROSS = 0x0040,
    FS3_GAMEPAD_SQUARE = 0x0080
} FS3_BUTTON, *PFS3_BUTTON;

///-------------------------------------------------------------------------------------------------
/// <summary>Defines FireShock 3 compatible pressure sensitive button states.</summary>
///
/// <remarks>Benjamin, 08.10.2016.</remarks>
///-------------------------------------------------------------------------------------------------
typedef struct _FS3_PRESSURE_STATE
{
    UCHAR Up;
    UCHAR Right;
    UCHAR Down;
    UCHAR Left;

    UCHAR LeftShoulder;
    UCHAR RightShoulder;

    UCHAR Triangle;
    UCHAR Circle;
    UCHAR Cross;
    UCHAR Square;

} FS3_PRESSURE_STATE, *PFS3_PRESSURE_STATE;

///-------------------------------------------------------------------------------------------------
/// <summary>Defines an FireShock 3 input state.</summary>
///
/// <remarks>Benjamin, 08.10.2016.</remarks>
///-------------------------------------------------------------------------------------------------
typedef struct _FS3_GAMEPAD_STATE
{
    UCHAR LeftThumbX;
    UCHAR LeftThumbY;
    UCHAR RightThumbX;
    UCHAR RightThumbY;
    USHORT Buttons;
    UCHAR PsButton;
    UCHAR LeftTrigger;
    UCHAR RightTrigger;
    FS3_PRESSURE_STATE Pressure;

} FS3_GAMEPAD_STATE, *PFS3_GAMEPAD_STATE;

typedef struct _FS3_REQUEST_REPORT
{
    IN ULONG Size;

    IN ULONG SerialNo;

    IN OUT FS3_GAMEPAD_STATE State;

} FS3_REQUEST_REPORT, *PFS3_REQUEST_REPORT;

VOID FORCEINLINE FS3_REQUEST_REPORT_INIT(
    IN OUT PFS3_REQUEST_REPORT Request,
    IN ULONG SerialNo)
{
    RtlZeroMemory(Request, sizeof(FS3_REQUEST_REPORT));

    Request->Size = sizeof(FS3_REQUEST_REPORT);
    Request->SerialNo = SerialNo;
}

typedef struct _FS_REQUEST_SETTINGS
{
    IN ULONG Size;

    IN ULONG SerialNo;

    IN OUT FS_DEVICE_SETTINGS Settings;

} FS_REQUEST_SETTINGS, *PFS_REQUEST_SETTINGS;

VOID FORCEINLINE FS_REQUEST_SETTINGS_INIT(
    IN OUT PFS_REQUEST_SETTINGS Request,
    IN ULONG SerialNo)
{
    RtlZeroMemory(Request, sizeof(FS_REQUEST_SETTINGS));

    Request->Size = sizeof(FS_REQUEST_SETTINGS);
    Request->SerialNo = SerialNo;
}

typedef struct _FS_SUBMIT_SETTINGS
{
    IN ULONG Size;

    IN ULONG SerialNo;

    IN FS_DEVICE_SETTINGS Settings;

} FS_SUBMIT_SETTINGS, *PFS_SUBMIT_SETTINGS;

VOID FORCEINLINE FS_SUBMIT_SETTINGS_INIT(
    IN OUT PFS_SUBMIT_SETTINGS Request,
    IN ULONG SerialNo)
{
    RtlZeroMemory(Request, sizeof(FS_SUBMIT_SETTINGS));

    Request->Size = sizeof(FS_SUBMIT_SETTINGS);
    Request->SerialNo = SerialNo;
}

