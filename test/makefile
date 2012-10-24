TARGET := test.bin
TARGETELF := test.elf

CC := arm-eabi-gcc
LD := arm-eabi-ld
OBJDUMP := arm-eabi-objdump
OBJCOPY := arm-eabi-objcopy

#VFP := -Wa,-mfpu=vfp
CFLAGS := -mthumb-interwork -O2 -g -mcpu=cortex-a8 -fno-builtin -msoft-float $(VFP)

LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

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
	$(OBJCOPY) -O binary $(TARGETELF) $(TARGET)
	$(OBJDUMP) -d $(TARGETELF) > $(TARGETELF).lst
	$(OBJDUMP) -S $(TARGETELF) > $(TARGETELF).debug.lst

$(TARGETELF): $(OBJS)
	$(LD) -g -T system-onesegment.ld $(OBJS) $(LIBGCC) -o $@

clean:
	rm -f $(TARGET) $(TARGETELF) $(TARGETELF).lst
	rm -f $(OBJS)

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.To: %.c
	$(CC) $(CFLAGS) -mthumb -c $< -o $@
