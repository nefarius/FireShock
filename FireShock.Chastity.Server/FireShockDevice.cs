using System;
using System.Net.NetworkInformation;
using System.Reactive.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using FireShock.Chastity.Server.Exceptions;
using FireShock.Chastity.Server.Properties;
using Nefarius.Sub.Kinbaku.Core.Hub.Client;
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

    public abstract partial class FireShockDevice : IDisposable, IDualShockDevice
    {
        private readonly CancellationTokenSource _inputCancellationTokenSourcePrimary = new CancellationTokenSource();
        private readonly CancellationTokenSource _inputCancellationTokenSourceSecondary = new CancellationTokenSource();
        private readonly IObservable<long> _outputReportSchedule = Observable.Interval(TimeSpan.FromMilliseconds(10));
        private readonly IDisposable _outputReportTask;

        internal FireShockDevice(string path, Kernel32.SafeObjectHandle handle)
        {
            DevicePath = path;
            DeviceHandle = handle;

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
                    throw new FireShockGetDeviceBdAddrFailedException($"Failed to request address of device {path}");

                var resp = Marshal.PtrToStructure<FireshockGetDeviceBdAddr>(pData);

                ClientAddress = new PhysicalAddress(resp.Device.Address);
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
                    throw new FireShockGetHostBdAddrFailedException($"Failed to request host address for device {ClientAddress}");

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

            _outputReportTask = _outputReportSchedule.Subscribe(OnOutputReport);

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

        protected virtual byte[] HidOutputReport { get; }

        public PhysicalAddress HostAddress { get; }

        public DualShockDeviceType DeviceType { get; private set; }

        public PhysicalAddress ClientAddress { get; }

        /// <summary>
        ///     Send Rumble request to the controller.
        /// </summary>
        /// <param name="largeMotor">Large motor intensity (0 = off, 255 = max).</param>
        /// <param name="smallMotor">Small motor intensity (0 = off, >0 = on).</param>
        public virtual void Rumble(byte largeMotor, byte smallMotor)
        {
            throw new NotImplementedException();
        }

        public virtual void PairTo(PhysicalAddress host)
        {
            throw new NotImplementedException();
        }

        /// <summary>
        ///     Factors a FireShock wrapper depending on the device type.
        /// </summary>
        /// <param name="path">Path of the device to open.</param>
        /// <returns>A <see cref="FireShockDevice" /> implementation.</returns>
        public static FireShockDevice CreateDevice(string path)
        {
            //
            // Open device
            // 
            var deviceHandle = Kernel32.CreateFile(path,
                Kernel32.ACCESS_MASK.GenericRight.GENERIC_READ | Kernel32.ACCESS_MASK.GenericRight.GENERIC_WRITE,
                Kernel32.FileShare.FILE_SHARE_READ | Kernel32.FileShare.FILE_SHARE_WRITE,
                IntPtr.Zero, Kernel32.CreationDisposition.OPEN_EXISTING,
                Kernel32.CreateFileFlags.FILE_ATTRIBUTE_NORMAL | Kernel32.CreateFileFlags.FILE_FLAG_OVERLAPPED,
                Kernel32.SafeObjectHandle.Null
            );

            if (deviceHandle.IsInvalid)
                throw new ArgumentException($"Couldn't open device {path}");

            var length = Marshal.SizeOf(typeof(FireshockGetDeviceType));
            var pData = Marshal.AllocHGlobal(length);

            try
            {
                var bytesReturned = 0;
                var ret = deviceHandle.OverlappedDeviceIoControl(
                    IoctlFireshockGetDeviceType,
                    IntPtr.Zero, 0, pData, length,
                    out bytesReturned);

                if (!ret)
                    throw new FireShockGetDeviceTypeFailedException($"Failed to request type of device {path}");

                var resp = Marshal.PtrToStructure<FireshockGetDeviceType>(pData);

                switch (resp.DeviceType)
                {
                    case DualShockDeviceType.DualShock3:
                        return new FireShock3Device(path, deviceHandle);
                    default:
                        throw new NotImplementedException();
                }
            }
            finally
            {
                Marshal.FreeHGlobal(pData);
            }

            return null;
        }

        /// <summary>
        ///     Periodically submits output report state changes of this controller.
        /// </summary>
        /// <param name="l">The interval.</param>
        private void OnOutputReport(long l)
        {
            // TODO: optimize periodical memory allocation
            var buffer = Marshal.AllocHGlobal(HidOutputReport.Length);
            Marshal.Copy(HidOutputReport, 0, buffer, HidOutputReport.Length);

            try
            {
                int bytesReturned;
                var ret = DeviceHandle.OverlappedWriteFile(
                    buffer,
                    Ds3HidOutputReportSize,
                    out bytesReturned);

                if (!ret)
                    throw new FireShockWriteOutputReportFailedException("Sending output report failed");
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

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
        public event RumbleRequestReceivedEventHandler RumbleRequestReceived;

        public override string ToString()
        {
            return $"{DeviceType} ({ClientAddress})";
        }

        private class FireShock3Device : FireShockDevice
        {
            private readonly Lazy<byte[]> _hidOutputReportLazy = new Lazy<byte[]>(() => new byte[]
            {
                0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0xFF, 0x27, 0x10, 0x00, 0x32, 0xFF,
                0x27, 0x10, 0x00, 0x32, 0xFF, 0x27, 0x10, 0x00,
                0x32, 0xFF, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            });

            public FireShock3Device(string path, Kernel32.SafeObjectHandle handle) : base(path, handle)
            {
                DeviceType = DualShockDeviceType.DualShock3;
            }

            protected override byte[] HidOutputReport => _hidOutputReportLazy.Value;

            /// <inheritdoc />
            /// <summary>
            ///     Send Rumble request to the controller.
            /// </summary>
            /// <param name="largeMotor">Large motor intensity (0 = off, 255 = max).</param>
            /// <param name="smallMotor">Small motor intensity (0 = off, >0 = on).</param>
            public override void Rumble(byte largeMotor, byte smallMotor)
            {
                HidOutputReport[2] = (byte)(smallMotor > 0 ? 0x01 : 0x00);
                HidOutputReport[4] = largeMotor;

                OnOutputReport(0);
            }

            public override void PairTo(PhysicalAddress host)
            {
                var length = Marshal.SizeOf(typeof(FireshockSetHostBdAddr));
                var pData = Marshal.AllocHGlobal(length);
                Marshal.Copy(host.GetAddressBytes(), 0, pData, length);

                try
                {
                    var bytesReturned = 0;
                    var ret = DeviceHandle.OverlappedDeviceIoControl(
                        IoctlFireshockSetHostBdAddr,
                        pData, length, IntPtr.Zero, 0,
                        out bytesReturned);

                    if (!ret)
                        throw new FireShockSetHostBdAddrFailedException($"Failed to pair {ClientAddress} to {host}");
                }
                finally
                {
                    Marshal.FreeHGlobal(pData);
                }
            }
        }

        #region Equals Support

        public override bool Equals(object obj)
        {
            if (ReferenceEquals(null, obj)) return false;
            if (ReferenceEquals(this, obj)) return true;
            var other = obj as FireShockDevice;
            return other != null && Equals(other);
        }

        private bool Equals(FireShockDevice other)
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
                    _outputReportTask.Dispose();

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