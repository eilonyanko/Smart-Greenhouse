# GreenSee

The desktop half of the system: a WPF application that configures the controller, displays
live telemetry, and provides manual actuator tests.

Version 1.0.2. C# on .NET Framework 4.6.1, built with Visual Studio 2017.

## Layout

```
GreenSee.sln
GreenSee/
├── App.xaml  ·  App.xaml.cs  ·  App.config
├── MainWindow.xaml           the entire interface
├── MainWindow.xaml.cs        protocol, telemetry and tests
└── Properties/
```

## How it works

Everything is driven from a single 100 ms timer. There is no background thread and no
event-driven serial callback — each tick drains whatever bytes have arrived and feeds them
one at a time into a receive state machine identical to the one in the firmware.

| Member | Responsibility |
| --- | --- |
| `MainWindow()` | Opens the serial port at 9600 8N1 and starts the master timer |
| `MainTimerTick()` | Drains the receive buffer; also runs the link-loss counter |
| `GreenLineStateMachine()` | Finds `~`, collects until line feed, hands off a message |
| `ParseMsg()` | Dispatches on the type character and updates the interface |
| `ConfigurationButton_Click()` | Serialises every field and sends the full configuration |

## Interface behaviour

State is encoded in colour rather than decoration, so the greenhouse can be read at a
glance:

- **Temperature** — the gauge turns red above the configured ceiling, cyan below the floor,
  and green inside the band.
- **Tensiometer** — yellow when the soil is drier than the threshold, green when moist.
- **Light** — yellow above the low-light threshold, grey below it.
- **Battery** — the remote node's voltage is converted to a percentage across the 7.5 V to
  9 V range and shown as a bar.
- **Signal** — four rectangles fill according to RSSI. If no telemetry arrives for 100
  consecutive ticks, roughly ten seconds, the display falls back to a no-signal state and
  all four clear.

The weekly schedule occupies the centre of the window: one column per day, each with an
enable checkbox, irrigation time and duration, and the start and end of the light window.
A debug pane at the bottom accumulates every `~D=` string the controller emits.

## Building

Requires Windows, Visual Studio with the .NET Framework 4.6.1 targeting pack, and WPF.
Open `GreenSee.sln` and build.

The serial port name is hard-coded in the `MainWindow` constructor:

```csharp
GreenLine.PortName = "COM7";
```

Change it to match the port the controller enumerates as, or the application will report
that it cannot open the channel on startup.
