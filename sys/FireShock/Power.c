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
    NTSTATUS status;
    PDEVICE_CONTEXT pDeviceContext;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
    USB_DEVICE_DESCRIPTOR deviceDescriptor;
    PDS3_DEVICE_CONTEXT             pDs3Context;
    PDS4_DEVICE_CONTEXT             pDs4Context;
    WDF_OBJECT_ATTRIBUTES           attributes;
    WDF_TIMER_CONFIG                timerCfg;
    BOOLEAN                         supported = FALSE;

    UNREFERENCED_PARAMETER(pDs4Context);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Entry");

    status = STATUS_SUCCESS;
    pDeviceContext = DeviceGetContext(Device);

    //
    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create the device the first time
    // PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
    //
    if (pDeviceContext->UsbDevice == NULL) {

        status = WdfUsbTargetDeviceCreate(Device,
            WDF_NO_OBJECT_ATTRIBUTES,
            &pDeviceContext->UsbDevice
        );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER,
                "WdfUsbTargetDeviceCreateWithParameters failed 0x%x", status);
            return status;
        }
    }

    // 
    // Use device descriptor to identify the device
    // 
    WdfUsbTargetDeviceGetDeviceDescriptor(pDeviceContext->UsbDevice, &deviceDescriptor);

    // 
    // Device is a DualShock 3 or Move Navigation Controller
    // 
    if (deviceDescriptor.idVendor == DS3_VENDOR_ID
        && (deviceDescriptor.idProduct == DS3_PRODUCT_ID || deviceDescriptor.idProduct == PS_MOVE_NAVI_PRODUCT_ID))
    {
        pDeviceContext->DeviceType = DualShock3;
        supported = TRUE;

        // Add DS3-specific context to device object
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DS3_DEVICE_CONTEXT);

        status = WdfObjectAllocateContext(Device, &attributes, (PVOID)&pDs3Context);
        if (!NT_SUCCESS(status))
        {
            KdPrint((DRIVERNAME "WdfObjectAllocateContext failed status 0x%x\n", status));
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
            KdPrint((DRIVERNAME "Error creating output report timer 0x%x\n", status));
            return status;
        }

        // Initialize input enable timer
        WDF_TIMER_CONFIG_INIT_PERIODIC(&timerCfg, Ds3EnableEvtTimerFunc, DS3_INPUT_ENABLE_SEND_DELAY);
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

        attributes.ParentObject = Device;

        status = WdfTimerCreate(&timerCfg, &attributes, &pDs3Context->InputEnableTimer);
        if (!NT_SUCCESS(status)) {
            KdPrint((DRIVERNAME "Error creating input enable timer 0x%x\n", status));
            return status;
        }

        // We can start the timer here since the callback is called again on failure
        WdfTimerStart(pDs3Context->InputEnableTimer, WDF_REL_TIMEOUT_IN_MS(DS3_INPUT_ENABLE_SEND_DELAY));
    }

    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

    status = WdfUsbTargetDeviceSelectConfig(pDeviceContext->UsbDevice,
        WDF_NO_OBJECT_ATTRIBUTES,
        &configParams
    );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_POWER,
            "WdfUsbTargetDeviceSelectConfig failed 0x%x", status);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_POWER, "%!FUNC! Exit");

    return status;
}

NTSTATUS FireShockEvtDeviceD0Entry(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);

    return STATUS_SUCCESS;
}

NTSTATUS FireShockEvtDeviceD0Exit(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);

    return STATUS_SUCCESS;
}