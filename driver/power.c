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

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    KdPrint(("FireShockEvtDevicePrepareHardware called\n"));

    return status;
}

NTSTATUS FireShockEvtDeviceD0Entry(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    UNREFERENCED_PARAMETER(Device); 
    UNREFERENCED_PARAMETER(PreviousState);

    KdPrint(("FireShockEvtDeviceD0Entry called\n"));

    

    return STATUS_SUCCESS;
}

