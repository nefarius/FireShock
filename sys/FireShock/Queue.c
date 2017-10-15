/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    User-mode Driver Framework 2

--*/

#include "driver.h"
#include "queue.tmh"

NTSTATUS
FireShockQueueInitialize(
    _In_ WDFDEVICE Device
)
/*++

Routine Description:


     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;

    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel
    );

    queueConfig.EvtIoDeviceControl = FireShockEvtIoDeviceControl;
    queueConfig.EvtIoStop = FireShockEvtIoStop;
    queueConfig.EvtIoRead = FireShockEvtIoRead;
    queueConfig.EvtIoWrite = FireShockEvtIoWrite;

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

NTSTATUS
FireShockIoReadQueueInitialize(
    WDFDEVICE Device
)
{
    PDEVICE_CONTEXT         pDeviceContext;
    NTSTATUS                status;
    WDF_IO_QUEUE_CONFIG     queueConfig;

    pDeviceContext = DeviceGetContext(Device);

    WDF_IO_QUEUE_CONFIG_INIT(
        &queueConfig,
        WdfIoQueueDispatchManual
    );

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &pDeviceContext->IoReadQueue
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    return status;
}

VOID
FireShockEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
    NTSTATUS                        status = STATUS_SUCCESS;
    size_t                          bufferLength;
    size_t                          transferred = 0;
    PDEVICE_CONTEXT                 pDeviceContext;
    PFIRESHOCK_GET_HOST_BD_ADDR     pGetHostAddr;
    PFIRESHOCK_GET_DEVICE_BD_ADDR   pGetDeviceAddr;
    PFIRESHOCK_SET_HOST_BD_ADDR     pSetHostAddr;
    PFIRESHOCK_GET_DEVICE_TYPE      pGetDeviceType;

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_QUEUE,
        "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d",
        Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);

    pDeviceContext = DeviceGetContext(WdfIoQueueGetDevice(Queue));

    switch (IoControlCode)
    {
#pragma region IOCTL_FIRESHOCK_GET_HOST_BD_ADDR

    case IOCTL_FIRESHOCK_GET_HOST_BD_ADDR:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE, "IOCTL_FIRESHOCK_GET_HOST_BD_ADDR");

        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(FIRESHOCK_GET_HOST_BD_ADDR),
            (LPVOID)&pGetHostAddr,
            &bufferLength);

        if (NT_SUCCESS(status) && OutputBufferLength == sizeof(FIRESHOCK_GET_HOST_BD_ADDR))
        {
            transferred = OutputBufferLength;
            RtlCopyMemory(&pGetHostAddr->Host, &pDeviceContext->HostAddress, sizeof(BD_ADDR));
        }

        break;

#pragma endregion

#pragma region IOCTL_FIRESHOCK_GET_DEVICE_BD_ADDR

    case IOCTL_FIRESHOCK_GET_DEVICE_BD_ADDR:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE, "IOCTL_FIRESHOCK_GET_DEVICE_BD_ADDR");

        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(FIRESHOCK_GET_DEVICE_BD_ADDR),
            (LPVOID)&pGetDeviceAddr,
            &bufferLength);

        if (NT_SUCCESS(status) && OutputBufferLength == sizeof(FIRESHOCK_GET_DEVICE_BD_ADDR))
        {
            transferred = OutputBufferLength;
            RtlCopyMemory(&pGetDeviceAddr->Device, &pDeviceContext->DeviceAddress, sizeof(BD_ADDR));
        }

        break;

#pragma endregion

#pragma region IOCTL_FIRESHOCK_SET_HOST_BD_ADDR

    case IOCTL_FIRESHOCK_SET_HOST_BD_ADDR:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE, "IOCTL_FIRESHOCK_SET_HOST_BD_ADDR");

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(FIRESHOCK_SET_HOST_BD_ADDR),
            (LPVOID)&pSetHostAddr,
            &bufferLength);

        if (NT_SUCCESS(status) && InputBufferLength == sizeof(FIRESHOCK_SET_HOST_BD_ADDR))
        {
            UCHAR controlBuffer[SET_HOST_BD_ADDR_CONTROL_BUFFER_LENGTH];
            RtlZeroMemory(controlBuffer, SET_HOST_BD_ADDR_CONTROL_BUFFER_LENGTH);

            RtlCopyMemory(&controlBuffer[2], &pSetHostAddr->Host, sizeof(BD_ADDR));

            status = SendControlRequest(
                pDeviceContext,
                BmRequestHostToDevice,
                BmRequestClass,
                SetReport,
                Ds3FeatureHostAddress,
                0,
                controlBuffer,
                SET_HOST_BD_ADDR_CONTROL_BUFFER_LENGTH);

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
                    "Setting host address failed with %!STATUS!", status);
                break;
            }

            RtlCopyMemory(&pDeviceContext->HostAddress, &pSetHostAddr->Host, sizeof(BD_ADDR));
        }

        break;

#pragma endregion

#pragma region IOCTL_FIRESHOCK_GET_DEVICE_TYPE

    case IOCTL_FIRESHOCK_GET_DEVICE_TYPE:

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_QUEUE, "IOCTL_FIRESHOCK_GET_DEVICE_TYPE");

        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(FIRESHOCK_GET_DEVICE_TYPE),
            (LPVOID)&pGetDeviceType,
            &bufferLength);

        if (NT_SUCCESS(status) && OutputBufferLength == sizeof(FIRESHOCK_GET_DEVICE_TYPE))
        {
            transferred = OutputBufferLength;
            pGetDeviceType->DeviceType = pDeviceContext->DeviceType;
        }

        break;

#pragma endregion
    }

    WdfRequestCompleteWithInformation(Request, status, transferred);
}

VOID
FireShockEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_QUEUE,
        "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d",
        Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it either postpones further processing
    //   of the request and calls WdfRequestStopAcknowledge, or it calls WdfRequestComplete
    //   with a completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //  
    //   The driver must call WdfRequestComplete only once, to either complete or cancel
    //   the request. To ensure that another thread does not call WdfRequestComplete
    //   for the same request, the EvtIoStop callback must synchronize with the driver's
    //   other event callback functions, for instance by using interlocked operations.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time. For example, the driver might
    // take no action for requests that are completed in one of the driver’s request handlers.
    //

    return;
}

VOID FireShockEvtIoRead(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     Length
)
{
    NTSTATUS            status;
    PDEVICE_CONTEXT     pDeviceContext;

    UNREFERENCED_PARAMETER(Length);

    pDeviceContext = DeviceGetContext(WdfIoQueueGetDevice(Queue));

    status = WdfRequestForwardToIoQueue(Request, pDeviceContext->IoReadQueue);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE, "WdfRequestForwardToIoQueue failed with status %!STATUS!", status);
        WdfRequestComplete(Request, status);
    }
}

VOID FireShockEvtIoWrite(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     Length
)
{
    NTSTATUS            status;
    PDEVICE_CONTEXT     pDeviceContext;
    LPVOID              buffer;
    size_t              bufferLength;
    size_t              transferred = 0;

    pDeviceContext = DeviceGetContext(WdfIoQueueGetDevice(Queue));

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        status = WdfRequestRetrieveInputBuffer(
            Request,
            DS3_HID_OUTPUT_REPORT_SIZE,
            &buffer,
            &bufferLength);

        if (NT_SUCCESS(status) && Length == bufferLength)
        {
            status = SendControlRequest(
                pDeviceContext,
                BmRequestHostToDevice,
                BmRequestClass,
                SetReport,
                USB_SETUP_VALUE(HidReportRequestTypeOutput, HidReportRequestIdOne),
                0,
                buffer,
                (ULONG)bufferLength);

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
                    "SendControlRequest failed with status %!STATUS!",
                    status);
                break;
            }

            transferred = bufferLength;
        }

        break;
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, transferred);
}