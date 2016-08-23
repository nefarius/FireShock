/*
MIT License

Copyright (c) 2016 Benjamin "Nefarius" Höglinger

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


#include "fireshock.h"
#include <wdfusb.h>

NTSTATUS SendControlRequest(
    WDFDEVICE Device,
    BYTE Request,
    USHORT Value,
    USHORT Index,
    PVOID Buffer,
    size_t BufferLength)
{
    NTSTATUS                        status;
    WDFMEMORY                       transferBuffer;
    PDEVICE_CONTEXT                 pDeviceContext;
    WDFREQUEST                      controlRequest;
    WDF_USB_CONTROL_SETUP_PACKET    packet;
    WDF_OBJECT_ATTRIBUTES           transferAttribs;

    pDeviceContext = GetCommonContext(Device);

    status = WdfRequestCreate(
        WDF_NO_OBJECT_ATTRIBUTES,
        WdfDeviceGetIoTarget(Device),
        &controlRequest);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfRequestCreate failed with status 0x%X\n", status));
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&transferAttribs);

    transferAttribs.ParentObject = controlRequest;

    status = WdfMemoryCreate(
        &transferAttribs,
        NonPagedPool,
        0,
        BufferLength,
        &transferBuffer,
        NULL);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfMemoryCreate failed with status 0x%X\n", status));
        return status;
    }

    status = WdfMemoryCopyFromBuffer(
        transferBuffer,
        0,
        Buffer,
        BufferLength);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfMemoryCopyFromBuffer failed with status 0x%X\n", status));
        return status;
    }

    WDF_USB_CONTROL_SETUP_PACKET_INIT_CLASS(
        &packet,
        BmRequestHostToDevice,
        BMREQUEST_TO_INTERFACE,
        Request,
        Value,
        Index);

    status = WdfUsbTargetDeviceFormatRequestForControlTransfer(
        pDeviceContext->UsbDevice,
        controlRequest,
        &packet,
        transferBuffer,
        NULL);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfUsbTargetDeviceFormatRequestForControlTransfer failed with status 0x%X\n", status));
        return status;
    }

    WdfRequestSetCompletionRoutine(
        controlRequest,
        ControlRequestCompletionRoutine,
        NULL);

    if (!WdfRequestSend(
        controlRequest,
        WdfUsbTargetDeviceGetIoTarget(pDeviceContext->UsbDevice),
        WDF_NO_SEND_OPTIONS))
    {
        KdPrint(("WdfRequestSend failed\n"));
    }

    return status;
}

NTSTATUS GetConfigurationDescriptorType(PURB urb, PDEVICE_CONTEXT pCommon)
{
    PUCHAR Buffer = (PUCHAR)urb->UrbControlDescriptorRequest.TransferBuffer;

    // First request just gets required buffer size back
    if (urb->UrbControlDescriptorRequest.TransferBufferLength == sizeof(USB_CONFIGURATION_DESCRIPTOR))
    {
        ULONG length = sizeof(USB_CONFIGURATION_DESCRIPTOR);

        switch (pCommon->DeviceType)
        {
        case DualShock3:

            Ds3GetConfigurationDescriptorType(Buffer, length);

            break;
        default:
            return STATUS_UNSUCCESSFUL;
        }
    }

    ULONG length = urb->UrbControlDescriptorRequest.TransferBufferLength;

    // Second request can store the whole descriptor
    switch (pCommon->DeviceType)
    {
    case DualShock3:

        if (length >= DS3_CONFIGURATION_DESCRIPTOR_SIZE)
        {
            Ds3GetConfigurationDescriptorType(Buffer, DS3_CONFIGURATION_DESCRIPTOR_SIZE);
        }

        break;
    default:
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS GetDescriptorFromInterface(PURB urb, PDEVICE_CONTEXT pCommon)
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    struct _URB_CONTROL_DESCRIPTOR_REQUEST* pRequest = &urb->UrbControlDescriptorRequest;
    PUCHAR Buffer = (PUCHAR)pRequest->TransferBuffer;

    KdPrint((">> >> >> _URB_CONTROL_DESCRIPTOR_REQUEST: Buffer Length %d\n", pRequest->TransferBufferLength));

    switch (pCommon->DeviceType)
    {
    case DualShock3:

        if (pRequest->TransferBufferLength >= DS3_HID_REPORT_DESCRIPTOR_SIZE)
        {
            Ds3GetDescriptorFromInterface(Buffer);
            status = STATUS_SUCCESS;
        }

        break;
    default:
        break;
    }

    return status;
}

void ControlRequestCompletionRoutine(
    _In_ WDFREQUEST                     Request,
    _In_ WDFIOTARGET                    Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT                     Context
)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    status = WdfRequestGetStatus(Request);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("ControlRequestCompletionRoutine failed with status 0x%X\n", status));
    }

    // Free memory
    WdfObjectDelete(Params->Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Buffer);
    WdfObjectDelete(Request);
}

void BulkOrInterruptTransferCompleted(
    _In_ WDFREQUEST                     Request,
    _In_ WDFIOTARGET                    Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT                     Context
)
{
    NTSTATUS                    status;
    WDFDEVICE                   device = Context;
    PURB                        urb;
    PUCHAR                      transferBuffer;
    ULONG                       transferBufferLength;
    PDEVICE_CONTEXT             pDeviceContext;
    PDS3_DEVICE_CONTEXT         pDs3Context;

    UCHAR upperBuffer[DS3_ORIGINAL_HID_REPORT_SIZE];
    RtlZeroMemory(upperBuffer, DS3_ORIGINAL_HID_REPORT_SIZE);

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    status = WdfRequestGetStatus(Request);

    if (!NT_SUCCESS(status))
    {
        WdfRequestComplete(Request, status);
        return;
    }

    KdPrint(("BulkOrInterruptTransferCompleted called with status 0x%X\n", status));

    pDeviceContext = GetCommonContext(device);
    urb = URB_FROM_IRP(WdfRequestWdmGetIrp(Request));
    transferBuffer = (PUCHAR)urb->UrbBulkOrInterruptTransfer.TransferBuffer;
    transferBufferLength = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        pDs3Context = Ds3GetContext(device);

        // Report ID
        upperBuffer[0] = transferBuffer[0];

        // Prepare D-Pad
        upperBuffer[5] &= ~0xF; // Clear lower 4 bits

        // Translate D-Pad to HAT format
        switch (transferBuffer[2] & ~0xF)
        {
        case 0x10: // N
            upperBuffer[5] |= 0 & 0xF;
            break;
        case 0x30: // NE
            upperBuffer[5] |= 1 & 0xF;
            break;
        case 0x20: // E
            upperBuffer[5] |= 2 & 0xF;
            break;
        case 0x60: // SE
            upperBuffer[5] |= 3 & 0xF;
            break;
        case 0x40: // S
            upperBuffer[5] |= 4 & 0xF;
            break;
        case 0xC0: // SW
            upperBuffer[5] |= 5 & 0xF;
            break;
        case 0x80: // W
            upperBuffer[5] |= 6 & 0xF;
            break;
        case 0x90: // NW
            upperBuffer[5] |= 7 & 0xF;
            break;
        default: // Released
            upperBuffer[5] |= 8 & 0xF;
            break;
        }

        // Prepare face buttons
        upperBuffer[5] &= ~0xF0; // Clear upper 4 bits
        // Set face buttons
        upperBuffer[5] |= transferBuffer[3] & 0xF0;

        // Thumb axes
        upperBuffer[1] = transferBuffer[6]; // LTX
        upperBuffer[2] = transferBuffer[7]; // LTY
        upperBuffer[3] = transferBuffer[8]; // RTX
        upperBuffer[4] = transferBuffer[9]; // RTY

        // Remaining buttons
        upperBuffer[6] &= ~0xFF; // Clear all 8 bits
        upperBuffer[6] |= transferBuffer[2] & 0xF;
        upperBuffer[6] |= (transferBuffer[3] & 0xF) << 4;

        // Trigger axes
        upperBuffer[8] = transferBuffer[18];
        upperBuffer[9] = transferBuffer[19];

        // PS button
        upperBuffer[7] = transferBuffer[4];

        // D-Pad (pressure)
        upperBuffer[10] = transferBuffer[14];
        upperBuffer[11] = transferBuffer[15];
        upperBuffer[12] = transferBuffer[16];
        upperBuffer[13] = transferBuffer[17];

        // Shoulders (pressure)
        upperBuffer[14] = transferBuffer[20];
        upperBuffer[15] = transferBuffer[21];

        // Face buttons (pressure)
        upperBuffer[16] = transferBuffer[22];
        upperBuffer[17] = transferBuffer[23];
        upperBuffer[18] = transferBuffer[24];
        upperBuffer[19] = transferBuffer[25];

        /* Cache gamepad state for sideband communication 
         * Skip first byte since it's the report ID we don't need */
        RtlCopyBytes(&pDs3Context->InputState, upperBuffer + 1, sizeof(FS3_GAMEPAD_STATE));

        break;
    default:
        break;
    }

    // Copy back modified buffer to request buffer
    RtlCopyBytes(transferBuffer, upperBuffer, transferBufferLength);

    // Complete upper request
    WdfRequestComplete(Request, status);
}

