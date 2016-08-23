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
    UCHAR Buffer[20];
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


    auto deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_FIRESHOCK, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVINTERFACE_FIRESHOCK, memberIndex, &deviceInterfaceData))
    {
        while (TRUE)
        {
            FS3_REQUEST_REPORT req;
            FS3_REQUEST_REPORT_INIT(&req, memberIndex);

            if (!DeviceIoControl(hControlDevice,
                IOCTL_FIRESHOCK_FS3_REQUEST_REPORT,
                &req, req.Size,
                &req, req.Size,
                &bytes, nullptr)) {
                printf("Ioctl to ToasterFilter device failed: 0x%X\n", GetLastError());
            }
            else {
                printf("Pressure Circle: %3d\r", req.State.Pressure.Circle);
            }

            Sleep(2);
        }

        memberIndex++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return 0;
}

