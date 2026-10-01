# Wiring - Pyango v0.1

<p align="center">
  <img src="images/pyango-wiring1.jpg" width="45%">
  <img src="images/pyango-wiring2.jpg" width="45%">
</p>

The ESP32 DevKit sits on female headers on top of the perfboard. The battery and the charger module go underneath it.

## Display and buttons

| Component | Component pin | ESP32 DevKit V1 |
|---|---|---|
| SSD1306 128x64 OLED (I2C) | VCC | 3V3 |
| | GND | GND |
| | SDA | GPIO21 |
| | SCL | GPIO22 |
| Left button | one leg | GPIO32 |
| | diagonal leg | GND |
| Right button | one leg | GPIO33 |
| | diagonal leg | GND |

- **No resistors needed:** the firmware enables the ESP32's internal pull-up resistors.
- **4-leg buttons:** the legs are internally connected in pairs. Use two legs placed **diagonally** so you never wire two legs of the same pair.
- **Check your OLED's pin order:** some modules swap VCC and GND.
- **1.3" display (SH1106):** change the display constructor at the top of `pyango.ino`.

## Battery

Parts: a 1S LiPo battery (Pyango uses a 400 mAh 602035), a **TP4056 charger module with protection** (the version with `B+ B-` and `OUT+ OUT-` pads) and a small power switch.

| From | To |
|---|---|
| Battery + | TP4056 `B+` |
| Battery − | TP4056 `B-` |
| TP4056 `OUT+` | ESP32 `VIN` |
| TP4056 `OUT-` | Power switch (one leg) |
| Power switch (other leg) | ESP32 `GND` |

To charge the battery, plug a USB cable into the **TP4056 module**, not into the ESP32.

###  Read before using the battery

**1. Turn the switch OFF before plugging USB into the ESP32.**
On the ESP32 DevKit, the USB 5 V reaches the `VIN` pin, which is connected to the battery through `OUT+`. If the switch is ON while you program the board over USB, those 5 V go straight into the battery without passing through the charger, and only the module's protection circuit stops it. With the switch OFF there is no path for that current, so **always switch off before connecting the ESP32 to your computer.**

*Optional extra protection:* a Schottky diode (1N5817 or SS14) between `OUT+` and `VIN`, with the band towards `VIN`, blocks that current completely. The trade-off is that it drops about 0.3 V, so the low-battery resets described below may start earlier.

**2. Set the charge current for your battery.**
TP4056 modules come set to **1 A**, which is too much for small batteries. A safe rule is to charge at no more than the battery's capacity (400 mA for a 400 mAh battery). The current is set by the programming resistor, usually marked `R3` with the code `122` (1.2 kΩ):

| Resistor | Code | Charge current |
|---|---|---|
| 1.2 kΩ (factory) | 122 | 1000 mA |
| 2 kΩ | 202 | 580 mA |
| 3 kΩ | 302 | 400 mA |
| 5 kΩ | 502 | 250 mA |
| 10 kΩ | 103 | 130 mA |

For a 400 mAh battery, use **3 k or higher**. Until you change it, charge only while you can watch it, and unplug it if the battery gets warm.

**3. Possible resets when the battery is low.**
`VIN` goes through the DevKit's own 3.3 V regulator (usually an AMS1117), which needs about 1 V more than its output. With a full battery Pyango works fine, but as the battery drains the ESP32 may restart on its own. That's a sign it's time to charge. Future versions will use a low-dropout regulator to fix this.
