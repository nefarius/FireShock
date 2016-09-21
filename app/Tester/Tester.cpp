// Tester.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <SetupAPI.h>
#include <stdlib.h>
#include <winioctl.h>
#include <public.h>


int main()
{
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { 0 };
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);
    DWORD memberIndex = 0;
    DWORD status;
    DWORD bytes = 0;

    HANDLE hControlDevice = CreateFile(TEXT("\\\\.\\FireShockFilter"),
        GENERIC_READ|GENERIC_WRITE, // Only read access
        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
        NULL, // no SECURITY_ATTRIBUTES structure
        OPEN_EXISTING, // No special create flags
        0, // No special attributes
        NULL); // No template file

    if (hControlDevice != INVALID_HANDLE_VALUE)
        printf("Found device! 0x%X\n", GetLastError());
    else
        printf("Device not found! 0x%X\n", GetLastError());

    FS_REQUEST_SETTINGS reqSettings;
    FS_SUBMIT_SETTINGS subSettings;
    
    auto deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_FIRESHOCK, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVINTERFACE_FIRESHOCK, memberIndex, &deviceInterfaceData))
    {
        FS_REQUEST_SETTINGS_INIT(&reqSettings, memberIndex);

        status = DeviceIoControl(
            hControlDevice,
            IOCTL_FIRESHOCK_REQUEST_SETTINGS,
            &reqSettings, reqSettings.Size,
            &reqSettings, reqSettings.Size, &bytes, NULL);

        printf("Request result: %X\n", status);

        printf("FsHidInputEnabled: %X\n", reqSettings.Settings.FsHidInputEnabled);

        FS_SUBMIT_SETTINGS_INIT(&subSettings, memberIndex);

        subSettings.Settings = reqSettings.Settings;
        subSettings.Settings.FsHidInputEnabled = FALSE;

        status = DeviceIoControl(
            hControlDevice,
            IOCTL_FIRESHOCK_SUBMIT_SETTINGS,
            &subSettings, subSettings.Size,
            &subSettings, subSettings.Size, &bytes, NULL);

        printf("Submit result: %X\n", status);

        memberIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return 0;
}

