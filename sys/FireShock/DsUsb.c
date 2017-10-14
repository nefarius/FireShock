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
            BmRequestToInterface,
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

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DSUSB,
            "WdfUsbTargetDeviceSendControlTransferSynchronously failed with status %!STATUS! (%d)\n",
            status, bytesTransferred);
    }

    return status;
}

NTSTATUS
DsUsbConfigContReaderForInterruptEndPoint(
    _In_ WDFDEVICE Device
)
{
    WDF_USB_CONTINUOUS_READER_CONFIG contReaderConfig;
    NTSTATUS status;
    PDEVICE_CONTEXT pDeviceContext;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DSUSB, "%!FUNC! Entry");

    pDeviceContext = DeviceGetContext(Device);

    WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&contReaderConfig,
        DsUsbEvtUsbInterruptPipeReadComplete,
        Device,    // Context
        INTERRUPT_IN_BUFFER_LENGTH);   // TransferLength

    contReaderConfig.EvtUsbTargetPipeReadersFailed = DsUsbEvtUsbInterruptReadersFailed;

    //
    // Reader requests are not posted to the target automatically.
    // Driver must explicitly call WdfIoTargetStart to kick start the
    // reader.  In this sample, it's done in D0Entry.
    // By default, framework queues two requests to the target
    // endpoint. Driver can configure up to 10 requests with CONFIG macro.
    //
    status = WdfUsbTargetPipeConfigContinuousReader(pDeviceContext->InterruptReadPipe,
        &contReaderConfig);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DSUSB,
            "WdfUsbTargetPipeConfigContinuousReader failed %x\n",
            status);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DSUSB, "%!FUNC! Exit");

    return status;
}

VOID
DsUsbEvtUsbInterruptPipeReadComplete(
    WDFUSBPIPE  Pipe,
    WDFMEMORY   Buffer,
    size_t      NumBytesTransferred,
    WDFCONTEXT  Context
)
{
    NTSTATUS            status;
    PDEVICE_CONTEXT     pDeviceContext;
    WDFREQUEST          request;
    size_t              bufferLength;
    LPVOID              reqBuffer;

    UNREFERENCED_PARAMETER(Pipe);
    UNREFERENCED_PARAMETER(Context);

    pDeviceContext = DeviceGetContext(Context);

    status = WdfIoQueueRetrieveNextRequest(pDeviceContext->IoReadQueue, &request);

    if (NT_SUCCESS(status))
    {
        status = WdfRequestRetrieveOutputBuffer(request,
            NumBytesTransferred, &reqBuffer, &bufferLength);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DSUSB,
                "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!", status);
            WdfRequestComplete(request, status);
            return;
        }

        RtlCopyMemory(reqBuffer, WdfMemoryGetBuffer(Buffer, NULL), NumBytesTransferred);

        WdfRequestCompleteWithInformation(request, status, NumBytesTransferred);
    }
}

BOOLEAN
DsUsbEvtUsbInterruptReadersFailed(
    _In_ WDFUSBPIPE Pipe,
    _In_ NTSTATUS Status,
    _In_ USBD_STATUS UsbdStatus
)
{
    UNREFERENCED_PARAMETER(UsbdStatus);
    UNREFERENCED_PARAMETER(Pipe);

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DSUSB,
        "DsUsbEvtUsbInterruptReadersFailed called with status %!STATUS!",
        Status);

    return TRUE;
}
