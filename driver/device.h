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


#include "ds.h"
#include "driver.h"

//
// Control device name
// 
#define NTDEVICE_NAME_STRING      L"\\Device\\FireShockFilter"
#define SYMBOLIC_NAME_STRING      L"\\DosDevices\\FireShockFilter"

//
// Data used in ViGEm interaction
// 
typedef struct _VIGEM_META
{
    //
    // Occupied device serial reported by ViGEm
    // 
    ULONG Serial;

    WDFIOTARGET IoTarget;

} VIGEM_META, *PVIGEM_META;

//
// Common device context
// 
typedef struct _DEVICE_CONTEXT
{
    //
    // USB Device object
    // 
    WDFUSBDEVICE UsbDevice;

    //
    // Device type
    // 
    DS_DEVICE_TYPE DeviceType;

    //
    // Device instance index
    // 
    ULONG DeviceIndex;

    //
    // ViGEm-specific data
    // 
    VIGEM_META ViGEm;
    
    //
    // Device settings
    // 
    FS_DEVICE_SETTINGS Settings;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetCommonContext)

//
// DualShock 3-specific context
// 
typedef struct _DS3_DEVICE_CONTEXT
{
    //
    // Timer for re-occurring output reports
    // 
    WDFTIMER OutputReportTimer;

    //
    // Timer for magic packet
    // 
    WDFTIMER InputEnableTimer;

    //
    // Raw output report buffer
    // 
    UCHAR OutputReportBuffer[DS3_HID_OUTPUT_REPORT_SIZE];

    //
    // Translated input state
    // 
    FS3_GAMEPAD_STATE InputState;

    //
    // Cached input report
    // 
    UCHAR LastReport[DS3_ORIGINAL_HID_REPORT_SIZE];

} DS3_DEVICE_CONTEXT, *PDS3_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DS3_DEVICE_CONTEXT, Ds3GetContext)

//
// DualShock 4-specific context
// 
typedef struct _DS4_DEVICE_CONTEXT
{
    //
    // Timer for re-occurring output reports
    // 
    WDFTIMER OutputReportTimer;

    //
    // Raw output report buffer
    // 
    UCHAR OutputReportBuffer[DS4_HID_OUTPUT_REPORT_SIZE];

    //
    // Cached input report
    // 
    DS4_REPORT LastReport;

} DS4_DEVICE_CONTEXT, *PDS4_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DS4_DEVICE_CONTEXT, Ds4GetContext)


EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL EvtIoInternalDeviceControl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FilterEvtIoDeviceControl;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtCleanupCallback;
EVT_WDF_IO_TARGET_QUERY_REMOVE EvtIoTargetQueryRemove;

_Must_inspect_result_
_Success_(return == STATUS_SUCCESS)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FilterCreateControlDevice(
    _In_ WDFDEVICE Device
);

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
FilterDeleteControlDevice(
    _In_ WDFDEVICE Device
);

VOID FilterShutdown(WDFDEVICE Device);

VOID AcquireViGEmInterface(WDFDEVICE Device, const UNICODE_STRING DeviceName);

VOID ResetDeviceSettings(PDEVICE_CONTEXT Context);

VOID XusbNotificationCallback(IN PVOID Context, IN UCHAR LargeMotor, IN UCHAR SmallMotor, IN UCHAR LedNumber);
