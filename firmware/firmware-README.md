# GreenDo

The control firmware. Plain C on an Arduino Mega 2560, with no third-party libraries other
than the keypad driver — every function in the sketch was written for this project.

## Layout

```
GreenDo/
└── GreenDo.ino
```

The sketch sits in a folder of the same name because the Arduino IDE requires it. Opening
`GreenDo/GreenDo.ino` opens the project.

## Structure

The firmware has three parts and no operating system.

| Part | Responsibility |
| --- | --- |
| `setup()` | Initialises hardware, drives every actuator to a known state, and configures the XBee registers by sending AT command frames over UART1 |
| `loop()` | Polls the keypad and the serial receive path. Event driven, never blocks |
| Timer interrupt | The 1 Hz heartbeat: advances the clock, evaluates the irrigation schedule, samples and filters both sensor chains, services the software timers |

## The software timer layer

The one piece worth reading first. Each timer is one-shot and holds a countdown plus a
function pointer; the heartbeat decrements them and fires the callback on expiry.

It exists because `delay()` cannot be used inside an interrupt — it depends on a timer
interrupt that cannot fire while another handler is running. Anything shaped like "do this
now, then do that in N seconds" arms a timer instead of blocking: pulsing the latching
valve driver, auto-closing the door, releasing the keypad lockout, and re-checking the
light level once the blind has finished travelling.

## Parsing

Both the XBee API frames and the GreenLine messages are decoded with explicit state
machines rather than blocking reads, so a truncated or corrupted message cannot wedge the
controller. The XBee link is 16-bit big-endian and the AVR is little-endian, so incoming
ADC values are byte-swapped on arrival.

## Building

Open `GreenDo/GreenDo.ino` in the Arduino IDE, install the `Keypad` library through the
library manager, select *Arduino Mega 2560*, and upload.

The coordinator radio needs no manual preparation — the firmware writes its registers at
startup, including the network ID, addressing, API mode and the AES key. The remote plant
node was configured once with the vendor's tool and holds its settings in non-volatile
memory.

## Pin map

See the table in the [main README](../README.md#pin-map).
