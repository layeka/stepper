DEVICE  = atmega32
F_CPU   = 12000000	# in Hz

CFLAGS  = -Iusbdrv -I. -DDEBUG_LEVEL=0
OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o

COMPILE = avr-gcc -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)

hex: main.elf
	rm -f main.hex
	avr-objcopy -R .eeprom -O ihex main.elf main.hex
	avr-size main.elf

main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf -Wl,-Map,main.map $(OBJECTS)

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

clean:
	rm -f main.hex main.lst main.obj main.cof main.list main.map main.eep.hex main.elf *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s

load: main.hex
	avrdude -P usb -c usbasp -p m32 -U flash:w:main.hex:i

fuse:
	avrdude -P usb -c usbasp -p m32 -U lfuse:w:lfuse.bin:r
	avrdude -P usb -c usbasp -p m32 -U hfuse:w:hfuse.bin:r

