YASIO
=====

Yet another [Atari XL/XE](http://en.wikipedia.org/wiki/Atari_8-bit_family) SIO
floppy drive emulator - Yet-Another-SIO.

Features
--------

* Handles ATR and Atari executable files.
* Files stored on a micro-SD FAT32-formatted card.
* Works at speeds up to 115200 BPS.
* Controlled via a WWW browser via WiFi connection.
* Works in WiFi access point or station mode.

Hardware
--------

* Based on an ESP32-S3 devboard.
* Fits into a USB3 device enclosure.

Programming & Asembly
---------------------

Prerequisities:

* [xasm](https://github.com/pfusik/xasm)
* [foco65](https://github.com/piotr-wiszowaty/foco65)
* [PlatformIO](https://platformio.org)

0. Solder all of the components to the PCB.
1. Compile bootloader (optional): `bootloader/generate-bootloader.sh`.
2. Compile setup program: `setup/build.sh`.
3. Connect the device via USB-C cable to a PC (do not connect it to an Atari
   computer yet).
4. Flash the firmware by executing `pio run -e 0 -t upload` in `uc`
   subdirectory.
5. Disconnect the device from Atari computer; put the device in the enclosure.

Usage
-----

### SD card preparation

Before turning on the device prepare an SD card - write configuration file
(`setup/yasio.conf`) and setup program (`setup/yasio_setup.xex`) to the root
directory of the SD card filesystem.

The configuration file contents may be prepared beforehand by using a text
editor or setup using the setup program. The setup program is 'inserted' into
`D1` drive by holding the setup button when turning on the device or by
pressing the setup button when the device is working and rebooting the Atari
computer.

### Configuration

Configuration file `yasio.conf` is an ASCII text file with LF-terminated lines.
Each line consists one configuration entry. Each entry starts with an entry key
followed by `:` character and (optional) entry value.
Available configuration entries and :

* `D1`, `D2`, `D3`, `D4` - path to file 'inserted' into appropriate drive;
  e.g.: `D1:demo/Drunk Chessboard.atr`
* `MO` - work mode; available values: `Station`, `AccessPoint`
* `AP` - WiFi access point parameters: SSID, password, IP-address/netmask;
  e.g.: `AP:My_ssid my_password 192.168.8.1/24`
* `ST` - WiFi station parameters: SSID, password; e.g.: `Some_ssid
  password123`; may occur multiple times

### LED indicator colors

* green - SD card initialization successful, WiFi initialization successful
* red - SD card initialization failure
* purple - failure connecting to WiFi access point

References
----------

The bootloader used for loading executable files is based on one
found in [Atari800](https://atari800.github.io/).

The code for controlling the LED on the devboard is by Espressif Systems
(Shanghai) CO LTD.
