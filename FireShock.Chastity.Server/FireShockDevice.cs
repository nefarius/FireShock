using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using FireShock.Chastity.Server.Properties;
using Nefarius.Sub.Kinbaku.Util;
using PInvoke;
using Serilog;

namespace FireShock.Chastity.Server
{
    public delegate void FireShockDeviceDisconnectedEventHandler(object sender, EventArgs e);

    public class FireShockDevice : IDisposable
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

                        Log.Information($"{buffer[6]}");
                    }
                }
            }
            finally
            {
                Marshal.FreeHGlobal(unmanagedBuffer);
            }
        }

        public static Guid ClassGuid => Guid.Parse(Settings.Default.ClassGuid);

        public string DevicePath { get; }

        public Kernel32.SafeObjectHandle DeviceHandle { get; }

        public event FireShockDeviceDisconnectedEventHandler DeviceDisconnected;

        #region IDisposable Support

        private bool disposedValue; // To detect redundant calls

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // TODO: dispose managed state (managed objects).
                }

                // TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
                // TODO: set large fields to null.

                disposedValue = true;
            }
        }

        // TODO: override a finalizer only if Dispose(bool disposing) above has code to free unmanaged resources.
        // ~FireShockDevice() {
        //   // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
        //   Dispose(false);
        // }

        // This code added to correctly implement the disposable pattern.
        public void Dispose()
        {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            Dispose(true);
            // TODO: uncomment the following line if the finalizer is overridden above.
            // GC.SuppressFinalize(this);
        }

        #endregion
    }
}