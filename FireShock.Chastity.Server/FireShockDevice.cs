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

        public FireShockDevice(string path)
        {
            DevicePath = path;

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

            var length = Marshal.SizeOf(typeof(FireshockGetDeviceBdAddr));
            var pData = Marshal.AllocHGlobal(length);

            try
            {
                var bytesReturned = 0;
                var ret = DeviceHandle.OverlappedDeviceIoControl(
                    IoctlFireshockGetDeviceBdAddr,
                    IntPtr.Zero, 0, pData, length,
                    out bytesReturned);

                if (!ret)
                    throw new Exception("Failed to request device address");

                var resp = Marshal.PtrToStructure<FireshockGetDeviceBdAddr>(pData);

                ClientAddress = new PhysicalAddress(resp.Device.Address);
            }
            finally
            {
                Marshal.FreeHGlobal(pData);
            }

            length = Marshal.SizeOf(typeof(FireshockGetDeviceType));
            pData = Marshal.AllocHGlobal(length);

            try
            {
                var bytesReturned = 0;
                var ret = DeviceHandle.OverlappedDeviceIoControl(
                    IoctlFireshockGetDeviceType,
                    IntPtr.Zero, 0, pData, length,
                    out bytesReturned);

                if (!ret)
                    throw new Exception("Failed to request device type");

                var resp = Marshal.PtrToStructure<FireshockGetDeviceType>(pData);

                DeviceType = resp.DeviceType;
            }
            finally
            {
                Marshal.FreeHGlobal(pData);
            }

            length = Marshal.SizeOf(typeof(FireshockGetHostBdAddr));
            pData = Marshal.AllocHGlobal(length);

            try
            {
                var bytesReturned = 0;
                var ret = DeviceHandle.OverlappedDeviceIoControl(
                    IoctlFireshockGetHostBdAddr,
                    IntPtr.Zero, 0, pData, length,
                    out bytesReturned);

                if (!ret)
                    throw new Exception("Failed to request host address");

                var resp = Marshal.PtrToStructure<FireshockGetHostBdAddr>(pData);

                HostAddress = new PhysicalAddress(resp.Host.Address);
            }
            finally
            {
                Marshal.FreeHGlobal(pData);
            }

            Log.Information($"Device is {DeviceType} " +
                            $"with address {ClientAddress.AsFriendlyName()} " +
                            $"currently paired to {HostAddress.AsFriendlyName()}");

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

        public PhysicalAddress HostAddress { get; }

        public DualShockDeviceType DeviceType { get; }

        public PhysicalAddress ClientAddress { get; }

        private void RequestInputReportWorker(object cancellationToken)
        {
            var token = (CancellationToken) cancellationToken;
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

        public override string ToString()
        {
            return $"{DeviceType} ({ClientAddress})";
        }

        public void Rumble(byte largeMotor, byte smallMotor)
        {
            throw new NotImplementedException();
        }

        public void PairTo(PhysicalAddress host)
        {
            throw new NotImplementedException();
        }

        #region Equals Support

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

        #endregion

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