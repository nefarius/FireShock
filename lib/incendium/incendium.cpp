// incendium.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "incendium.h"
#include <SetupAPI.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <initguid.h>
#include <public.h>


INCENDIUM_API DWORD Fs3GetControllerCount()
{
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { 0 };
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);
    DWORD memberIndex = 0;

    auto hControlDevice = CreateFile(TEXT("\\\\.\\FireShockFilter"),
        GENERIC_READ, // Only read access
        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
        nullptr, // no SECURITY_ATTRIBUTES structure
        OPEN_EXISTING, // No special create flags
        0, // No special attributes
        nullptr); // No template file

    // No FireShock device found
    if (hControlDevice == INVALID_HANDLE_VALUE) {
        return memberIndex;
    }

    auto deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_FIRESHOCK, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    // Count interface instances
    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVINTERFACE_FIRESHOCK, memberIndex, &deviceInterfaceData)) {
        memberIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return memberIndex;
}

