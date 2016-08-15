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

typedef struct _DEVICE_CONTEXT
{
    // Our WMI data generated from fireshock.mof
    FireShockDeviceInformation WmiInstance;

    UNICODE_STRING PdoName;

    WDFUSBDEVICE UsbDevice;

	DS_DEVICE_TYPE DeviceType;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FireShockDeviceInformation, InstanceGetInfo)

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

