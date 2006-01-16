TARGET := test.bin
TARGETELF := test.elf

TOOLCHAIN_PATH := /usr/local/arm-elf/bin

#VFP := -Wa,-mfpu=vfp
CFLAGS := -mthumb-interwork -O2 -g -mcpu=arm926ej-s -fno-builtin -msoft-float $(VFP)

LIBGCC := $(shell $(TOOLCHAIN_PATH)/arm-elf-gcc $(CFLAGS) -print-libgcc-file-name)

OBJS := \
	test.o \
	main.o \
	mmu.o \
	console.o \
	debug.o \
	display.o \
	text.o \
	utils.o \
\
	arm.o \
	thumb.To \

$(TARGET): $(TARGETELF)
	$(TOOLCHAIN_PATH)/arm-elf-objcopy -O binary $(TARGETELF) $(TARGET)
	$(TOOLCHAIN_PATH)/arm-elf-objdump -S $(TARGETELF) > $(TARGETELF).lst

$(TARGETELF): $(OBJS)
	$(TOOLCHAIN_PATH)/arm-elf-ld -g -Ttext=0 $(OBJS) $(LIBGCC) -o $@

clean:
	rm -f $(TARGET) $(TARGETELF) $(TARGETELF).lst
	rm -f $(OBJS)

%.o: %.S
	$(TOOLCHAIN_PATH)/arm-elf-gcc $(CFLAGS) -c $< -o $@

%.o: %.c
	$(TOOLCHAIN_PATH)/arm-elf-gcc $(CFLAGS) -c $< -o $@

%.To: %.c
	$(TOOLCHAIN_PATH)/arm-elf-gcc $(CFLAGS) -mthumb -c $< -o $@
