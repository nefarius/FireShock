using System;
using System.Collections.Generic;
using System.Linq;
using System.Reactive.Linq;
using Serilog;
using Nefarius.Devcon;

namespace FireShock.Chastity.Server
{
    internal class ChastityService
    {
        private readonly IObservable<long> _deviceLookupSchedule = Observable.Interval(TimeSpan.FromSeconds(2));
        private IDisposable _deviceLookupTask;
        private readonly List<FireShockDevice> _devices = new List<FireShockDevice>();

        public void Start()
        {
            Log.Information("FireShock Chastity Server started");

            _deviceLookupTask = _deviceLookupSchedule.Subscribe(OnLookup);
        }

        private void OnLookup(long l)
        {
            var instanceId = 0;
            string path = string.Empty, instance = string.Empty;

            while (Devcon.Find(FireShockDevice.ClassGuid, ref path, ref instance, instanceId++))
            {
                if (_devices.Any(h => h.DevicePath.Equals(path))) continue;

                Log.Information($"Found FireShock device {path} ({instance})");

                var host = new FireShockDevice(path);

                host.DeviceDisconnected += (sender, args) =>
                {
                    var device = (FireShockDevice)sender;
                    _devices.Remove(device);
                    device.Dispose();
                };

                _devices.Add(host);
            }
        }

        public void Stop()
        {
            _deviceLookupTask?.Dispose();

            Log.Information("FireShock Chastity Server stopped");
        }
    }
}
