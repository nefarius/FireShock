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

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FilterCreateControlDevice)
#pragma alloc_text(PAGE, FilterDeleteControlDevice)
#pragma alloc_text(PAGE, SidebandEvtIoDeviceControl)
#endif

//
// Creates the control device for sideband communication.
// 
_Use_decl_annotations_
NTSTATUS
FilterCreateControlDevice(
    WDFDEVICE Device
)
{
    PWDFDEVICE_INIT             pInit = NULL;
    WDFDEVICE                   controlDevice = NULL;
    WDF_OBJECT_ATTRIBUTES       controlAttributes;
    WDF_IO_QUEUE_CONFIG         ioQueueConfig;
    BOOLEAN                     bCreate = FALSE;
    NTSTATUS                    status;
    WDFQUEUE                    queue;
    DECLARE_CONST_UNICODE_STRING(ntDeviceName, NTDEVICE_NAME_STRING);
    DECLARE_CONST_UNICODE_STRING(symbolicLinkName, SYMBOLIC_NAME_STRING);

    PAGED_CODE();

    //
    // First find out whether any ControlDevice has been created. If the
    // collection has more than one device then we know somebody has already
    // created or in the process of creating the device.
    //
    WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

    if (WdfCollectionGetCount(FilterDeviceCollection) == 1) {
        bCreate = TRUE;
    }

    WdfWaitLockRelease(FilterDeviceCollectionLock);

    if (!bCreate) {
        //
        // Control device is already created. So return success.
        //
        return STATUS_SUCCESS;
    }

    KdPrint((DRIVERNAME "Creating Control Device\n"));

    //
    //
    // In order to create a control device, we first need to allocate a
    // WDFDEVICE_INIT structure and set all properties.
    //
    pInit = WdfControlDeviceInitAllocate(
        WdfDeviceGetDriver(Device),
        &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R
    );

    if (pInit == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Error;
    }

    //
    // Set exclusive to false so that more than one app can talk to the
    // control device simultaneously.
    //
    WdfDeviceInitSetExclusive(pInit, FALSE);

    status = WdfDeviceInitAssignName(pInit, &ntDeviceName);

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    //
    // Specify the size of device context
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&controlAttributes);
    status = WdfDeviceCreate(&pInit,
        &controlAttributes,
        &controlDevice);
    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    //
    // Create a symbolic link for the control object so that usermode can open
    // the device.
    //

    status = WdfDeviceCreateSymbolicLink(controlDevice,
        &symbolicLinkName);

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    //
    // Configure the default queue associated with the control device object
    // to be Serial so that request passed to EvtIoDeviceControl are serialized.
    //

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
        WdfIoQueueDispatchSequential);

    ioQueueConfig.EvtIoDeviceControl = SidebandEvtIoDeviceControl;

    //
    // Framework by default creates non-power managed queues for
    // filter drivers.
    //
    status = WdfIoQueueCreate(controlDevice,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue // pointer to default queue
    );
    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    //
    // Control devices must notify WDF when they are done initializing.   I/O is
    // rejected until this call is made.
    //
    WdfControlFinishInitializing(controlDevice);

    ControlDevice = controlDevice;

    return STATUS_SUCCESS;

Error:

    if (pInit != NULL) {
        WdfDeviceInitFree(pInit);
    }

    if (controlDevice != NULL) {
        //
        // Release the reference on the newly created object, since
        // we couldn't initialize it.
        //
        WdfObjectDelete(controlDevice);
        controlDevice = NULL;
    }

    return status;
}

//
// Deletes the control device.
// 
_Use_decl_annotations_
VOID
FilterDeleteControlDevice(
    WDFDEVICE Device
)
{
    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();

    KdPrint((DRIVERNAME "Deleting Control Device\n"));

    if (ControlDevice) {
        WdfObjectDelete(ControlDevice);
        ControlDevice = NULL;
    }
}

//
// Handles requests sent to the sideband control device.
// 
#pragma warning(push)
#pragma warning(disable:28118) // this callback will run at IRQL=PASSIVE_LEVEL
_Use_decl_annotations_
VOID SidebandEvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    NTSTATUS                    status = STATUS_INVALID_PARAMETER;
    size_t                      length = 0;
    WDFDEVICE                   hFilterDevice;
    PFS3_REQUEST_REPORT         pFs3Report = NULL;
    PDEVICE_CONTEXT             pDeviceContext;
    PDS3_DEVICE_CONTEXT         pDs3Context;
    PFS_REQUEST_SETTINGS        pReqSettings;
    PFS_SUBMIT_SETTINGS         pSubSettings;

    UNREFERENCED_PARAMETER(Queue);

    PAGED_CODE();

    KdPrint((DRIVERNAME "Ioctl received into filter control object.\n"));

    WdfWaitLockAcquire(FilterDeviceCollectionLock, NULL);

    // Our control device is gone, abandon ship!
    if (WdfCollectionGetCount(FilterDeviceCollection) == 0)
    {
        WdfWaitLockRelease(FilterDeviceCollectionLock);
        WdfRequestCompleteWithInformation(Request, STATUS_NO_SUCH_DEVICE, 0);
        return;
    }

    switch (IoControlCode)
    {
    case IOCTL_FIRESHOCK_FS3_REQUEST_REPORT:

        KdPrint((DRIVERNAME "IOCTL_FIRESHOCK_FS3_REQUEST_REPORT\n"));

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(FS3_REQUEST_REPORT), (PVOID)&pFs3Report, &length);

        // Validate input buffer size
        if (!NT_SUCCESS(status)
            || (sizeof(FS3_REQUEST_REPORT) != pFs3Report->Size)
            || (length != InputBufferLength))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        hFilterDevice = WdfCollectionGetItem(FilterDeviceCollection, pFs3Report->SerialNo);
        pDeviceContext = GetCommonContext(hFilterDevice);
        pDs3Context = Ds3GetContext(hFilterDevice);

        if (!pDeviceContext || pDeviceContext->DeviceType != DualShock3)
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (!pDs3Context)
        {
            // Context unavailable, 
            status = STATUS_DEVICE_NOT_READY;
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(FS3_REQUEST_REPORT), (PVOID)&pFs3Report, &length);

        // Validate output buffer size
        if (!NT_SUCCESS(status)
            || (sizeof(FS3_REQUEST_REPORT) != pFs3Report->Size)
            || (length != OutputBufferLength))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        pFs3Report->State = pDs3Context->InputState;

        break;

    case IOCTL_FIRESHOCK_REQUEST_SETTINGS:

        KdPrint((DRIVERNAME "IOCTL_FIRESHOCK_REQUEST_SETTINGS\n"));

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(FS_REQUEST_SETTINGS), (PVOID)&pReqSettings, &length);

        // Validate input buffer size
        if (!NT_SUCCESS(status)
            || (sizeof(FS_REQUEST_SETTINGS) != pReqSettings->Size)
            || (length != InputBufferLength))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        hFilterDevice = WdfCollectionGetItem(FilterDeviceCollection, pReqSettings->SerialNo);
        pDeviceContext = GetCommonContext(hFilterDevice);

        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(FS_REQUEST_SETTINGS), (PVOID)&pReqSettings, &length);

        // Validate output buffer size
        if (!NT_SUCCESS(status)
            || (sizeof(FS_REQUEST_SETTINGS) != pReqSettings->Size)
            || (length != OutputBufferLength))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        pReqSettings->Settings = pDeviceContext->Settings;

        break;

    case IOCTL_FIRESHOCK_SUBMIT_SETTINGS:

        KdPrint((DRIVERNAME "IOCTL_FIRESHOCK_SUBMIT_SETTINGS\n"));

        status = WdfRequestRetrieveInputBuffer(Request, sizeof(FS_SUBMIT_SETTINGS), (PVOID)&pSubSettings, &length);

        // Validate input buffer size
        if (!NT_SUCCESS(status)
            || (sizeof(FS_SUBMIT_SETTINGS) != pSubSettings->Size)
            || (length != InputBufferLength))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        hFilterDevice = WdfCollectionGetItem(FilterDeviceCollection, pSubSettings->SerialNo);
        pDeviceContext = GetCommonContext(hFilterDevice);

        pDeviceContext->Settings = pSubSettings->Settings;

        break;
    default: break;
    }

    WdfWaitLockRelease(FilterDeviceCollectionLock);

    WdfRequestCompleteWithInformation(Request, status, length);
}
#pragma warning(pop) // enable 28118 again

