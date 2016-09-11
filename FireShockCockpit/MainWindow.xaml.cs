using System.Windows;

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
            Detector.DeviceAttached += (o, args) => MessageBox.Show(args.ToString());

            Detector.Register();
        }
    }
}
