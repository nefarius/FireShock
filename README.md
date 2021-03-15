<img src="assets/FireShock.png" align="right" />

# FireShock

Windows USB Driver for Sony DualShock Controllers

[![Build status](https://ci.appveyor.com/api/projects/status/qtn7klq26ho8atg6/branch/master?svg=true)](https://ci.appveyor.com/project/nefarius/fireshock/branch/master) [![GitHub All Releases](https://img.shields.io/github/downloads/ViGEm/FireShock/total)](https://somsubhra.com/github-release-stats/?username=ViGEm&repository=FireShock)

---

⚠️ **This project is no longer maintained. It has been superseded by [DsHidMini](https://github.com/ViGEm/DsHidMini).** ⚠️

---

## Summary

`FireShock` consists of a custom USB user-mode driver and a [user-mode dispatch service](https://github.com/ViGEm/Shibari) handling wired communication with Sony DualShock **3** Controllers. It allows 3rd party developers to handle controller inputs and outputs via a simple plug-in system.

## How it works

Once installed the `fireshock.dll` user-mode driver will be loaded on any compatible DualShock 3 Controller connected to the system via USB. It replaces the default `HIDUSB.SYS` driver with `WinUSB.sys`.

If a DualShock 3 gets connected to the USB hub, the filter will send a "magic" start packet to the _control endpoint_ so the controller will continuously start sending HID input reports via the _interrupt in endpoint_ on interface 0. If an _interrupt in_ transfer arrives, the contents of the transfer buffer (the HID report) get streamed to any user-mode application calling [`ReadFile(...)`](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile) on the device. If a packet war written to the device via [`WriteFile(...)`](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile), the request gets converted into an output report and redirected to the _control endpoint_.

## How to use

**Important:** this is *not* an HID/XInput compatible driver, you **need** [the Shibari companion application](https://github.com/ViGEm/Shibari#documentation) and follow its setup instructions to get the controller recognized by games!

## Supported systems

The driver is built for and tested with Windows 8.1 up to Windows 10 (x86 and amd64).

## Download

### Latest stable builds (signed)

- [GitHub](../../releases/latest)
- [Mirror](https://downloads.vigem.org/projects/FireShock/stable/)

### Latest CI builds (unsigned)

### x86

- [FireShock.inf](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x86/FireShock/FireShock.inf?job=Platform%3A%20Win32)
- [FireShock.cat](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x86/FireShock/fireshock.cat?job=Platform%3A%20Win32)
- [FireShock.dll](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x86/FireShock/FireShock.dll?job=Platform%3A%20Win32)

### x64

- [FireShock.inf](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x64/FireShock/FireShock.inf?job=Platform%3A%20x64)
- [FireShock.cat](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x64/FireShock/fireshock.cat?job=Platform%3A%20x64)
- [FireShock.dll](https://ci.appveyor.com/api/projects/nefarius/fireshock/artifacts/bin/x64/FireShock/FireShock.dll?job=Platform%3A%20x64)

## Sources

- [Eleccelerator Wiki](http://eleccelerator.com/wiki/index.php?title=DualShock_3)
- [felis/USB_Host_Shield_2.0 - PS3 Information](https://github.com/felis/USB_Host_Shield_2.0/wiki/PS3-Information)
- [PS3 and Wiimote Game Controllers on the Arduino Host Shield: Part 2](https://web.archive.org/web/20160326093555/https://www.circuitsathome.com/mcu/ps3-and-wiimote-game-controllers-on-the-arduino-host-shield-part-2)
- [ribbotson/USB-Host](https://github.com/ribbotson/USB-Host/tree/master/ps3/PS3USB)
- [Windows-driver-samples/hid/firefly/driver](https://github.com/Microsoft/Windows-driver-samples/tree/master/hid/firefly/driver)
- [Windows-driver-samples/general/toaster/toastDrv/kmdf/filter/sideband](https://github.com/Microsoft/Windows-driver-samples/tree/master/general/toaster/toastDrv/kmdf/filter/sideband)
- [wdfusb.h header](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfusb/index)
- [USB Descriptor and Request Parser](http://eleccelerator.com/usbdescreqparser/)
- [PS4 Developer wiki - DS4-USB](http://www.psdevwiki.com/ps4/DS4-USB)
- [HID: sony: Update device ids](https://patchwork.kernel.org/patch/9367441/)
