CC=avr-gcc
OBJCPY=avr-objcopy
CFLAGS= -Wall -Wformat-overflow=0 -Os -std=c99 -ffunction-sections -fdata-sections -MMD -mmcu=atmega328p
LDFLAGS = -Wall -Os -Wl,--gc-sections,--relax -mmcu=atmega328p
LIBS = -lm

OBJ = twi.o gps.o pll.o usart.o morse.o pi4.o wspr.o gpsdo.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@  

gpsdo: $(OBJ)
	$(CC) $(LDFLAGS) -o gpsdo.elf $^ $(LIBS)
	$(OBJCPY) -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 gpsdo.elf gpsdo.hex
	$(OBJCPY) -O ihex -R .eeprom gpsdo.elf gpsdo.hex
	avr-size -C --mcu=atmega328p gpsdo.elf

burn:
	avrdude -c usbtiny -p ATMEGA328P -v -U flash:w:gpsdo.hex -U lfuse:w:0xe0:m -U hfuse:w:0xd8:m -U efuse:w:0xff:m

clean:
	rm -f *.o *.d *.elf *.hex
