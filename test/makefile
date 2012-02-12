TARGET := test.bin
TARGETELF := test.elf

#VFP := -Wa,-mfpu=vfp
CFLAGS := -mthumb-interwork -O2 -g -mcpu=arm926ej-s -fno-builtin -msoft-float $(VFP)

LIBGCC := $(shell arm-elf-gcc $(CFLAGS) -print-libgcc-file-name)

OBJS := \
	test.o \
	main.o \
	mmu.o \
	network.o \
	console.o \
	debug.o \
	display.o \
	text.o \
	utils.o \
	block.o \
\
	arm.o \
	thumb.To \

$(TARGET): $(TARGETELF)
	arm-elf-objcopy -O binary $(TARGETELF) $(TARGET)
	arm-elf-objdump -d $(TARGETELF) > $(TARGETELF).lst
	arm-elf-objdump -S $(TARGETELF) > $(TARGETELF).debug.lst

$(TARGETELF): $(OBJS)
	arm-elf-ld -g -T system-onesegment.ld $(OBJS) $(LIBGCC) -o $@

clean:
	rm -f $(TARGET) $(TARGETELF) $(TARGETELF).lst
	rm -f $(OBJS)

%.o: %.S
	arm-elf-gcc $(CFLAGS) -c $< -o $@

%.o: %.c
	arm-elf-gcc $(CFLAGS) -c $< -o $@

%.To: %.c
	arm-elf-gcc $(CFLAGS) -mthumb -c $< -o $@
