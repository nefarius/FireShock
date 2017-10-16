using System.Runtime.InteropServices;
using Nefarius.Sub.Kinbaku.Core.Plugins;

namespace FireShock.Chastity.Server
{
    public partial class FireShockDevice
    {
        #region I/O control codes

        private const uint IoctlFireshockGetHostBdAddr = 0x80006004;
        private const uint IoctlFireshockGetDeviceBdAddr = 0x80006008;
        private const uint IoctlFireshockSetHostBdAddr = 0x8000A00C;
        private const uint IoctlFireshockGetDeviceType = 0x80006010;

        #endregion

        #region Error constants

        private const int ErrorOperationAborted = 0x3E3;

        #endregion

        #region Misc. Consts

        private const int Ds3HidOutputReportSize = 0x30;

        #endregion

        #region Managed to unmanaged structs

        [StructLayout(LayoutKind.Sequential)]
        internal struct BdAddr
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
            public byte[] Address;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        private struct FireshockGetHostBdAddr
        {
            public BdAddr Host;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        private struct FireshockGetDeviceBdAddr
        {
            public BdAddr Device;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        private struct FireshockSetHostBdAddr
        {
            public BdAddr Host;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        private struct FireshockGetDeviceType
        {
            public DualShockDeviceType DeviceType;
        }

        #endregion
    }
}