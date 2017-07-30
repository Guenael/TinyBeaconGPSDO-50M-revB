# TinyBeacon Project -- VHF/UHF radio beacon

*Work in progress -- rev.C*

This code was written for a set of radio beacons, usable on VHF and UHF radio-amateur bands. This firmware drives the components using SPI/I2C ports and manage the sequencer. Beacons can transmit WSPR and PI4 protocols (digital modes) but any other mode usable on class C could be used.
Schematic and PCB layout will be ported on CircuitMaker, and release soon (ref. Rev.D). Previous version helped to improve the design and 4 beacons are now used successfully on air (Montreal, Canada - FN35).
Schematic & PCB designs are offered for 50, 144, 222 & 432 MHz, using a credit card size format. The Output power is 5W (QRP) and the device tolerate an input power supply between 10 and 15V @1.5A max. Finally, the PLL is disciplined by GPS to provide an exact frequency carrier and allows WSPR mode.

<h2>Keywords:</h2>
radio beacon, vhf, uhf, wspr, pi4, qrp, gpsdo

<h3>Basically, this application :</h3>
- Configure the GPS output clock, and configure the PLL
- Encode WSPR, PI4 and CW messages
- Start sequencer:
  - Perform a time alignment, using GPS data
  - Send PI4 message (include MGM, CW, tone), duration 1 minute
  - Send PI4 message (include MGM, CW, tone), duration 1 minute
  - Send WSPR message, duration 2 minute
  - Loop...

<h2>Hardware features:</h2>
- VHF/UHF Beacon (design available for 50, 144, 220 & 440 MHz)
- Compact design / Credit card size
- 10 MHz oscillator stabilized by GPS (GPSDO)
- QRP, 5W output power
- DC-DC Power supply within 10-15V, 1.5A max

<h2>Firmware feature:</h2>
- Support WSPR & PI4 protocols
- Run on a common ATMEGA328p (like Arduino)
- Drive GPS (uBlox Max-8) date/time sync & GPSDO sync signal
- Drive PLL (ADF4355) for frequency adjustment

<h3>Howto:</h3>
1. Install avr-gcc & avr-dude on your Unix disto
2. Edit source code files to change your callsign, locator and frequency (morse.c pi4.c wspr.c)
3. Use "make" to build the firmware
4. Use "make burn" to flash the firmware, with a "AVR Pocket Programmer"
5. Disconnect the programming cable & cut the power supply (the programmer could interfere with SPI port)

<h2>Links for this project:</h2>
- Article : TODO
- Hardware : http://circuitmaker.com/User/Details/Guenael-VA2GKA
- Twitter : https://twitter.com/guenael_jouchet
- Blog : https://www.guenael.ca/

<h3>Notes:</h3>
- Make your first tests with a dummy load (>10W)
- Never setup an antenna with a return loss greater than -10dB / SWR 2:1
- Use a RF power-meter and an ampermeter for calibration (efficiency should be greater than 60%)
- Check thermal dissipation

<h3>TODO:</h3>
- fine frequency test -- click noise problem
- add 6 hex header validation -- gps com' part
- fix timing (email Bo)
- test Si PLL
- better PLL lib, freq calculation without reg. adjustment
- Callsign / loc / freq. in one place
