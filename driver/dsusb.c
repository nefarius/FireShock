#include "fireshock.h"

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

        if (length >= 41)
        {
            Ds3GetConfigurationDescriptorType(Buffer, 41);
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
