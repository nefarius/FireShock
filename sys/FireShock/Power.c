/*
MIT License

Copyright (c) 2017 Benjamin "Nefarius" Höglinger

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


#include "Driver.h"
#include "power.tmh"
#include <evntrace.h>

#ifdef _KERNEL_MODE
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FireShockEvtDevicePrepareHardware)
#pragma alloc_text(PAGE, FireShockEvtDeviceD0Entry)
#pragma alloc_text(PAGE, FireShockEvtDeviceD0Exit)
#endif
#endif


_Use_decl_annotations_
NTSTATUS
FireShockEvtDevicePrepareHardware(
    WDFDEVICE  Device,
    WDFCMRESLIST  ResourcesRaw,
    WDFCMRESLIST  ResourcesTranslated
)
{
    NTSTATUS                                status;
    PDEVICE_CONTEXT                         pDeviceContext;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS     configParams;
    USB_DEVICE_DESCRIPTOR                   deviceDescriptor;
    PDS3_DEVICE_CONTEXT                     pDs3Context;
    PDS4_DEVICE_CONTEXT                     pDs4Context;
    WDF_OBJECT_ATTRIBUTES                   attributes;
    WDF_TIMER_CONFIG                        timerCfg;
    BOOLEAN                                 supported = FALSE;
    UCHAR                                   index;
    WDFUSBPIPE                              pipe;
    WDF_USB_PIPE_INFORMATION                pipeInfo;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Entry");

    pDeviceContext = DeviceGetContext(Device);

    if (pDeviceContext->UsbDevice == NULL) {

        status = WdfUsbTargetDeviceCreate(Device,
            WDF_NO_OBJECT_ATTRIBUTES,
            &pDeviceContext->UsbDevice
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER,
                "WdfUsbTargetDeviceCreateWithParameters failed %!STATUS!", status);
            return status;
        }
    }

    // 
    // Use device descriptor to identify the device
    // 
    WdfUsbTargetDeviceGetDeviceDescriptor(pDeviceContext->UsbDevice, &deviceDescriptor);

#pragma region DualShock 3, Navigation Controller detection

    // 
    // Device is a DualShock 3 or Move Navigation Controller
    // 
    if (deviceDescriptor.idVendor == DS3_VENDOR_ID
        && (deviceDescriptor.idProduct == DS3_PRODUCT_ID || deviceDescriptor.idProduct == PS_MOVE_NAVI_PRODUCT_ID))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER,
            "PlayStation 3 compatible controller detected");

        pDeviceContext->DeviceType = DualShock3;
        supported = TRUE;

        // Add DS3-specific context to device object
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DS3_DEVICE_CONTEXT);

        status = WdfObjectAllocateContext(Device, &attributes, (PVOID)&pDs3Context);
        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER,
                "WdfObjectAllocateContext failed status %!STATUS!", status);
            return status;
        }

        // 
        // Initial output state (rumble & LEDs off)
        // 
        // Note: no report ID because sent over control endpoint
        // 
        UCHAR DefaultOutputReport[DS3_HID_OUTPUT_REPORT_SIZE] =
        {
            0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF,
            0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00,
            0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        RtlCopyMemory(pDs3Context->OutputReportBuffer, DefaultOutputReport, DS3_HID_OUTPUT_REPORT_SIZE);

        // Set initial LED index to device arrival index (max. 4 possible for DS3)
        switch (pDeviceContext->DeviceIndex)
        {
        case 0:
            pDs3Context->OutputReportBuffer[DS3_OFFSET_LED_INDEX] |= DS3_OFFSET_LED_0;
            break;
        case 1:
            pDs3Context->OutputReportBuffer[DS3_OFFSET_LED_INDEX] |= DS3_OFFSET_LED_1;
            break;
        case 2:
            pDs3Context->OutputReportBuffer[DS3_OFFSET_LED_INDEX] |= DS3_OFFSET_LED_2;
            break;
        case 3:
            pDs3Context->OutputReportBuffer[DS3_OFFSET_LED_INDEX] |= DS3_OFFSET_LED_3;
            break;
        default:
            // TODO: what do we do in this case? Light animation?
            break;
        }

        // Initialize output report timer
        WDF_TIMER_CONFIG_INIT_PERIODIC(&timerCfg, Ds3OutputEvtTimerFunc, DS3_OUTPUT_REPORT_SEND_DELAY);
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

        attributes.ParentObject = Device;

        status = WdfTimerCreate(&timerCfg, &attributes, &pDs3Context->OutputReportTimer);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER,
                "Error creating output report timer %!STATUS!", status);
            return status;
        }
    }

#pragma endregion

#pragma region DualShock 4 (v1, v2), Wireless Adapter

    // 
    // Device is a DualShock 4 model 1, 2 or Wireless USB Adapter
    // 
    if (deviceDescriptor.idVendor == DS4_VENDOR_ID
        && (deviceDescriptor.idProduct == DS4_PRODUCT_ID
            || deviceDescriptor.idProduct == DS4_WIRELESS_ADAPTER_PRODUCT_ID
            || deviceDescriptor.idProduct == DS4_2_PRODUCT_ID))
    {
        pDeviceContext->DeviceType = DualShock4;
        supported = TRUE;

        //
        // Add DS4-specific context to device object
        //  
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DS4_DEVICE_CONTEXT);

        status = WdfObjectAllocateContext(Device, &attributes, (PVOID)&pDs4Context);
        if (!NT_SUCCESS(status))
        {
            KdPrint((DRIVERNAME "WdfObjectAllocateContext failed status 0x%x\n", status));
            return status;
        }

        // 
        // Initial output state (rumble & LEDs off)
        // 
        UCHAR DefaultOutputReport[DS4_HID_OUTPUT_REPORT_SIZE] =
        {
            0x05, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
            0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        RtlCopyMemory(pDs4Context->OutputReportBuffer, DefaultOutputReport, DS4_HID_OUTPUT_REPORT_SIZE);
    }

#pragma endregion

    if (!supported)
    {
        return STATUS_NOT_SUPPORTED;
    }

#pragma region USB Interface & Pipe settings

    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

    status = WdfUsbTargetDeviceSelectConfig(pDeviceContext->UsbDevice,
        WDF_NO_OBJECT_ATTRIBUTES,
        &configParams
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER,
            "WdfUsbTargetDeviceSelectConfig failed %!STATUS!", status);
        return status;
    }

    pDeviceContext->UsbInterface = configParams.Types.SingleInterface.ConfiguredUsbInterface;

    //
    // Get pipe handles
    //
    for (index = 0; index < WdfUsbInterfaceGetNumConfiguredPipes(pDeviceContext->UsbInterface); index++) {

        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

        pipe = WdfUsbInterfaceGetConfiguredPipe(
            pDeviceContext->UsbInterface,
            index, //PipeIndex,
            &pipeInfo
        );
        //
        // Tell the framework that it's okay to read less than
        // MaximumPacketSize
        //
        WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

        if (WdfUsbPipeTypeInterrupt == pipeInfo.PipeType &&
            WdfUsbTargetPipeIsInEndpoint(pipe)) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER,
                "InterruptReadPipe is 0x%p\n", pipe);
            pDeviceContext->InterruptReadPipe = pipe;
        }

        if (WdfUsbPipeTypeInterrupt == pipeInfo.PipeType &&
            WdfUsbTargetPipeIsOutEndpoint(pipe)) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER,
                "InterruptWritePipe is 0x%p\n", pipe);
            pDeviceContext->InterruptWritePipe = pipe;
        }
    }

    if (!pDeviceContext->InterruptReadPipe || !pDeviceContext->InterruptWritePipe)
    {
        status = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER,
            "Device is not configured properly %!STATUS!\n",
            status);

        return status;
    }

#pragma endregion

    status = DsUsbConfigContReaderForInterruptEndPoint(Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Exit (%!STATUS!)", status);

    return status;
}

NTSTATUS FireShockEvtDeviceD0Entry(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    PDEVICE_CONTEXT         pDeviceContext;
    NTSTATUS                status;
    BOOLEAN                 isTargetStarted;

    pDeviceContext = DeviceGetContext(Device);
    isTargetStarted = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(PreviousState);

    //
    // Since continuous reader is configured for this interrupt-pipe, we must explicitly start
    // the I/O target to get the framework to post read requests.
    //
    status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptReadPipe));
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER, "Failed to start interrupt read pipe %!STATUS!\n", status);
        goto End;
    }

    status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptWritePipe));
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER, "Failed to start interrupt write pipe %!STATUS!\n", status);
        goto End;
    }

    isTargetStarted = TRUE;

End:

    if (!NT_SUCCESS(status)) {
        //
        // Failure in D0Entry will lead to device being removed. So let us stop the continuous
        // reader in preparation for the ensuing remove.
        //
        if (isTargetStarted) {
            WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptReadPipe), WdfIoTargetCancelSentIo);
            WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptWritePipe), WdfIoTargetCancelSentIo);
        }
    }

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        status = Ds3Init(pDeviceContext);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER,
                "Ds3Init failed with status %!STATUS!",
                status);
        }
        else
        {
            SendReceiveControlRequest(
                pDeviceContext,
                BmRequestClass,
                GetReport,
                Ds3FeatureHostAddress,
                0,
                NULL,
                64,
                NULL);


            //
            // Frequently pushed output report state changes to the DS3
            // 
            WdfTimerStart(Ds3GetContext(Device)->OutputReportTimer, WDF_REL_TIMEOUT_IN_MS(DS3_OUTPUT_REPORT_SEND_DELAY));
        }

        break;
    default:
        break;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Exit");

    return status;
}

NTSTATUS FireShockEvtDeviceD0Exit(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
)
{
    PDEVICE_CONTEXT         pDeviceContext;
    PDS3_DEVICE_CONTEXT     pDs3DeviceContext;

    UNREFERENCED_PARAMETER(TargetState);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Entry");

    pDeviceContext = DeviceGetContext(Device);

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:
        pDs3DeviceContext = Ds3GetContext(Device);
        WdfTimerStop(pDs3DeviceContext->OutputReportTimer, TRUE);
        break;
    }

    WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptReadPipe), WdfIoTargetCancelSentIo);
    WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptWritePipe), WdfIoTargetCancelSentIo);

    WdfIoQueuePurgeSynchronously(pDeviceContext->IoReadQueue);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}