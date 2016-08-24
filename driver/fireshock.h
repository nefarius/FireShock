/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.


Module Name:

    FireShock.h

Abstract:

    This is the header for the Windows Driver Framework version
    of the fireshock filter driver.

Environment:

    Kernel mode

--*/

#include <ntddk.h>
#include <wdf.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <initguid.h>
#include <wdmguid.h>
#include <wdmsec.h> // for SDDLs
#include <usb.h>
#include <wdfusb.h>

// Our drivers modules includes
#include "public.h"
#include "device.h"
#include "power.h"
#include "ds.h"
#include "dsusb.h"


extern WDFCOLLECTION   FilterDeviceCollection;
extern WDFWAITLOCK     FilterDeviceCollectionLock;
extern WDFDEVICE       ControlDevice;

//
// WDFDRIVER Object Events
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD FireShockEvtDeviceAdd;

