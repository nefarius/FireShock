using System.Linq;
using System.Windows;
using HidLibrary;

namespace FireShockCockpit
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private static readonly FireShockDetector Detector = new FireShockDetector();

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            FireShockControllersListBox.ItemsSource = FireShockDetector.FireShockDevices;

            Detector.DeviceAttached += (o, args) => MessageBox.Show(args.ToString());

            Detector.Register();

            //foreach (var hidDevice in HidDevices.Enumerate())
            //{
            //    MessageBox.Show(hidDevice.Description);
            //}
        }
    }
}
