/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_FireShock,
    0xc02225a0,0x94e1,0x4886,0x8b,0x08,0x31,0x75,0x3f,0x1c,0xdb,0xab);
// {c02225a0-94e1-4886-8b08-31753f1cdbab}
