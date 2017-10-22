using Serilog;
using Topshelf;

namespace FireShock.Chastity.Server
{
    class Program
    {
        static void Main(string[] args)
        {
            Log.Logger = new LoggerConfiguration()
                .MinimumLevel.Information()
                .WriteTo.Console()
                .WriteTo.RollingFile("FireShock.Chastity.Server-{Date}.log")
                .CreateLogger();

            HostFactory.Run(x =>
            {
                x.Service<ChastityService>(s =>
                {
                    s.ConstructUsing(name => new ChastityService());
                    s.WhenStarted(tc => tc.Start());
                    s.WhenStopped(tc => tc.Stop());
                });
                x.RunAsLocalSystem();

                x.SetDescription("Communicates with FireShock USB Devices.");
                x.SetDisplayName("FireShock Chastity Server");
                x.SetServiceName("FireShock.Chastity.Server");
                x.DependsOn("Kinbaku.Hub.Server");
            });
        }
    }
}
