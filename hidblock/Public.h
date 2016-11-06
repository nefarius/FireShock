/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    driver and application

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_hidblock,
    0xa0b02523,0x0ada,0x4de6,0xa6,0x9d,0xcd,0x88,0x82,0xd5,0xee,0x11);
// {a0b02523-0ada-4de6-a69d-cd8882d5ee11}
