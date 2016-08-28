# FireShock
HIDUSB filter driver for Sony DualShock controllers

[![Twitter Follow](https://img.shields.io/twitter/follow/shields_io.svg?style=social&label=Follow&maxAge=2592000)](https://twitter.com/CNefarius)

## Summary
FireShock allows you to use a wired DualShock controller with any [Raw Input](https://msdn.microsoft.com/en-us/library/windows/desktop/ms645543(v=vs.85).aspx), [DirectInput](https://msdn.microsoft.com/de-de/library/windows/desktop/ee418273(v=vs.85)) or ([with additional drivers](../../../ViGEm)) [XInput](https://msdn.microsoft.com/en-us/library/windows/desktop/ee417001(v=vs.85).aspx) compatible application/game. It's a Windows Filter Driver which sits between `HIDUSB.SYS` and the USB PDO exposed by the USB hub taking care of the necessary modifications to ensure proper HID compatibility. With this driver the DualShock becomes transparently useable by Windows like any other USB gamepad.

## How it works
Once installed the `fireshock.sys` filter driver will be loaded into the driver stack of any compatible DualShock 3 or 4 controller connected to the system via USB. It sits under `HIDUSB.SYS` and monitors/intercepts the URBs sent from `HIDUSB.SYS` to the USB PDO.

If a DualShock 3 gets connected to the USB hub, the filter will inject a modified configuration and HID report descriptor which describes a button and axis layout similar to the DualShock 4. After the USB device was successfully initialized it send a "magic" start packet to the _control endpoint_ so the controller will continously start sending HID input reports via the _interrupt in endpoint_ on interface 0. If an _interrupt in_ transfer was requested, the request gets sent down to the USB PDO, the result intercepted and translated by the filter and completed.


## Sources
 * http://eleccelerator.com/wiki/index.php?title=DualShock_3
 * https://github.com/felis/USB_Host_Shield_2.0/wiki/PS3-Information
 * https://github.com/Microsoft/Windows-driver-samples/tree/master/hid/firefly/driver
 * https://github.com/Microsoft/Windows-driver-samples/tree/master/general/toaster/toastDrv/kmdf/filter/sideband
 * https://msdn.microsoft.com/en-us/library/windows/hardware/dn265671(v=vs.85).aspx
