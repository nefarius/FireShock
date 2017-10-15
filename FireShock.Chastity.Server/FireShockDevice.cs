using System;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using FireShock.Chastity.Server.Properties;
using Nefarius.Sub.Kinbaku.Core.Plugins;
using Nefarius.Sub.Kinbaku.Core.Reports.Common;
using Nefarius.Sub.Kinbaku.Core.Reports.DualShock3;
using Nefarius.Sub.Kinbaku.Util;
using PInvoke;
using Serilog;

namespace FireShock.Chastity.Server
{
    public delegate void FireShockDeviceDisconnectedEventHandler(object sender, EventArgs e);
    public delegate void FireShockInputReportReceivedEventHandler(object sender, InputReportEventArgs e);

    public partial class FireShockDevice : IDisposable, IDualShockDevice
    {
        private readonly CancellationTokenSource _inputCancellationTokenSourcePrimary = new CancellationTokenSource();
        private readonly CancellationTokenSource _inputCancellationTokenSourceSecondary = new CancellationTokenSource();
        private const int ErrorOperationAborted = 0x3E3;

        public FireShockDevice(string path)
        {
            DevicePath = path;
            DeviceType = DualShockDeviceType.DualShock3;
            ClientAddress = PhysicalAddress.None;

            //
            // Open device
            // 
            DeviceHandle = Kernel32.CreateFile(DevicePath,
                Kernel32.ACCESS_MASK.GenericRight.GENERIC_READ | Kernel32.ACCESS_MASK.GenericRight.GENERIC_WRITE,
                Kernel32.FileShare.FILE_SHARE_READ | Kernel32.FileShare.FILE_SHARE_WRITE,
                IntPtr.Zero, Kernel32.CreationDisposition.OPEN_EXISTING,
                Kernel32.CreateFileFlags.FILE_ATTRIBUTE_NORMAL | Kernel32.CreateFileFlags.FILE_FLAG_OVERLAPPED,
                Kernel32.SafeObjectHandle.Null
            );

            if (DeviceHandle.IsInvalid)
                throw new ArgumentException($"Couldn't open device {DevicePath}");

            //
            // Start two tasks requesting input reports in parallel.
            // 
            // While on threads request gets completed, another request can be
            // queued by the other thread. This way no input can get lost because
            // there's always at least one pending request in the driver to get
            // completed. Each thread uses inverted calls for maximum performance.
            // 
            Task.Factory.StartNew(RequestInputReportWorker, _inputCancellationTokenSourcePrimary.Token);
            Task.Factory.StartNew(RequestInputReportWorker, _inputCancellationTokenSourceSecondary.Token);
        }

        public static Guid ClassGuid => Guid.Parse(Settings.Default.ClassGuid);

        public string DevicePath { get; }

        public Kernel32.SafeObjectHandle DeviceHandle { get; }

        public DualShockDeviceType DeviceType { get; }

        public PhysicalAddress ClientAddress { get; }

        private void RequestInputReportWorker(object cancellationToken)
        {
            var token = (CancellationToken)cancellationToken;
            var buffer = new byte[512];
            var unmanagedBuffer = Marshal.AllocHGlobal(buffer.Length);

            try
            {
                while (!token.IsCancellationRequested)
                {
                    int bytesReturned;

                    var ret = DeviceHandle.OverlappedReadFile(
                        unmanagedBuffer,
                        buffer.Length,
                        out bytesReturned);

                    if (ret)
                    {
                        Marshal.Copy(unmanagedBuffer, buffer, 0, bytesReturned);

                        OnInputReport(new DualShock3InputReport(buffer));
                        continue;
                    }

                    if (Marshal.GetLastWin32Error() == ErrorOperationAborted)
                    {
                        OnDisconnected();
                        return;
                    }

                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
            }
            finally
            {
                Marshal.FreeHGlobal(unmanagedBuffer);
            }
        }

        private void OnDisconnected()
        {
            _inputCancellationTokenSourcePrimary.Cancel();
            _inputCancellationTokenSourceSecondary.Cancel();

            if (!Monitor.TryEnter(this)) return;

            try
            {
                DeviceDisconnected?.Invoke(this, EventArgs.Empty);
            }
            finally
            {
                Monitor.Exit(this);
            }
        }

        private void OnInputReport(IInputReport report)
        {
            InputReportReceived?.Invoke(this, new InputReportEventArgs(report));
        }

        public event FireShockDeviceDisconnectedEventHandler DeviceDisconnected;

        public event FireShockInputReportReceivedEventHandler InputReportReceived;

        public override bool Equals(object obj)
        {
            return base.Equals(obj);
        }

        protected bool Equals(FireShockDevice other)
        {
            return ClientAddress.Equals(other.ClientAddress);
        }

        public override int GetHashCode()
        {
            return ClientAddress.GetHashCode();
        }

        public static bool operator ==(FireShockDevice left, FireShockDevice right)
        {
            return Equals(left, right);
        }

        public static bool operator !=(FireShockDevice left, FireShockDevice right)
        {
            return !Equals(left, right);
        }

        #region IDisposable Support

        private bool disposedValue; // To detect redundant calls

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    _inputCancellationTokenSourcePrimary.Cancel();
                    _inputCancellationTokenSourceSecondary.Cancel();
                }

                DeviceHandle.Dispose();

                disposedValue = true;
            }
        }

        ~FireShockDevice()
        {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            Dispose(false);
        }

        // This code added to correctly implement the disposable pattern.
        public void Dispose()
        {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        #endregion
    }
}
