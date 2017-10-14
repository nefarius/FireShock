using System;
using FireShock.Chastity.Server.Properties;

namespace FireShock.Chastity.Server
{
    public delegate void FireShockDeviceDisconnectedEventHandler(object sender, EventArgs e);

    public class FireShockDevice : IDisposable
    {
        public static Guid ClassGuid => Guid.Parse(Settings.Default.ClassGuid);

        public FireShockDevice(string path)
        {
            DevicePath = path;
        }

        public event FireShockDeviceDisconnectedEventHandler DeviceDisconnected;

        public string DevicePath { get; }

        #region IDisposable Support
        private bool disposedValue = false; // To detect redundant calls

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