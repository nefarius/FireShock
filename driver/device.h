/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    device.h

Abstract:

    This module contains the Windows Driver Framework Device object
    handlers for the fireshock filter driver.

Environment:

    Kernel mode

--*/

//
// The device context performs the same job as
// a WDM device extension in the driver framework
//
#include "ds.h"
#include "driver.h"

#define NTDEVICE_NAME_STRING      L"\\Device\\FireShockFilter"
#define SYMBOLIC_NAME_STRING      L"\\DosDevices\\FireShockFilter"

typedef struct _DEVICE_CONTEXT
{
    WDFUSBDEVICE UsbDevice;

    DS_DEVICE_TYPE DeviceType;

    ULONG DeviceIndex;

    VIGEM_INTERFACE_STANDARD VigemInterface;

    BOOLEAN VigemAvailable;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetCommonContext)

typedef struct _DS3_DEVICE_CONTEXT
{
    WDFTIMER OutputReportTimer;

    WDFTIMER InputEnableTimer;

    UCHAR OutputReportBuffer[DS3_HID_OUTPUT_REPORT_SIZE];

    FS3_GAMEPAD_STATE InputState;

} DS3_DEVICE_CONTEXT, *PDS3_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DS3_DEVICE_CONTEXT, Ds3GetContext)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL EvtIoInternalDeviceControl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FilterEvtIoDeviceControl;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtCleanupCallback;

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

