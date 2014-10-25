DEVICE  = atmega32
F_CPU   = 12000000	# in Hz

AS = avr-as 
ASFLAGS = $(CFLAGS)

CC = avr-gcc
CFLAGS  = -Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(DEVICE) -Iusbdrv -I. -DDEBUG_LEVEL=0

OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o



main.hex: main.elf
	rm -f main.hex
	avr-objcopy -R .eeprom -O ihex main.elf main.hex
	avr-size main.elf

main.elf: $(OBJECTS)
	$(CC) $(CFLAGS) -o main.elf -Wl,-Map,main.map $(OBJECTS)

#.c.o:
#	$(COMPILE) -c $< -o $@

%.o: %.S
	$(CC) -x assembler-with-cpp $(CFLAGS) -c $< -o $@

clean:
	rm -f main.hex main.lst main.obj main.cof main.list main.map main.eep.hex main.elf *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s

load: main.hex
	avrdude -P usb -c usbasp -p m32 -U flash:w:main.hex:i

fuse:
	avrdude -P usb -c usbasp -p m32 -U lfuse:w:0xfc:m -U hfuse:w:0xd9:m

