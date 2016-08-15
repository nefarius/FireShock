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

//
// Our drivers generated include from fireshock.mof
// See makefile.inc for wmi commands
//
#include "fireshockmof.h"

// Our drivers modules includes
#include "device.h"
#include "power.h"
#include "ds.h"
#include "dsusb.h"

#define DRIVERNAME "FireShock.sys: "

//
// WDFDRIVER Object Events
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD FireShockEvtDeviceAdd;

