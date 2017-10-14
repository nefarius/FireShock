/*
MIT License

Copyright (c) 2017 Benjamin "Nefarius" Höglinger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "Driver.h"
#include "DsUsb.tmh"

//
// Sends a custom buffer to the device's control endpoint.
// 
NTSTATUS
SendControlRequest(
    _In_ PDEVICE_CONTEXT Context,
    _In_ BYTE Type,
    _In_ BYTE Request,
    _In_ USHORT Value,
    _In_ USHORT Index,
    _In_ PVOID Buffer,
    _In_ ULONG BufferLength)
{
    NTSTATUS                        status;
    WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS        sendOptions;
    WDF_MEMORY_DESCRIPTOR           memDesc;
    ULONG                           bytesTransferred;

    WDF_REQUEST_SEND_OPTIONS_INIT(
        &sendOptions,
        WDF_REQUEST_SEND_OPTION_TIMEOUT
    );

    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
        &sendOptions,
        DEFAULT_CONTROL_TRANSFER_TIMEOUT
    );

    switch (Type)
    {
    case BmRequestClass:
        WDF_USB_CONTROL_SETUP_PACKET_INIT_CLASS(&controlSetupPacket,
            BmRequestHostToDevice,
            BmRequestToDevice,
            Request,
            Value,
            Index);
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
        Buffer,
        BufferLength);

    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
        Context->UsbDevice,
        WDF_NO_HANDLE,
        &sendOptions,
        &controlSetupPacket,
        &memDesc,
        &bytesTransferred);

    if (!NT_SUCCESS(status)) {

        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DSUSB,
            "WdfUsbTargetDeviceSendControlTransferSynchronously: Failed - 0x%x (%d)\n", status, bytesTransferred);
    }

    return status;
}
