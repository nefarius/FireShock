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
#include <usb.h>
#include <wdfusb.h>


NTSTATUS SendControlRequest(
    WDFDEVICE Device,
    BYTE Request,
    USHORT Value,
    USHORT Index,
    PVOID Buffer,
    size_t BufferLength)
{
    NTSTATUS                        status;
    WDFMEMORY                       transferBuffer;
    PDEVICE_CONTEXT                 pDeviceContext;
    WDFREQUEST                      controlRequest;
    WDF_USB_CONTROL_SETUP_PACKET    packet;
    WDF_OBJECT_ATTRIBUTES           transferAttribs;

    pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

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
        BufferLength,
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
        Buffer,
        BufferLength);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("WdfMemoryCopyFromBuffer failed with status 0x%X\n", status));
        return status;
    }

    WDF_USB_CONTROL_SETUP_PACKET_INIT_CLASS(
        &packet,
        BmRequestHostToDevice,
        BMREQUEST_TO_INTERFACE,
        Request,
        Value,
        Index);

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
        ControlRequestCompletionRoutine,
        WDF_NO_SEND_OPTIONS);

    if (!WdfRequestSend(
        controlRequest,
        WdfUsbTargetDeviceGetIoTarget(pDeviceContext->UsbDevice),
        NULL))
    {
        KdPrint(("WdfRequestSend failed\n"));
    }

    return status;
}

NTSTATUS Ds3Init(WDFDEVICE hDevice)
{
    UCHAR hidCommandEnable[DS_HID_COMMAND_ENABLE_SIZE] =
    {
        0x42, 0x0C, 0x00, 0x00
    };

    return SendControlRequest(
        hDevice,
        SetReport,
        Ds3StartDevice,
        0,
        hidCommandEnable,
        DS_HID_COMMAND_ENABLE_SIZE);
}

VOID Ds3OutputEvtTimerFunc(
    _In_ WDFTIMER Timer
)
{
    WDFDEVICE           hDevice;
    PDEVICE_CONTEXT     pDeviceContext;
    NTSTATUS            status;

    hDevice = WdfTimerGetParentObject(Timer);
    pDeviceContext = WdfObjectGet_DEVICE_CONTEXT(hDevice);

    switch (pDeviceContext->DeviceType)
    {
    case DualShock3:

        status = SendControlRequest(
            hDevice,
            SetReport,
            USB_SETUP_VALUE(Output, One),
            0,
            Ds3GetContext(hDevice)->OutputReportBuffer,
            DS_HID_OUTPUT_REPORT_SIZE
        );

        if (!NT_SUCCESS(status))
        {
            KdPrint(("SendControlRequest failed with status 0x%X\n", status));
        }

        break;
    }
}

VOID Ds3GetConfigurationDescriptorType(PUCHAR Buffer, ULONG Length)
{
    UCHAR ConfigurationDescriptor[41] =
    {
        0x09,        // bLength
        0x02,        // bDescriptorType (Configuration)
        0x29, 0x00,  // wTotalLength 41
        0x01,        // bNumInterfaces 1
        0x01,        // bConfigurationValue
        0x00,        // iConfiguration (String Index)
        0x80,        // bmAttributes
        0xFA,        // bMaxPower 500mA

        0x09,        // bLength
        0x04,        // bDescriptorType (Interface)
        0x00,        // bInterfaceNumber 0
        0x00,        // bAlternateSetting
        0x02,        // bNumEndpoints 2
        0x03,        // bInterfaceClass
        0x00,        // bInterfaceSubClass
        0x00,        // bInterfaceProtocol
        0x00,        // iInterface (String Index)

        0x09,        // bLength
        0x21,        // bDescriptorType (HID)
        0x11, 0x01,  // bcdHID 1.11
        0x00,        // bCountryCode
        0x01,        // bNumDescriptors
        0x22,        // bDescriptorType[0] (HID)
        DS3_HID_REPORT_DESCRIPTOR_SIZE, 0x00,  // wDescriptorLength[0]

        0x07,        // bLength
        0x05,        // bDescriptorType (Endpoint)
        0x02,        // bEndpointAddress (OUT/H2D)
        0x03,        // bmAttributes (Interrupt)
        0x40, 0x00,  // wMaxPacketSize 64
        0x01,        // bInterval 1 (unit depends on device speed)

        0x07,        // bLength
        0x05,        // bDescriptorType (Endpoint)
        0x81,        // bEndpointAddress (IN/D2H)
        0x03,        // bmAttributes (Interrupt)
        0x40, 0x00,  // wMaxPacketSize 64
        0x01,        // bInterval 1 (unit depends on device speed)

                     // 41 bytes

                     // best guess: USB Standard Descriptor
    };

    RtlCopyBytes(Buffer, ConfigurationDescriptor, Length);
}


