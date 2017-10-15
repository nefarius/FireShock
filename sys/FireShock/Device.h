/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    User-mode Driver Framework 2

--*/

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
// DualShock 3-specific context
// 
typedef struct _DS3_DEVICE_CONTEXT
{
    //
    // Timer for re-occurring output reports
    // 
    WDFTIMER OutputReportTimer;

    //
    // Raw output report buffer
    // 
    UCHAR OutputReportBuffer[DS3_HID_OUTPUT_REPORT_SIZE];

    //
    // Translated input state
    // 
    //FS3_GAMEPAD_STATE InputState;

    //
    // Cached input report
    // 
    UCHAR LastReport[DS3_ORIGINAL_HID_REPORT_SIZE];

    //
    // Raw input report
    // 
    UCHAR InputReport[DS3_ORIGINAL_HID_REPORT_SIZE];

} DS3_DEVICE_CONTEXT, *PDS3_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DS3_DEVICE_CONTEXT, Ds3GetContext)

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
