# Prog Rock

Prog Rock is a rock programmer that implants artificial memories in chunks of
silicon. Specifically, it burns ROM images into 28C64 EEPROMs.

The tool consists of an Arduino sketch that defines the peripheral side of
an EEPROM programmer as well as a Python command-line tool to upload and
download ROM images via the Arduino peripheral.

The Arduino program provides a serial interface to a host computer and handles
the hardware side of programming the 28C64 EEPROM. It parses serial commands
to put in in reading or writing mode, and then either reads the requested byte
range from the EEPROM or writes the specified image file to the EEPROM at a
given offest.

## Notes

- The Arduino bootloader will soft reset the microcontroller to allow a new
program to be loaded if it detects activity on the USB serial connection. The
Python client waits two seconds after connecting to ensure the peripheral code
is running.

- It's important to assert Output Enable on the EEPROM after writing to disable
write mode. Otherwise, resetting the Arduino will bring Write Enable low, which
will write whatever is asserted on the data pins to the last address shifted
onto the address bus.
