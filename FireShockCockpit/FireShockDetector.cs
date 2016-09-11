using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Windows.Interop;

namespace FireShockCockpit
{
    public class FireShockDetector : Win32Native, IDisposable
    {
        public delegate void DeviceAttachedEventHandler(object sender, FireShockDetectorEventArgs args);

        public delegate void DeviceRemovedEventHandler(object sender, FireShockDetectorEventArgs args);

        [Flags]
        public enum DEVICE_NOTIFY : uint
        {
            DEVICE_NOTIFY_WINDOW_HANDLE = 0x00000000,
            DEVICE_NOTIFY_SERVICE_HANDLE = 0x00000001,
            DEVICE_NOTIFY_ALL_INTERFACE_CLASSES = 0x00000004
        }

        public const int WM_DEVICECHANGE = 0x0219; // device state change
        public const int DBT_DEVICEARRIVAL = 0x8000; // detected a new device
        public const int DBT_DEVICEQUERYREMOVE = 0x8001; // preparing to remove
        public const int DBT_DEVICEREMOVECOMPLETE = 0x8004; // removed 
        public const int DBT_DEVNODES_CHANGED = 0x0007; //A device has been added to or removed from the system.

        private static readonly Guid MediaClassId = Guid.Parse("2409EA50-9ECA-410E-AC9E-F9AC798C4D9C");

        public const int DBT_DEVTYP_DEVICEINTERFACE = 0x00000005;
        public const int DBT_DEVTYP_HANDLE = 0x00000006;
        public const int DBT_DEVTYP_OEM = 0x00000000;
        public const int DBT_DEVTYP_PORT = 0x00000003;
        public const int DBT_DEVTYP_VOLUME = 0x00000002;

        private readonly HwndSource _hwndSource;

        // Track whether Dispose has been called.
        private bool _disposed;

        /// <summary>
        ///     Constructor
        /// </summary>
        public FireShockDetector()
        {
            //Create a fake window
            try
            {
                _hwndSource = new HwndSource(0, 0, 0, 0, 0, "fake", IntPtr.Zero);
            }
            catch
            {
            }
        }

        // Implement IDisposable.
        // Do not make this method virtual.
        // A derived class should not be able to override this method.
        public void Dispose()
        {
            Dispose(true);
            // This object will be cleaned up by the Dispose method.
            // Therefore, you should call GC.SupressFinalize to
            // take this object off the finalization queue 
            // and prevent finalization code for this object
            // from executing a second time.
            GC.SuppressFinalize(this);
        }

        public event DeviceAttachedEventHandler DeviceAttached;
        public event DeviceRemovedEventHandler DeviceRemoved;

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr RegisterDeviceNotification(IntPtr intPtr, IntPtr notificationFilter, uint flags);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool UnregisterDeviceNotification(IntPtr Handle);


        /// <summary>
        ///     Register Device Notification
        /// </summary>
        public void Register()
        {
            if (_hwndSource == null) return;

            //Attaching to window procedure
            _hwndSource.AddHook(HwndSourceHook);

            var notificationFilter = new DEV_BROADCAST_DEVICEINTERFACE();
            var size = Marshal.SizeOf(notificationFilter);
            notificationFilter.dbcc_size = size;
            notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            notificationFilter.dbcc_reserved = 0;
            notificationFilter.dbcc_classguid = MediaClassId.ToByteArray();
            var buffer = IntPtr.Zero;
            buffer = Marshal.AllocHGlobal(size);
            Marshal.StructureToPtr(notificationFilter, buffer, true);
            var result = RegisterDeviceNotification(_hwndSource.Handle, buffer,
                (int)DEVICE_NOTIFY.DEVICE_NOTIFY_WINDOW_HANDLE);
        }


        /// <summary>
        ///     Unregister Device Notification
        /// </summary>
        public void UnRegister()
        {
            if (_hwndSource == null) return;

            UnregisterDeviceNotification(_hwndSource.Handle);
            _hwndSource.RemoveHook(HwndSourceHook);
        }

        /// <summary>
        ///     Overrided Window procedure
        /// </summary>
        /// <param name="hwnd"></param>
        /// <param name="msg"></param>
        /// <param name="wParam"></param>
        /// <param name="lParam"></param>
        /// <param name="handled"></param>
        /// <returns></returns>
        private IntPtr HwndSourceHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (msg != WM_DEVICECHANGE) return IntPtr.Zero;

            switch ((int)wParam)
            {
                case DBT_DEVICEARRIVAL:
                    {
                        var info =
                            (DEV_BROADCAST_DEVICEINTERFACE)
                                Marshal.PtrToStructure(lParam, typeof(DEV_BROADCAST_DEVICEINTERFACE));

                        DeviceAttached?.Invoke(this,
                            new FireShockDetectorEventArgs(new Guid(info.dbcc_classguid), new string(info.dbcc_name)));
                    }
                    break;
                case DBT_DEVICEREMOVECOMPLETE:
                    {
                        var info =
                            (DEV_BROADCAST_DEVICEINTERFACE)
                                Marshal.PtrToStructure(lParam, typeof(DEV_BROADCAST_DEVICEINTERFACE));

                        DeviceRemoved?.Invoke(this,
                            new FireShockDetectorEventArgs(new Guid(info.dbcc_classguid), new string(info.dbcc_name)));
                    }
                    break;
                case DBT_DEVNODES_CHANGED:
                    break;
            }

            return IntPtr.Zero;
        }

        // Dispose(bool disposing) executes in two distinct scenarios.
        // If disposing equals true, the method has been called directly
        // or indirectly by a user's code. Managed and unmanaged resources
        // can be disposed.
        // If disposing equals false, the method has been called by the 
        // runtime from inside the finalizer and you should not reference 
        // other objects. Only unmanaged resources can be disposed.
        private void Dispose(bool disposing)
        {
            // Check to see if Dispose has already been called.
            if (!_disposed)
            {
                // If disposing equals true, dispose all managed 
                // and unmanaged resources.
                if (disposing)
                {
                    // Dispose managed resources.
                    _hwndSource.Dispose();
                }
            }
            _disposed = true;
        }

        // Use C# destructor syntax for finalization code.
        // This destructor will run only if the Dispose method 
        // does not get called.
        // It gives your base class the opportunity to finalize.
        // Do not provide destructors in types derived from this class.
        ~FireShockDetector()
        {
            // Do not re-create Dispose clean-up code here.
            // Calling Dispose(false) is optimal in terms of
            // readability and maintainability.
            Dispose(false);
        }

        /// <summary>
        ///     Helper method to return the device path given a DeviceInterfaceData structure and an InfoSet handle.
        ///     Used in 'FindDevice' so check that method out to see how to get an InfoSet handle and a DeviceInterfaceData.
        /// </summary>
        /// <param name="hInfoSet">Handle to the InfoSet</param>
        /// <param name="oInterface">DeviceInterfaceData structure</param>
        /// <returns>The device path or null if there was some problem</returns>
        private static string GetDevicePath(IntPtr hInfoSet, ref DeviceInterfaceData oInterface)
        {
            uint nRequiredSize = 0;
            // Get the device interface details
            if (
                !SetupDiGetDeviceInterfaceDetail(hInfoSet, ref oInterface, IntPtr.Zero, 0, ref nRequiredSize,
                    IntPtr.Zero))
            {
                var detailDataBuffer = Marshal.AllocHGlobal((int)nRequiredSize);
                Marshal.WriteInt32(detailDataBuffer, IntPtr.Size == 4 ? 4 + Marshal.SystemDefaultCharSize : 8);

                try
                {
                    if (SetupDiGetDeviceInterfaceDetail(hInfoSet, ref oInterface, detailDataBuffer, nRequiredSize,
                        ref nRequiredSize, IntPtr.Zero))
                    {
                        var pDevicePathName = new IntPtr(detailDataBuffer.ToInt64() + 4);
                        return Marshal.PtrToStringAnsi(pDevicePathName) ?? string.Empty;
                    }
                }
                finally
                {
                    Marshal.FreeHGlobal(detailDataBuffer);
                }
            }

            return string.Empty;
        }

        public static IList<string> EnumerateFireShockDevices()
        {
            var list = new List<string>();
            var gHid = MediaClassId; // next, get the GUID from Windows that it uses to represent the HID USB interface
            var hInfoSet = SetupDiGetClassDevs(ref gHid, null, IntPtr.Zero, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
            // this gets a list of all HID devices currently connected to the computer (InfoSet)

            try
            {
                var oInterface = new DeviceInterfaceData(); // build up a device interface data block
                oInterface.Size = Marshal.SizeOf(oInterface);
                // Now iterate through the InfoSet memory block assigned within Windows in the call to SetupDiGetClassDevs
                // to get device details for each device connected
                var nIndex = 0;
                while (SetupDiEnumDeviceInterfaces(hInfoSet, 0, ref gHid, (uint)nIndex, ref oInterface))
                // this gets the device interface information for a device at index 'nIndex' in the memory block
                {
                    list.Add(GetDevicePath(hInfoSet, ref oInterface));
                }
            }
            catch (Exception)
            {
                return list;
            }
            finally
            {
                // Before we go, we have to free up the InfoSet memory reserved by SetupDiGetClassDevs
                SetupDiDestroyDeviceInfoList(hInfoSet);
            }
            return list; // oops, didn't find our device
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct DEV_BROADCAST_DEVICEINTERFACE
        {
            public int dbcc_size;
            public int dbcc_devicetype;
            public int dbcc_reserved;
            //public IntPtr dbcc_handle;
            //public IntPtr dbcc_hdevnotify;
            [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.U1, SizeConst = 16)]
            public byte[]
                dbcc_classguid;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
            public char[] dbcc_name;
            //public byte dbcc_data;
            //public byte dbcc_data1; 
        }
    }
}