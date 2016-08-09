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
    WDFMEMORY transferBuffer;
    UCHAR hidCommandEnable[4] = { 0x42, 0x0C, 0x00, 0x00 };
    WDF_OBJECT_ATTRIBUTES transferAttribs;

    WDFREQUEST controlRequest;
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
        4,
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
        hidCommandEnable, 
        4);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfMemoryCopyFromBuffer failed with status 0x%X\n", status));
        return status;
    }

    WDF_USB_CONTROL_SETUP_PACKET packet;
    WDF_USB_CONTROL_SETUP_PACKET_INIT_CLASS(
        &packet,
        BmRequestHostToDevice,
        BMREQUEST_TO_INTERFACE,
        0x09,
        0x03F4,
        0);

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
        CompletionRoutine,
        NULL);

    if (!WdfRequestSend(
        controlRequest,
        WdfUsbTargetDeviceGetIoTarget(pDeviceContext->UsbDevice),
        NULL))
    {
        KdPrint(("WdfRequestSend failed\n"));
    }

    return STATUS_SUCCESS;
}

void CompletionRoutine(
    _In_ WDFREQUEST                     Request,
    _In_ WDFIOTARGET                    Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT                     Context
)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Params);
    UNREFERENCED_PARAMETER(Context);

    KdPrint(("CompletionRoutine called with status 0x%X\n", WdfRequestGetStatus(Request)));

    

    //KdPrint(("MAC: %02X:%02X:%02X:%02X:%02X\n", buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]));
}

