/*
* MIT License
*
* Copyright (c) 2017-2019 Nefarius Software Solutions e.U.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


EXTERN_C_START

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
    //
    // USB Device object
    // 
    WDFUSBDEVICE UsbDevice;

    WDFUSBINTERFACE UsbInterface;

    WDFUSBPIPE InterruptReadPipe;

    WDFUSBPIPE InterruptWritePipe;
    
    //
    // Device type
    // 
    DS_DEVICE_TYPE DeviceType;

    //
    // Device instance index
    // 
    ULONG DeviceIndex;

    WDFQUEUE IoReadQueue;

    BD_ADDR HostAddress;

    BD_ADDR DeviceAddress;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// DualShock 4-specific context
// 
typedef struct _DS4_DEVICE_CONTEXT
{
    //
    // Timer for re-occurring output reports
    // 
    WDFTIMER OutputReportTimer;

    //
    // Raw output report buffer
    // 
    UCHAR OutputReportBuffer[DS4_HID_OUTPUT_REPORT_SIZE];

    //
    // Cached input report
    // 
    //DS4_REPORT LastReport;

} DS4_DEVICE_CONTEXT, *PDS4_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DS4_DEVICE_CONTEXT, Ds4GetContext)


//
// Function to initialize the device's queues and callbacks
//
NTSTATUS
FireShockCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

NTSTATUS Ds3Init(PDEVICE_CONTEXT Context);

EXTERN_C_END
