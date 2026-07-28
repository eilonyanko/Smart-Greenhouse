using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

using System.IO.Ports;
using System.Windows.Threading;
using System.IO;


namespace GreenSee
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {

        // Put here global costatnats & Variables
        int counter = 0;
        bool TestsFansIsOn = false;
        bool TestsLightsIsOn = false;
        bool TestsValveIsOpen = false;
        int NoSignalCnt = 0;

        enum GreenLine_state
        {
            SearchSOM, //search start of message
            CollectMsg
        };
        
        GreenLine_state state = GreenLine_state.SearchSOM;
        private char[] MsgBuffer = new char[256];
        private int MsgBufferIndex = 0;

        // serial port to communicate with Arduino
        private SerialPort GreenLine = new SerialPort();

        // GreenSee main 100msec timer
        private DispatcherTimer MainTimer = new DispatcherTimer();

        public MainWindow()
        {
            InitializeComponent();

            // Initlize timer
            MainTimer.Interval = TimeSpan.FromMilliseconds(100);
            MainTimer.Tick += MainTimerTick;

            // from here start button code 
            // start timer
            MainTimer.Start();

            //configure arduino COM port
            GreenLine.PortName = "COM7";
            GreenLine.BaudRate = 9600;
            GreenLine.Handshake = System.IO.Ports.Handshake.None;
            GreenLine.Parity = Parity.None;
            GreenLine.DataBits = 8;
            GreenLine.StopBits = StopBits.One;
            GreenLine.ReadBufferSize = 4096;
            GreenLine.ReadTimeout = -1;
            GreenLine.ReceivedBytesThreshold = 1;

            GreenLine.Open();
            GreenLine.DiscardInBuffer();


            if (GreenLine.IsOpen == false)
                MessageBox.Show("לא ניתן לפתוח ערוץ סיריאלי", "GreenSee");


        }

        void ParseMsg()
        {
            string s = "";
            for(int i = 2; i < MsgBufferIndex; i++)
                s += MsgBuffer[i];
            switch(MsgBuffer[0])
            {
                case 'D':
                    DebugBlock.Text += s;
                    DebugScroll.ScrollToBottom();
                    break;
                case 'C':
                    CurrentTemprature.Text = s;
                    TempratureSlider.Value = int.Parse(s);
                    if(TempratureSlider.Value > int.Parse(TempratureHigh.Text))
                        TempratureSlider.Background = Brushes.Red;
                    else if(TempratureSlider.Value < int.Parse(TempratureLow.Text))
                        TempratureSlider.Background = new SolidColorBrush(Color.FromArgb(0x7F, 0x0B, 0xBF, 0xBC));
                    else
                        TempratureSlider.Background = Brushes.LightGreen;
                    break;
                case 'T':
                    ArduinoTime.Text = s;
                    break;
                case 'B':
                    float val = float.Parse(s);
                    if(val < 7.5)
                        val = 7.5f;
                    int batteryLevel = (int)((val - 7.5) / 1.5 * 100);
                    BatteryLevel.Value = batteryLevel;
                    BatteryLevelPercentage.Text = batteryLevel.ToString() + "%";
                    break;
                case 'M':
                    CurrentVoltage.Text = s;
                    TensiometerSlider.Value = float.Parse(s);
                    if (TensiometerSlider.Value > 0.7)
                        TensiometerSlider.Background = new SolidColorBrush(Color.FromArgb(0xFF, 0xC6, 0xD4, 0x1C));
                    else
                        TensiometerSlider.Background = new SolidColorBrush(Color.FromArgb(0xFF, 0x26, 0xAE, 0x26));
                    break;
                case 'L':
                    CurrentLux.Text = s;
                    LuxSlider.Value = int.Parse(s);
                    if (LuxSlider.Value > int.Parse(LuxLow.Text))
                        LuxSlider.Background = Brushes.Yellow;
                    else
                        LuxSlider.Background = new SolidColorBrush(Color.FromArgb(0xFF, 0x80, 0x80, 0x80));
                    break;
                case 'R':
                    RSSI.Text = s;
                    NoSignalCnt = 0;
                    int RSSIValue = int.Parse(s);
                    if (RSSIValue > 50)
                    {
                        Rect4.Fill = Brushes.Green;
                        Rect3.Fill = Brushes.White;
                        Rect2.Fill = Brushes.White;
                        Rect1.Fill = Brushes.White;
                    }
                    else if (RSSIValue > 40 )
                    {
                        Rect4.Fill = Brushes.Green;
                        Rect3.Fill = Brushes.Green;
                        Rect2.Fill = Brushes.White;
                        Rect1.Fill = Brushes.White;
                    }
                    else if (RSSIValue > 30)
                    {
                        Rect4.Fill = Brushes.Green;
                        Rect3.Fill = Brushes.Green;
                        Rect2.Fill = Brushes.Green;
                        Rect1.Fill = Brushes.White;
                    }
                    else
                    {
                        Rect4.Fill = Brushes.Green;
                        Rect3.Fill = Brushes.Green;
                        Rect2.Fill = Brushes.Green;
                        Rect1.Fill = Brushes.Green;
                    }

                    break;
                default:
                    MessageBox.Show("הודעה לא מוכרת", "GreenSee");
                    break;
            }
        }

        void GreenLineStateMachine(char rxChar)
        {
            switch(state)
            {
                case GreenLine_state.SearchSOM:
                    if(rxChar == '~')
                        state = GreenLine_state.CollectMsg;
                    MsgBufferIndex = 0;
                    break;
                case GreenLine_state.CollectMsg:
                    MsgBuffer[MsgBufferIndex] = rxChar;
                    MsgBufferIndex++;
                    if(rxChar == 10) //new line character, indicates end of message
                    {
                        state = GreenLine_state.SearchSOM;
                        ParseMsg();
                    }
                    break;
            }
        }

        void MainTimerTick(object sender, EventArgs e)
        {
            int rxChar;
            NoSignalCnt++;
            if (NoSignalCnt >= 100)
            {
                NoSignalCnt = 100;
                RSSI.Text = "אין קליטה";
                Rect4.Fill = Brushes.White;
                Rect3.Fill = Brushes.White;
                Rect2.Fill = Brushes.White;
                Rect1.Fill = Brushes.White;
            }
            counter++;
            TimerText.Text = counter.ToString();
            while(GreenLine.BytesToRead > 0)
            {
                rxChar = GreenLine.ReadByte();
                GreenLineStateMachine((char) rxChar);
            }
        }

        private void ConfigurationButton_Click(object sender, RoutedEventArgs e)
        {
            string s;
            s = String.Format("~I=0,{0},{1},{2},{3},{4}", (bool) (SundayCheckbox.IsChecked) ? 1 : 0, 
                SundayIrigationTime.Text.Replace(':', ','), SundayIrigationDuration.Text, SundayLightStartTime.Text.Replace(':', ','), SundayLightEndTime.Text.Replace(':', ','));
            GreenLine.WriteLine(s);
            s = String.Format("~I=1,{0},{1},{2},{3},{4}", (bool)(MondayCheckbox.IsChecked) ? 1 : 0,
                MondayIrigationTime.Text.Replace(':', ','), MondayIrigationDuration.Text, MondayLightStartTime.Text.Replace(':', ','), MondayLightEndTime.Text.Replace(':', ','));
            GreenLine.WriteLine(s);
            s = String.Format("~I=2,{0},{1},{2},{3},{4}", (bool)(TuesdayCheckbox.IsChecked) ? 1 : 0,
                TuesdayIrigationTime.Text.Replace(':', ','), TuesdayIrigationDuration.Text, TuesdayLightStartTime.Text.Replace(':', ','), TuesdayLightEndTime.Text.Replace(':', ','));
            GreenLine.WriteLine(s);
            s = String.Format("~I=3,{0},{1},{2},{3},{4}", (bool)(WednesdayCheckbox.IsChecked) ? 1 : 0,
                WednesdayIrigationTime.Text.Replace(':', ','), WednesdayIrigationDuration.Text, WednesdayLightStartTime.Text.Replace(':', ','), WednesdayLightEndTime.Text.Replace(':', ','));
            GreenLine.WriteLine(s);
            s = String.Format("~I=4,{0},{1},{2},{3},{4}", (bool)(ThursdayCheckbox.IsChecked) ? 1 : 0,
                ThursdayIrigationTime.Text.Replace(':', ','), ThursdayIrigationDuration.Text, ThursdayLightStartTime.Text.Replace(':', ','), ThursdayLightEndTime.Text.Replace(':', ','));
            GreenLine.WriteLine(s);
            s = String.Format("~I=5,{0},{1},{2},{3},{4}", (bool)(FridayCheckbox.IsChecked) ? 1 : 0,
                FridayIrigationTime.Text.Replace(':', ','), FridayIrigationDuration.Text, FridayLightStartTime.Text.Replace(':', ','), FridayLightEndTime.Text.Replace(':', ','));
            GreenLine.WriteLine(s);
            s = String.Format("~I=6,{0},{1},{2},{3},{4}", (bool)(SaturdayCheckbox.IsChecked) ? 1 : 0,
                SaturdayIrigationTime.Text.Replace(':', ','), SaturdayIrigationDuration.Text, SaturdayLightStartTime.Text.Replace(':', ','), SaturdayLightEndTime.Text.Replace(':', ','));
            GreenLine.WriteLine(s);
            s = String.Format("~H={0}", TempratureHigh.Text);
            GreenLine.WriteLine(s);
            s = String.Format("~L={0}", TempratureLow.Text);
            GreenLine.WriteLine(s);
            s = String.Format("~V={0}", LuxLow.Text);
            GreenLine.WriteLine(s);
            s = String.Format("~U={0}", DryLevel.Text);
            GreenLine.WriteLine(s);
            DateTime localDate = DateTime.Now;
            int dayOfWeek = (int) localDate.DayOfWeek;
            s = String.Format("~T={0},{1},{2},{3}", dayOfWeek, localDate.Hour, localDate.Minute, localDate.Second);
            GreenLine.WriteLine(s);
            s = String.Format("~Y={0}", ShutterUpDelay.Text);
            GreenLine.WriteLine(s);
            s = String.Format("~Z={0}", ShutterDownDelay.Text);
            GreenLine.WriteLine(s);
        }

        private void FansTest_Click(object sender, RoutedEventArgs e)
        {
            if (TestsFansIsOn)
            {
                FansTest.Background = new SolidColorBrush(Color.FromArgb(0xFF, 221, 221, 221));
                TestsFansIsOn = false;
                FansTest.Content = "Fans On";
                GreenLine.WriteLine("~X=2");
            }   
            else
            {
                FansTest.Background = new SolidColorBrush(Color.FromArgb(0xFF, 212, 42, 42));
                TestsFansIsOn = true;
                FansTest.Content = "Fans Off";
                GreenLine.WriteLine("~X=1");
            }
        }

        private void LightsTest_Click(object sender, RoutedEventArgs e)
        {
            if (TestsLightsIsOn)
            {
                LightsTest.Background = new SolidColorBrush(Color.FromArgb(0xFF, 221, 221, 221));
                TestsLightsIsOn = false;
                LightsTest.Content = "Lights On";
                GreenLine.WriteLine("~X=4");
            }
            else
            {
                LightsTest.Background = new SolidColorBrush(Color.FromArgb(0xFF, 212, 42, 42));
                TestsLightsIsOn = true;
                LightsTest.Content = "Lights Off";
                GreenLine.WriteLine("~X=3");
            }
        }

        private void ShutterTestDown_Click(object sender, RoutedEventArgs e)
        {
           GreenLine.WriteLine("~X=6");
        }
        private void ShutterTestUp_Click(object sender, RoutedEventArgs e)
        {
            GreenLine.WriteLine("~X=5");
        }

        private void TestWaterValve_Click(object sender, RoutedEventArgs e)
        {
            if (TestsValveIsOpen)
            {
                TestWaterValve.Background = new SolidColorBrush(Color.FromArgb(0xFF, 221, 221, 221));
                TestsValveIsOpen = false;
                TestWaterValve.Content = "Open Valve";
                GreenLine.WriteLine("~X=8");
            }
            else
            {
                TestWaterValve.Background = new SolidColorBrush(Color.FromArgb(0xFF, 212, 42, 42));
                TestsValveIsOpen = true;
                TestWaterValve.Content = "Close Valve";
                GreenLine.WriteLine("~X=7");
            }
        }
    }
}
