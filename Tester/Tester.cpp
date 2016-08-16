// Tester.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <SetupAPI.h>
#include <stdlib.h>


int main()
{
    HANDLE hControlDevice = INVALID_HANDLE_VALUE;


    hControlDevice = CreateFile(TEXT("\\\\.\\FireShockFilter"),
        GENERIC_READ, // Only read access
        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
        NULL, // no SECURITY_ATTRIBUTES structure
        OPEN_EXISTING, // No special create flags
        0, // No special attributes
        NULL); // No template file

    if (hControlDevice != INVALID_HANDLE_VALUE)
        printf("Found device! 0x%X\n", GetLastError());
    else
        printf("Device not found! 0x%X\n", GetLastError());

    getchar();

    UCHAR Buffer[10];
    DWORD read = 0;

    printf("Read request...\n");
    auto retval = ReadFile(hControlDevice, Buffer, 10, &read, nullptr);
    printf("retval = %d, read = %d, error = 0x%X\n", retval, read, GetLastError());

    getchar();
    getchar();

    printf("Closing...\n");
    CloseHandle(hControlDevice);


    return 0;
}

