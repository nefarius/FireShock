/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.


Module Name:

    device.c

Abstract:

    This module contains the Windows Driver Framework Device object
    handlers for the fireshock filter driver.

Environment:

    Kernel mode

--*/

#include "FireShock.h"
#include <usbioctl.h>
#include <usb.h>
#include <wdfusb.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FireShockEvtDeviceAdd)
#endif

NTSTATUS
FireShockEvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
)
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent to be part of the device stack as a filter.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES           attributes;
    NTSTATUS                        status;
    PDEVICE_CONTEXT                 pDeviceContext;
    WDFDEVICE                       device;
    WDFMEMORY                       memory;
    size_t                          bufferLength;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    WDF_IO_QUEUE_CONFIG             ioQueueConfig;


    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    //
    // Configure the device as a filter driver
    //
    WdfFdoInitSetFilter(DeviceInit);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    pnpPowerCallbacks.EvtDevicePrepareHardware = FireShockEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = FireShockEvtDeviceD0Entry;

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireShock: WdfDeviceCreate, Error %x\n", status));
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig,
        WdfIoQueueDispatchParallel);

    ioQueueConfig.EvtIoInternalDeviceControl = EvtIoInternalDeviceControl;

    status = WdfIoQueueCreate(device,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE // pointer to default queue
    );
    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    //
    // Driver Framework always zero initializes an objects context memory
    //
    pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(device);


    //
    // Initialize our WMI support
    //
    status = WmiInitialize(device, pDeviceContext);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireShock: Error initializing WMI 0x%x\n", status));
        return status;
    }

    //
    // In order to send ioctls to our PDO, we have open to open it
    // by name so that we have a valid filehandle (fileobject).
    // When we send ioctls using the IoTarget, framework automatically 
    // sets the filobject in the stack location.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    //
    // By parenting it to device, we don't have to worry about
    // deleting explicitly. It will be deleted along witht the device.
    //
    attributes.ParentObject = device;

    status = WdfDeviceAllocAndQueryProperty(device,
        DevicePropertyPhysicalDeviceObjectName,
        NonPagedPool,
        &attributes,
        &memory);

    if (!NT_SUCCESS(status)) {
        KdPrint(("FireShock: WdfDeviceAllocAndQueryProperty failed 0x%x\n", status));
        return STATUS_UNSUCCESSFUL;
    }

    pDeviceContext->PdoName.Buffer = WdfMemoryGetBuffer(memory, &bufferLength);

    if (pDeviceContext->PdoName.Buffer == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    pDeviceContext->PdoName.MaximumLength = (USHORT)bufferLength;
    pDeviceContext->PdoName.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);

    return status;
}

VOID EvtIoInternalDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
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
    PDS3_DEVICE_CONTEXT             pDs3Context;

    hDevice = WdfIoQueueGetDevice(Queue);
    irp = WdfRequestWdmGetIrp(Request);
    pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(hDevice);
    pDs3Context = Ds3GetContext(hDevice);

    switch (IoControlCode)
    {
    case IOCTL_INTERNAL_USB_SUBMIT_URB:

        urb = (PURB)URB_FROM_IRP(irp);

        switch (urb->UrbHeader.Function)
        {
        case URB_FUNCTION_CONTROL_TRANSFER:

            KdPrint((">> >> URB_FUNCTION_CONTROL_TRANSFER\n"));

            break;

        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
        {
            KdPrint((">> >> URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER\n"));

            if (pDeviceContext->DeviceType == DualShock3 && !pDs3Context->Enabled)
            {
                status = Ds3Init(hDevice);
                if (NT_SUCCESS(status))
                {
                    pDs3Context->Enabled = TRUE;

                    WdfTimerStart(pDs3Context->OutputReportTimer, WDF_REL_TIMEOUT_IN_MS(10));
                }
            }

            WDFMEMORY transferBuffer;
            WDFREQUEST interruptRequest;
            WDF_OBJECT_ATTRIBUTES transferAttribs;

            status = WdfRequestCreate(
                WDF_NO_OBJECT_ATTRIBUTES,
                WdfDeviceGetIoTarget(hDevice),
                &interruptRequest
            );

            if (!NT_SUCCESS(status))
            {
                KdPrint(("WdfRequestCreate failed with status 0x%X\n", status));
                break;
            }

            WDF_OBJECT_ATTRIBUTES_INIT(&transferAttribs);

            transferAttribs.ParentObject = interruptRequest;

            status = WdfMemoryCreate(
                &transferAttribs,
                NonPagedPool,
                0,
                64,
                &transferBuffer,
                NULL);

            if (!NT_SUCCESS(status))
            {
                KdPrint(("WdfMemoryCreate failed with status 0x%X\n", status));
                break;
            }

            status = WdfUsbTargetPipeFormatRequestForRead(
                pDs3Context->InterruptReadPipe,
                interruptRequest,
                transferBuffer,
                NULL);

            if (!NT_SUCCESS(status))
            {
                KdPrint(("WdfUsbTargetPipeFormatRequestForRead failed with status 0x%X\n", status));
                break;
            }

            WdfRequestSetCompletionRoutine(
                interruptRequest,
                InterruptReadRequestCompletionRoutine,
                WDF_NO_SEND_OPTIONS);

            if (!WdfRequestSend(
                interruptRequest,
                WdfUsbTargetDeviceGetIoTarget(pDeviceContext->UsbDevice),
                NULL))
            {
                KdPrint(("WdfRequestSend failed\n"));
            }

            break;
        }
        case URB_FUNCTION_SELECT_CONFIGURATION:

            KdPrint((">> >> URB_FUNCTION_SELECT_CONFIGURATION\n"));

            break;

        case URB_FUNCTION_SELECT_INTERFACE:

            KdPrint((">> >> URB_FUNCTION_SELECT_INTERFACE\n"));

            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:

            KdPrint((">> >> URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE\n"));

            switch (urb->UrbControlDescriptorRequest.DescriptorType)
            {
            case USB_DEVICE_DESCRIPTOR_TYPE:

                KdPrint((">> >> >> USB_DEVICE_DESCRIPTOR_TYPE\n"));

                break;

            case USB_CONFIGURATION_DESCRIPTOR_TYPE:

                KdPrint((">> >> >> USB_CONFIGURATION_DESCRIPTOR_TYPE\n"));

                if (pDeviceContext->DeviceType == DualShock3)
                {
                    status = GetConfigurationDescriptorType(urb, pDeviceContext);
                    processed = TRUE;
                }

                break;

            case USB_STRING_DESCRIPTOR_TYPE:

                KdPrint((">> >> >> USB_STRING_DESCRIPTOR_TYPE\n"));

                break;
            case USB_INTERFACE_DESCRIPTOR_TYPE:

                KdPrint((">> >> >> USB_INTERFACE_DESCRIPTOR_TYPE\n"));

                break;

            case USB_ENDPOINT_DESCRIPTOR_TYPE:

                KdPrint((">> >> >> USB_ENDPOINT_DESCRIPTOR_TYPE\n"));

                break;

            default:
                KdPrint((">> >> >> Unknown descriptor type\n"));
                break;
            }

            KdPrint(("<< <<\n"));

            break;

        case URB_FUNCTION_GET_STATUS_FROM_DEVICE:

            KdPrint((">> >> URB_FUNCTION_GET_STATUS_FROM_DEVICE\n"));

            break;

        case URB_FUNCTION_ABORT_PIPE:

            KdPrint((">> >> URB_FUNCTION_ABORT_PIPE\n"));

            break;

        case URB_FUNCTION_CLASS_INTERFACE:

            KdPrint((">> >> URB_FUNCTION_CLASS_INTERFACE\n"));

            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:

            KdPrint((">> >> URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE\n"));

            if (pDeviceContext->DeviceType == DualShock3)
            {
                status = GetDescriptorFromInterface(urb, pDeviceContext);
                processed = TRUE;
            }

            break;

        default:
            KdPrint((">> >> Unknown function: 0x%X\n", urb->UrbHeader.Function));
            break;
        }

        break;
    }

    if (processed)
    {
        WdfRequestComplete(Request, status);
        return;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        KdPrint(("WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }
}

void ControlRequestCompletionRoutine(
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

    KdPrint(("ControlRequestCompletionRoutine called with status 0x%X\n", WdfRequestGetStatus(Request)));
}

void InterruptReadRequestCompletionRoutine(
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

    KdPrint(("InterruptReadRequestCompletionRoutine called with status 0x%X\n", WdfRequestGetStatus(Request)));

    NTSTATUS    status;
    PWDF_USB_REQUEST_COMPLETION_PARAMS usbCompletionParams;
    size_t      bytesRead = 0, buflen;

    status = Params->IoStatus.Status;

    usbCompletionParams = Params->Parameters.Usb.Completion;

    bytesRead = usbCompletionParams->Parameters.PipeRead.Length;

    PUCHAR Buffer = WdfMemoryGetBuffer(usbCompletionParams->Parameters.PipeRead.Buffer, &buflen);

    KdPrint(("INPUT: "));

    for (size_t i = 0; i < bytesRead; i++)
    {
        KdPrint(("0x%02X ", Buffer[i]));
    }

    KdPrint(("\n"));
}

