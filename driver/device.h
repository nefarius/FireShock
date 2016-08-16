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

// {2409EA50-9ECA-410E-AC9E-F9AC798C4D9C}
DEFINE_GUID(GUID_DEVINTERFACE_FIRESHOCK,
    0x2409ea50, 0x9eca, 0x410e, 0xac, 0x9e, 0xf9, 0xac, 0x79, 0x8c, 0x4d, 0x9c);

typedef struct _DEVICE_CONTEXT
{
    WDFUSBDEVICE UsbDevice;

	DS_DEVICE_TYPE DeviceType;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetCommonContext)

typedef struct _DS3_DEVICE_CONTEXT
{
	BOOLEAN Enabled;

	WDFTIMER OutputReportTimer;

	UCHAR OutputReportBuffer[DS3_HID_OUTPUT_REPORT_SIZE];

    WDFUSBINTERFACE DefaultInterface;

    WDFUSBPIPE InterruptReadPipe;

} DS3_DEVICE_CONTEXT, *PDS3_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DS3_DEVICE_CONTEXT, Ds3GetContext)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL EvtIoInternalDeviceControl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_READ EvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE EvtIoWrite;

