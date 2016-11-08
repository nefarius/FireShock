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


//
// Sends a custom buffer to the device's control endpoint.
// 
NTSTATUS SendControlRequest(
    WDFDEVICE Device,
    BYTE Request,
    USHORT Value,
    USHORT Index,
    PVOID Buffer,
    size_t BufferLength,
    PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    WDFCONTEXT CompletionContext)
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
        KdPrint((DRIVERNAME "WdfRequestCreate failed with status 0x%X\n", status));
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
        KdPrint((DRIVERNAME "WdfMemoryCreate failed with status 0x%X\n", status));
        return status;
    }

    status = WdfMemoryCopyFromBuffer(
        transferBuffer,
        0,
        Buffer,
        BufferLength);

    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "WdfMemoryCopyFromBuffer failed with status 0x%X\n", status));
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
        KdPrint((DRIVERNAME "WdfUsbTargetDeviceFormatRequestForControlTransfer failed with status 0x%X\n", status));
        return status;
    }

    WdfRequestSetCompletionRoutine(
        controlRequest,
        CompletionRoutine,
        CompletionContext);

    if (!WdfRequestSend(
        controlRequest,
        WdfUsbTargetDeviceGetIoTarget(pDeviceContext->UsbDevice),
        WDF_NO_SEND_OPTIONS))
    {
        KdPrint((DRIVERNAME "WdfRequestSend failed\n"));
    }

    return status;
}

//
// Returns the device's configuration descriptor.
// 
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
            return STATUS_NOT_IMPLEMENTED;
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
        return STATUS_NOT_IMPLEMENTED;
    }

    return STATUS_SUCCESS;
}

//
// Returns the device's HID report descriptor.
// 
NTSTATUS GetDescriptorFromInterface(PURB urb, PDEVICE_CONTEXT pCommon)
{
    NTSTATUS                                    status = STATUS_INVALID_PARAMETER;
    struct _URB_CONTROL_DESCRIPTOR_REQUEST*     pRequest = &urb->UrbControlDescriptorRequest;
    PUCHAR                                      Buffer = (PUCHAR)pRequest->TransferBuffer;

    KdPrint((DRIVERNAME ">> >> >> _URB_CONTROL_DESCRIPTOR_REQUEST: Buffer Length %d\n", pRequest->TransferBufferLength));

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
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    return status;
}

//
// Gets called when the upper driver sent a bulk or interrupt OUT request.
// 
// The translation of the output report occurs here.
// 
NTSTATUS ParseBulkOrInterruptTransfer(PURB Urb, WDFDEVICE Device)
{
    NTSTATUS                status;
    PDEVICE_CONTEXT         pDeviceContext;
    PDS3_DEVICE_CONTEXT     pDs3Context;
    PUCHAR                  Buffer;

    pDeviceContext = GetCommonContext(Device);
    Buffer = (PUCHAR)Urb->UrbBulkOrInterruptTransfer.TransferBuffer;

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        pDs3Context = Ds3GetContext(Device);

        //
        // Copy rumble bytes from interrupt to control endpoint buffer.
        // 
        // Caution: the control buffer doesn't have a report ID, so it
        // has to be skipped on the interrupt buffer when getting copied.
        // 
        RtlCopyBytes(&pDs3Context->OutputReportBuffer[1], &Buffer[2], 4);

        status = STATUS_SUCCESS;

        break;
    default:

        status = STATUS_NOT_IMPLEMENTED;

        break;
    }

    return status;
}

//
// A DS3-related enable packet request has been completed.
// 
void Ds3EnableRequestCompleted(
    _In_ WDFREQUEST                     Request,
    _In_ WDFIOTARGET                    Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT                     Context
)
{
    NTSTATUS                status;
    WDFDEVICE               hDevice = Context;
    PDS3_DEVICE_CONTEXT     pDs3Context;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    status = WdfRequestGetStatus(Request);

    KdPrint((DRIVERNAME "Ds3EnableRequestCompleted called with status 0x%X\n", status));

    // On successful delivery...
    if (NT_SUCCESS(status))
    {
        pDs3Context = Ds3GetContext(hDevice);

        // ...we can stop sending the enable packet...
        WdfTimerStop(pDs3Context->InputEnableTimer, FALSE);
        // ...and start sending the output report (mainly rumble & LED states)
        WdfTimerStart(pDs3Context->OutputReportTimer, WDF_REL_TIMEOUT_IN_MS(DS3_OUTPUT_REPORT_SEND_DELAY));
    }
    else
    {
        KdPrint((DRIVERNAME "Ds3EnableRequestCompleted failed with status 0x%X\n", status));
    }

    // Free memory
    WdfObjectDelete(Params->Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Buffer);
    WdfObjectDelete(Request);
}

//
// A DS3-related output packet request has been completed.
// 
void Ds3OutputRequestCompleted(
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
        KdPrint((DRIVERNAME "Ds3OutputRequestCompleted failed with status 0x%X\n", status));
    }

    // Free memory
    WdfObjectDelete(Params->Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Buffer);
    WdfObjectDelete(Request);
}

//
// Gets called when the lower driver completed a bulk or interrupt request.
// 
// The translation of the input report occurs here.
// 
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
    PDS4_DEVICE_CONTEXT         pDs4Context;
    PXUSB_SUBMIT_REPORT         pXusbReport;
    PDS4_REPORT                 pDs4Report;

    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);

    status = WdfRequestGetStatus(Request);

    KdPrint((DRIVERNAME "BulkOrInterruptTransferCompleted called with status 0x%X\n", status));

    // Nothing to do if the lower driver failed, complete end exit
    if (!NT_SUCCESS(status))
    {
        WdfRequestComplete(Request, status);
        return;
    }

    // Get context, URB and buffers
    pDeviceContext = GetCommonContext(device);
    urb = URB_FROM_IRP(WdfRequestWdmGetIrp(Request));
    transferBuffer = (PUCHAR)urb->UrbBulkOrInterruptTransfer.TransferBuffer;
    transferBufferLength = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
    pXusbReport = &pDeviceContext->ViGEm.XusbSubmitReport;

    XUSB_SUBMIT_REPORT_INIT(pXusbReport, pDeviceContext->ViGEm.Serial);

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        pDs3Context = Ds3GetContext(device);

        // Report ID
        pDs3Context->InputReport[0] = transferBuffer[0];

        // Prepare D-Pad
        pDs3Context->InputReport[5] &= ~0xF; // Clear lower 4 bits

        // Translate D-Pad to HAT format
        switch (transferBuffer[2] & ~0xF)
        {
        case 0x10: // N
            pDs3Context->InputReport[5] |= 0 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
            break;
        case 0x30: // NE
            pDs3Context->InputReport[5] |= 1 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
            break;
        case 0x20: // E
            pDs3Context->InputReport[5] |= 2 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
            break;
        case 0x60: // SE
            pDs3Context->InputReport[5] |= 3 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
            break;
        case 0x40: // S
            pDs3Context->InputReport[5] |= 4 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
            break;
        case 0xC0: // SW
            pDs3Context->InputReport[5] |= 5 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
            break;
        case 0x80: // W
            pDs3Context->InputReport[5] |= 6 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
            break;
        case 0x90: // NW
            pDs3Context->InputReport[5] |= 7 & 0xF;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
            break;
        default: // Released
            pDs3Context->InputReport[5] |= 8 & 0xF;
            break;
        }

        // Prepare face buttons
        pDs3Context->InputReport[5] &= ~0xF0; // Clear upper 4 bits
        // Set face buttons
        pDs3Context->InputReport[5] |= transferBuffer[3] & 0xF0;

        // Thumb axes
        pDs3Context->InputReport[1] = transferBuffer[6]; // LTX
        pDs3Context->InputReport[2] = transferBuffer[7]; // LTY
        pDs3Context->InputReport[3] = transferBuffer[8]; // RTX
        pDs3Context->InputReport[4] = transferBuffer[9]; // RTY

        // Remaining buttons
        pDs3Context->InputReport[6] &= ~0xFF; // Clear all 8 bits
        pDs3Context->InputReport[6] |= (transferBuffer[2] & 0xF);
        pDs3Context->InputReport[6] |= (transferBuffer[3] & 0xF) << 4;

        // Trigger axes
        pDs3Context->InputReport[8] = transferBuffer[18];
        pDs3Context->InputReport[9] = transferBuffer[19];

        // PS button
        pDs3Context->InputReport[7] = transferBuffer[4];

        // D-Pad (pressure)
        pDs3Context->InputReport[10] = transferBuffer[14];
        pDs3Context->InputReport[11] = transferBuffer[15];
        pDs3Context->InputReport[12] = transferBuffer[16];
        pDs3Context->InputReport[13] = transferBuffer[17];

        // Shoulders (pressure)
        pDs3Context->InputReport[14] = transferBuffer[20];
        pDs3Context->InputReport[15] = transferBuffer[21];

        // Face buttons (pressure)
        pDs3Context->InputReport[16] = transferBuffer[22];
        pDs3Context->InputReport[17] = transferBuffer[23];
        pDs3Context->InputReport[18] = transferBuffer[24];
        pDs3Context->InputReport[19] = transferBuffer[25];

        /* Cache gamepad state for sideband communication
         * Skip first byte since it's the report ID we don't need */
        RtlCopyBytes(&pDs3Context->InputState, &pDs3Context->InputReport[1], sizeof(FS3_GAMEPAD_STATE));

        // Translate FS3 buttons to XUSB buttons
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_SELECT) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_BACK;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_LEFT_THUMB) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_RIGHT_THUMB) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_RIGHT_THUMB;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_START) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_START;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_LEFT_SHOULDER) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_RIGHT_SHOULDER) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_TRIANGLE) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_Y;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_CIRCLE) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_B;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_CROSS) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_A;
        if (pDs3Context->InputState.Buttons & FS3_GAMEPAD_SQUARE) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_X;

        // PS to Guide button
        if (pDs3Context->InputState.PsButton & 0x01) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_GUIDE;

        // FS3 to XUSB Trigger axes
        pXusbReport->Report.bLeftTrigger = pDs3Context->InputState.LeftTrigger;
        pXusbReport->Report.bRightTrigger = pDs3Context->InputState.RightTrigger;

        // FS3 to XUSB Thumb axes
        pXusbReport->Report.sThumbLX = ScaleAxis(pDs3Context->InputState.LeftThumbX, FALSE);
        pXusbReport->Report.sThumbLY = ScaleAxis(pDs3Context->InputState.LeftThumbY, TRUE);
        pXusbReport->Report.sThumbRX = ScaleAxis(pDs3Context->InputState.RightThumbX, FALSE);
        pXusbReport->Report.sThumbRY = ScaleAxis(pDs3Context->InputState.RightThumbY, TRUE);

        if (pDeviceContext->Settings.FsHidInputEnabled)
        {
            RtlCopyBytes(pDs3Context->LastReport, pDs3Context->InputReport, DS3_ORIGINAL_HID_REPORT_SIZE);
        }
        else
        {
            RtlCopyBytes(pDs3Context->InputReport, pDs3Context->LastReport, DS3_ORIGINAL_HID_REPORT_SIZE);
        }

        // Copy back modified buffer to request buffer
        RtlCopyBytes(transferBuffer, pDs3Context->InputReport, transferBufferLength);

        break;
    case DualShock4:

        pDs4Context = Ds4GetContext(device);
        pDs4Report = (PDS4_REPORT)&transferBuffer[1];

        // Translate FS4 D-Pad to XUSB format
        switch (pDs4Report->wButtons & 0xF)
        {
        case Ds4DpadN:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
            break;
        case Ds4DpadNE:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
            break;
        case Ds4DpadE:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
            break;
        case Ds4DpadSE:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
            break;
        case Ds4DpadS:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
            break;
        case Ds4DpadSW:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
            break;
        case Ds4DpadW:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
            break;
        case Ds4DpadNW:
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
            pXusbReport->Report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
            break;
        }

        // Translate FS4 buttons to XUSB buttons
        if (pDs4Report->wButtons & Ds4ThumbR) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_RIGHT_THUMB;
        if (pDs4Report->wButtons & Ds4ThumbL) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
        if (pDs4Report->wButtons & Ds4Options) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_START;
        if (pDs4Report->wButtons & Ds4Share) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_BACK;
        if (pDs4Report->wButtons & Ds4ShoulderR) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
        if (pDs4Report->wButtons & Ds4ShoulderL) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
        if (pDs4Report->wButtons & Ds4Triangle) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_Y;
        if (pDs4Report->wButtons & Ds4Circle) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_B;
        if (pDs4Report->wButtons & Ds4Cross) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_A;
        if (pDs4Report->wButtons & Ds4Square) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_X;

        // PS to Guide button
        if (pDs4Report->bSpecial & Ds4Ps) pXusbReport->Report.wButtons |= XUSB_GAMEPAD_GUIDE;

        // FS4 to XUSB Trigger axes
        pXusbReport->Report.bLeftTrigger = pDs4Report->bTriggerL;
        pXusbReport->Report.bRightTrigger = pDs4Report->bTriggerR;

        // FS4 to XUSB Thumb axes
        pXusbReport->Report.sThumbLX = ScaleAxis(pDs4Report->bThumbLX, FALSE);
        pXusbReport->Report.sThumbLY = ScaleAxis(pDs4Report->bThumbLY, TRUE);
        pXusbReport->Report.sThumbRX = ScaleAxis(pDs4Report->bThumbRX, FALSE);
        pXusbReport->Report.sThumbRY = ScaleAxis(pDs4Report->bThumbRY, TRUE);

        if (pDeviceContext->Settings.FsHidInputEnabled)
        {
            RtlCopyBytes(&pDs4Context->LastReport, pDs4Report, sizeof(DS4_REPORT));
        }
        else
        {
            RtlCopyBytes(pDs4Report, &pDs4Context->LastReport, sizeof(DS4_REPORT));
        }

        break;
    default:
        break;
    }

    if (pDeviceContext->ViGEm.IoTarget)
    {
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&pDeviceContext->ViGEm.XusbSubmitReportBuffer, pXusbReport, pXusbReport->Size);

        status = WdfIoTargetSendInternalIoctlSynchronously(
            pDeviceContext->ViGEm.IoTarget,
            NULL,
            IOCTL_XUSB_SUBMIT_REPORT,
            &pDeviceContext->ViGEm.XusbSubmitReportBuffer,
            NULL,
            NULL,
            NULL
        );

        KdPrint((DRIVERNAME "WdfIoTargetSendIoctlSynchronously failed with status 0x%X\n", status));
    }

    // Complete upper request
    WdfRequestComplete(Request, status);
}

