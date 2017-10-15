using System;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.IO;
using System.Linq;
using System.Reactive.Linq;
using System.Reflection;
using Nefarius.Devcon;
using Nefarius.Sub.Kinbaku.Core.Plugins;
using Serilog;

namespace FireShock.Chastity.Server
{
    internal class ChastityService
    {
        private readonly IObservable<long> _deviceLookupSchedule = Observable.Interval(TimeSpan.FromSeconds(2));
        private readonly ObservableCollection<FireShockDevice> _devices = new ObservableCollection<FireShockDevice>();

        private readonly SinkPluginHost _sinkPluginHost = new SinkPluginHost(Path.Combine(Path.GetDirectoryName
            (Assembly.GetExecutingAssembly().Location), "Plugins"));

        private IDisposable _deviceLookupTask;

        public void Start()
        {
            Log.Information("FireShock Chastity Server started");

            _devices.CollectionChanged += (sender, args) =>
            {
                switch (args.Action)
                {
                    case NotifyCollectionChangedAction.Add:
                        foreach (IDualShockDevice item in args.NewItems)
                            _sinkPluginHost.DeviceArrived(item);
                        break;
                    case NotifyCollectionChangedAction.Remove:
                        foreach (IDualShockDevice item in args.OldItems)
                            _sinkPluginHost.DeviceRemoved(item);
                        break;
                }
            };

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

                var device = new FireShockDevice(path);

                device.DeviceDisconnected += (sender, args) =>
                {
                    var dev = (FireShockDevice) sender;
                    Log.Information($"Device {dev} disconnected");
                    _devices.Remove(dev);
                    dev.Dispose();
                };

                device.InputReportReceived += (sender, args) =>
                    _sinkPluginHost.InputReportReceived((IDualShockDevice) sender, args.Report);

                _devices.Add(device);
            }
        }

        public void Stop()
        {
            _deviceLookupTask?.Dispose();

            foreach (var device in _devices)
                device.Dispose();

            _devices.Clear();

            Log.Information("FireShock Chastity Server stopped");
        }
    }
}