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
#include <wdf.h>
#include <usb.h>
#include <wdfusb.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FireShockEvtDevicePrepareHardware)
#pragma alloc_text(PAGE, FireShockEvtDeviceD0Entry)
#endif

NTSTATUS FireShockEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    NTSTATUS                        status = STATUS_SUCCESS;
    PDEVICE_CONTEXT                 pDeviceContext = NULL;
    USB_DEVICE_DESCRIPTOR           deviceDescriptor;
    PDS3_DEVICE_CONTEXT             pDs3Context;
    WDF_OBJECT_ATTRIBUTES           attributes;
    WDF_TIMER_CONFIG                outputTimerCfg;

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

    WdfUsbTargetDeviceGetDeviceDescriptor(pDeviceContext->UsbDevice, &deviceDescriptor);

    if (deviceDescriptor.idVendor == 0x054C && deviceDescriptor.idProduct == 0x0268)
    {
        pDeviceContext->DeviceType = DualShock3;

        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DS3_DEVICE_CONTEXT);

        status = WdfObjectAllocateContext(Device, &attributes, &pDs3Context);
        if (!NT_SUCCESS(status))
        {
            KdPrint(("WdfObjectAllocateContext failed status 0x%x\n", status));
            return status;
        }

        UCHAR DefaultOutputReport[DS3_HID_OUTPUT_REPORT_SIZE] =
        {
            0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF,
            0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00,
            0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        RtlCopyBytes(pDs3Context->OutputReportBuffer, DefaultOutputReport, DS3_HID_OUTPUT_REPORT_SIZE);

        WDF_TIMER_CONFIG_INIT_PERIODIC(&outputTimerCfg, Ds3OutputEvtTimerFunc, 10);
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

        attributes.ParentObject = Device;

        status = WdfTimerCreate(&outputTimerCfg, &attributes, &pDs3Context->OutputReportTimer);
        if (!NT_SUCCESS(status)) {
            KdPrint(("FireShock: Error creating output report timer 0x%x\n", status));
            return status;
        }
    }

    if (deviceDescriptor.idVendor == 0x054C && deviceDescriptor.idProduct == 0x05C4)
    {
        pDeviceContext->DeviceType = DualShock4;
    }

    return status;
}

NTSTATUS FireShockEvtDeviceD0Entry(
    _In_ WDFDEVICE              Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    PDEVICE_CONTEXT                         pDeviceContext;
    PDS3_DEVICE_CONTEXT                     pDs3Context;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS     configParams;
    NTSTATUS                                status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(PreviousState);

    KdPrint(("FireShockEvtDeviceD0Entry called\n"));

    pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);
    pDs3Context = Ds3GetContext(Device);

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

        status = WdfUsbTargetDeviceSelectConfig(
            pDeviceContext->UsbDevice,
            WDF_NO_OBJECT_ATTRIBUTES,
            &configParams
        );

        if (!NT_SUCCESS(status))
        {
            KdPrint(("WdfUsbTargetDeviceSelectConfig failed with status 0x%X\n", status));
        }

        pDs3Context->DefaultInterface = WdfUsbTargetDeviceGetInterface(pDeviceContext->UsbDevice, 0);
        pDs3Context->InterruptReadPipe = WdfUsbInterfaceGetConfiguredPipe(pDs3Context->DefaultInterface, 1, NULL);

        break;
    default:
        break;
    }

    return status;
}

