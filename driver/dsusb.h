#pragma once
#include <usb.h>

NTSTATUS GetConfigurationDescriptorType(PURB urb, PDEVICE_CONTEXT pCommon);
NTSTATUS GetDescriptorFromInterface(PURB urb, PDEVICE_CONTEXT pCommon);

