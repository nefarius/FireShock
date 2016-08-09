#include "fireshock.h"
#include <wdf.h>
#include <usb.h>
#include <usbdlib.h>
#include <wdfusb.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FireShockEvtDevicePrepareHardware)
#endif

NTSTATUS FireShockEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    NTSTATUS  status = STATUS_SUCCESS;
    PDEVICE_CONTEXT pDeviceContext = NULL;

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    KdPrint(("FireShockEvtDevicePrepareHardware called\n"));

    pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

    if (pDeviceContext->UsbDevice == NULL)
    {
        status = WdfUsbTargetDeviceCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &pDeviceContext->UsbDevice);
        if (!NT_SUCCESS(status))
        {
            KdPrint(("WdfUsbTargetDeviceCreate failed with status 0x%X\n", status));
            return status;
        }
    }

    return status;
}

NTSTATUS FireShockEvtDeviceD0Entry(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    PDEVICE_CONTEXT pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

    UNREFERENCED_PARAMETER(PreviousState);

    KdPrint(("FireShockEvtDeviceD0Entry called\n"));

    NTSTATUS status;
    ULONG transferred;
    WDF_MEMORY_DESCRIPTOR transferBuffer;
    UCHAR buffer[64] = { 0 };

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&transferBuffer, buffer, 64);

    WDF_USB_CONTROL_SETUP_PACKET packet;
    WDF_USB_CONTROL_SETUP_PACKET_INIT_CLASS(
        &packet,
        BmRequestHostToDevice,
        BMREQUEST_TO_INTERFACE,
        0x01,
        0x03F2,
        0);

    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
        pDeviceContext->UsbDevice,
        WDF_NO_HANDLE,
        NULL,
        &packet,
        &transferBuffer,
        &transferred);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfUsbTargetDeviceSendControlTransferSynchronously failed with status 0x%X\n", status));
        return status;
    }

    KdPrint(("(%02d) MAC: %02X:%02X:%02X:%02X:%02X\n",
        transferred,
        buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]));

    return STATUS_SUCCESS;
}

