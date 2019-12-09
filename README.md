![Icon](https://raw.githubusercontent.com/nefarius/FireShock/master/Installer/FireShock.png)

# FireShock

Windows USB Driver for Sony DualShock Controllers

![Disclaimer](https://forums.vigem.org/assets/uploads/files/alpha_disclaimer.png)

## Summary

`FireShock` consists of a custom USB user-mode driver and a user-mode dispatch service handling wired communication with Sony DualShock 3 and 4 controllers. It allows 3rd party developers to handle controller inputs and outputs via a simple plug-in system.

## How it works

Once installed the `fireshock.dll` user-mode driver will be loaded on any compatible DualShock 3 or 4 controller connected to the system via USB. It replaces the default `HIDUSB.SYS` driver with `WinUSB.sys`.

If a DualShock 3 gets connected to the USB hub, the filter will send a "magic" start packet to the _control endpoint_ so the controller will continuously start sending HID input reports via the _interrupt in endpoint_ on interface 0. If an _interrupt in_ transfer arrives, the contents of the transfer buffer (the HID report) get streamed to any user-mode application calling [`ReadFile(...)`](https://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx) on the device. If a packet war written to the device via [`WriteFile(...)`](https://msdn.microsoft.com/en-us/library/windows/desktop/aa365747(v=vs.85).aspx), the request gets converted into an output report and redirected to the _control endpoint_.

## Supported systems

The driver is built for and tested with Windows 8.1 up to Windows 10 (x86 and amd64).

## Download

### Latest CI builds (unsigned)

### x86

- [FireShock.inf](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x86/FireShock/FireShock.inf?job=Platform%3A%20Win32)
- [FireShock.dll](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x86/FireShock/FireShock.dll?job=Platform%3A%20Win32)

### x64

- [FireShock.inf](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x64/FireShock/FireShock.inf?job=Platform%3A%20x64)
- [FireShock.dll](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x64/FireShock/FireShock.dll?job=Platform%3A%20x64)

## Sources

- http://eleccelerator.com/wiki/index.php?title=DualShock_3
- https://github.com/felis/USB_Host_Shield_2.0/wiki/PS3-Information
- https://www.circuitsathome.com/mcu/ps3-and-wiimote-game-controllers-on-the-arduino-host-shield-part-2
- https://github.com/ribbotson/USB-Host/tree/master/ps3/PS3USB
- https://github.com/Microsoft/Windows-driver-samples/tree/master/hid/firefly/driver
- https://github.com/Microsoft/Windows-driver-samples/tree/master/general/toaster/toastDrv/kmdf/filter/sideband
- https://msdn.microsoft.com/en-us/library/windows/hardware/dn265671(v=vs.85).aspx
- http://eleccelerator.com/usbdescreqparser/
- http://www.psdevwiki.com/ps4/DS4-USB
- https://patchwork.kernel.org/patch/9367441/
