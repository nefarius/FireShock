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
    WDFDEVICE                       device;
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

                    WdfTimerStart(pDs3Context->OutputReportTimer, WDF_REL_TIMEOUT_IN_MS(DS3_OUTPUT_REPORT_SEND_DELAY));
                }
            }

            status = SendInterruptInRequest(hDevice, Request);
            status = NT_SUCCESS(status) ? STATUS_PENDING : status;

            KdPrint(("UPPER BUFLEN: %d\n", urb->UrbBulkOrInterruptTransfer.TransferBufferLength));

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

                // Intercept and write back custom configuration descriptor
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

            if (pDeviceContext->DeviceType == DualShock3)
            {
                WdfTimerStop(pDs3Context->OutputReportTimer, FALSE);
            }

            break;

        case URB_FUNCTION_CLASS_INTERFACE:

            KdPrint((">> >> URB_FUNCTION_CLASS_INTERFACE\n"));

            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:

            KdPrint((">> >> URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE\n"));

            // Intercept and write back custom HID report descriptor
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

    // The upper request is pending and gets completed later
    if (status == STATUS_PENDING)
    {
        return;
    }

    // The upper request was handled by the filter
    if (processed)
    {
        WdfRequestComplete(Request, status);
        return;
    }

    // The upper request gets forwarded to the lower driver
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,
        WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(hDevice), &options);

    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        KdPrint(("WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }
}

