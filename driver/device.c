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


#include "FireShock.h"
#include <usbioctl.h>


WDFCOLLECTION   FilterDeviceCollection;
WDFWAITLOCK     FilterDeviceCollectionLock;
WDFDEVICE       ControlDevice = NULL;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FireShockEvtDeviceAdd)
#pragma alloc_text(PAGE, FilterShutdown)
#pragma alloc_text(PAGE, EvtCleanupCallback)
#endif


NTSTATUS
FireShockEvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
)
{
    WDF_OBJECT_ATTRIBUTES           attributes;
    NTSTATUS                        status;
    WDFDEVICE                       device;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    WDF_IO_QUEUE_CONFIG             ioQueueConfig;
    PDEVICE_CONTEXT                 pDeviceContext;


    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    //
    // Configure the device as a filter driver
    //
    WdfFdoInitSetFilter(DeviceInit);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetPowerPolicyOwnership(DeviceInit, FALSE);

    //
    // Set PNP & power callbacks.
    // 
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    pnpPowerCallbacks.EvtDevicePrepareHardware = FireShockEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = FireShockEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = FireShockEvtDeviceD0Exit;

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

    attributes.EvtCleanupCallback = EvtCleanupCallback;

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint((DRIVERNAME "FireShock: WdfDeviceCreate, Error %x\n", status));
        return status;
    }

    pDeviceContext = GetCommonContext(device);

    //
    // Create default queue.
    // 
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
        WdfIoQueueDispatchParallel);

    ioQueueConfig.EvtIoInternalDeviceControl = EvtIoInternalDeviceControl;

    __analysis_assume(ioQueueConfig.EvtIoStop != 0);
    status = WdfIoQueueCreate(device,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE // pointer to default queue
    );
    __analysis_assume(ioQueueConfig.EvtIoStop == 0);

    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    // 
    // Expose custom interface to simplify device enumeration
    // 
    // Note: we can't directly use this interface but exposing 
    // it on every instance allows us to conveniently enumerate
    // all filter instances from any user-mode application.
    // 
    // Actual filter manipulation is achieved via the control device.
    // 
    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_FIRESHOCK, NULL);
    if (!NT_SUCCESS(status)) {
        KdPrint((DRIVERNAME "WdfDeviceCreateDeviceInterface failed status 0x%x\n", status));
        return status;
    }

    //
    // Add this device to the FilterDevice collection.
    //
    WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

    // Store index of ourself in the collection (zero-based) for later use
    pDeviceContext->DeviceIndex = WdfCollectionGetCount(FilterDeviceCollection);

    //
    // WdfCollectionAdd takes a reference on the item object and removes
    // it when you call WdfCollectionRemove.
    //
    status = WdfCollectionAdd(FilterDeviceCollection, device);
    if (!NT_SUCCESS(status)) {
        KdPrint((DRIVERNAME "WdfCollectionAdd failed with status code 0x%x\n", status));
    }
    WdfWaitLockRelease(FilterDeviceCollectionLock);

    //
    // Get ViGEm interface
    // 
    AcquireViGEmInterface(device, VigemDosDeviceName);

    // 
    // Default settings
    // 
    ResetDeviceSettings(pDeviceContext);

    //
    // Create a control device
    //
    status = FilterCreateControlDevice(device);
    if (!NT_SUCCESS(status)) {
        KdPrint((DRIVERNAME "FilterCreateControlDevice failed with status 0x%x\n",
            status));
        //
        // Let us not fail AddDevice just because we weren't able to create the
        // control device.
        //
        status = STATUS_SUCCESS;
    }

    return status;
}

//
// Called on filter unload.
// 
#pragma warning(push)
#pragma warning(disable:28118) // this callback will run at IRQL=PASSIVE_LEVEL
_Use_decl_annotations_
VOID EvtCleanupCallback(
    _In_ WDFOBJECT Device
)
{
    ULONG   count;

    PAGED_CODE();

    KdPrint((DRIVERNAME "Entered FilterEvtDeviceContextCleanup\n"));

    WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

    count = WdfCollectionGetCount(FilterDeviceCollection);

    if (count == 1)
    {
        //
        // We are the last instance. So let us delete the control-device
        // so that driver can unload when the FilterDevice is deleted.
        // We absolutely have to do the deletion of control device with
        // the collection lock acquired because we implicitly use this
        // lock to protect ControlDevice global variable. We need to make
        // sure another thread doesn't attempt to create while we are
        // deleting the device.
        //
        FilterDeleteControlDevice((WDFDEVICE)Device);
    }

    WdfCollectionRemove(FilterDeviceCollection, Device);

    WdfWaitLockRelease(FilterDeviceCollectionLock);
}
#pragma warning(pop) // enable 28118 again

//
// Intercepts communication between HIDUSB.SYS and the USB PDO.
// 
VOID EvtIoInternalDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    NTSTATUS                        status = STATUS_INVALID_PARAMETER;
    WDFDEVICE                       hDevice;
    BOOLEAN                         ret;
    WDF_REQUEST_SEND_OPTIONS        options;
    PIRP                            irp;
    PURB                            urb;
    BOOLEAN                         processed = FALSE;
    PDEVICE_CONTEXT                 pDeviceContext;

    hDevice = WdfIoQueueGetDevice(Queue);
    irp = WdfRequestWdmGetIrp(Request);
    pDeviceContext = GetCommonContext(hDevice);

    if (pDeviceContext->DeviceType == Unknown)
    {
        goto ioForward;
    }

    switch (IoControlCode)
    {
    case IOCTL_INTERNAL_USB_SUBMIT_URB:

        urb = (PURB)URB_FROM_IRP(irp);

        switch (urb->UrbHeader.Function)
        {
        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:

            KdPrint((DRIVERNAME "URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER\n"));

            if (IS_INTERRUPT_IN(urb))
            {
                KdPrint((DRIVERNAME "Interrupt IN\n"));

                WdfRequestFormatRequestUsingCurrentType(Request);
                WdfRequestSetCompletionRoutine(Request, BulkOrInterruptTransferCompleted, hDevice);

                ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), WDF_NO_SEND_OPTIONS);

                if (ret == FALSE) {
                    status = WdfRequestGetStatus(Request);
                    KdPrint((DRIVERNAME "WdfRequestSend failed: 0x%x\n", status));
                    processed = TRUE;
                }
                else return;
            }
            else
            {
                KdPrint((DRIVERNAME "Interrupt OUT\n"));

                if (IS_DS4(pDeviceContext)) break;

                status = ParseBulkOrInterruptTransfer(urb, hDevice);

                processed = NT_SUCCESS(status);
            }

            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:

            KdPrint((DRIVERNAME "URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE\n"));

            switch (urb->UrbControlDescriptorRequest.DescriptorType)
            {
            case USB_CONFIGURATION_DESCRIPTOR_TYPE:

                KdPrint((DRIVERNAME "USB_CONFIGURATION_DESCRIPTOR_TYPE\n"));

                // Intercept and write back custom configuration descriptor
                if (IS_DS3(pDeviceContext))
                {
                    status = GetConfigurationDescriptorType(urb, pDeviceContext);
                    processed = NT_SUCCESS(status);
                }

                break;
            default:
                break;
            }

            break;

        case URB_FUNCTION_ABORT_PIPE:

            KdPrint((DRIVERNAME "URB_FUNCTION_ABORT_PIPE\n"));

            FilterShutdown(hDevice);

            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:

            KdPrint((DRIVERNAME "URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE\n"));

            // Intercept and write back custom HID report descriptor
            if (IS_DS3(pDeviceContext))
            {
                status = GetDescriptorFromInterface(urb, pDeviceContext);
                processed = NT_SUCCESS(status);
            }

            break;
        default:
            break;
        }

        break;
    default:
        break;
    }

    // The upper request was handled by the filter
    if (processed)
    {
        WdfRequestComplete(Request, status);
        return;
    }

ioForward:

    // The upper request gets forwarded to the lower driver
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        KdPrint((DRIVERNAME "WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }
}

//
// Performs clean-up tasks.
// 
void FilterShutdown(WDFDEVICE Device)
{
    PDEVICE_CONTEXT                 pDeviceContext;
    PDS3_DEVICE_CONTEXT             pDs3Context;

    PAGED_CODE();

    pDeviceContext = GetCommonContext(Device);

    if (pDeviceContext->DeviceType == DualShock3)
    {
        pDs3Context = Ds3GetContext(Device);

        if (pDs3Context)
        {
            WdfTimerStop(pDs3Context->OutputReportTimer, TRUE);
            WdfTimerStop(pDs3Context->InputEnableTimer, FALSE);
        }
    }
}

//
// Searches for ViGEm bus and tries to initialize direct-call interface.
// 
VOID AcquireViGEmInterface(WDFDEVICE Device, const UNICODE_STRING DeviceName)
{
    NTSTATUS                        status;
    WDF_IO_TARGET_OPEN_PARAMS       openParams;
    PDEVICE_CONTEXT                 pDeviceContext;
    WDF_OBJECT_ATTRIBUTES           attributes;

    pDeviceContext = GetCommonContext(Device);

    status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &pDeviceContext->ViGEm.IoTarget);
    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "WdfIoTargetCreate failed with status code 0x%x\n", status));
        return;
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_CREATE_BY_NAME(&openParams, &DeviceName, STANDARD_RIGHTS_ALL);

    status = WdfIoTargetOpen(pDeviceContext->ViGEm.IoTarget, &openParams);
    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "ViGEm interface not available: 0x%X\n", status));
        WdfObjectDelete(pDeviceContext->ViGEm.IoTarget);
        pDeviceContext->ViGEm.IoTarget = NULL;
        return;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    //
    // Create request for XUSB submission
    // 
    status = WdfRequestCreate(&attributes, pDeviceContext->ViGEm.IoTarget, &pDeviceContext->ViGEm.XusbSubmitReportRequest);
    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "WdfRequestCreate failed with status 0x%X\n", status));
        WdfObjectDelete(pDeviceContext->ViGEm.IoTarget);
        pDeviceContext->ViGEm.IoTarget = NULL;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    //
    // Create memory for XUSB submission
    // 
    status = WdfMemoryCreate(
        &attributes,
        NonPagedPool,
        0,
        sizeof(XUSB_SUBMIT_REPORT),
        &pDeviceContext->ViGEm.XusbSubmitReportBuffer,
        NULL);
    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVERNAME "WdfMemoryCreate failed with status 0x%X\n", status));
        WdfObjectDelete(pDeviceContext->ViGEm.IoTarget);
        pDeviceContext->ViGEm.IoTarget = NULL;
    }
}

//
// Reset current device's settings to its defaults.
// 
VOID ResetDeviceSettings(PDEVICE_CONTEXT Context)
{
    Context->Settings.FsHidInputEnabled = TRUE;
    Context->Settings.FsHidOutputEnabled = TRUE;
    Context->Settings.XusbEmulationEnabled = TRUE;
    Context->Settings.XusbHidInputEnabled = TRUE;
    Context->Settings.XusbHidOutputEnabled = TRUE;
}

VOID XusbNotificationCallback(IN PVOID Context, IN UCHAR LargeMotor, IN UCHAR SmallMotor, IN UCHAR LedNumber)
{
    WDFDEVICE               device = Context;
    PDEVICE_CONTEXT         pDeviceContext;
    PDS3_DEVICE_CONTEXT     pDs3Context;

    UNREFERENCED_PARAMETER(LedNumber);

    KdPrint((DRIVERNAME "XusbNotificationCallback called\n"));

    pDeviceContext = GetCommonContext(device);

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        pDs3Context = Ds3GetContext(device);

        pDs3Context->OutputReportBuffer[2] = (SmallMotor > 0) ? 0x01 : 0x00;
        pDs3Context->OutputReportBuffer[4] = LargeMotor;

        break;
    default:
        return;
    }
}

