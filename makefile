SYSTARGET ?= generic
TARGET := armemu
BUILDDIR := build-$(SYSTARGET)

#PROFILE := -pg

OBJDUMP := objdump

# generic cflags
CFLAGS := -O3 -g -Iinclude -Wall -W -Wno-unused-parameter -Wmissing-prototypes -Wno-multichar -finline $(PROFILE)
LDFLAGS := -g $(PROFILE)
LDLIBS := -lSDL

UNAME := $(shell uname -s)
ARCH := $(shell uname -m)

BINEXT := 

# cygwin has some special needs
ifeq ($(findstring CYGWIN,$(UNAME)),CYGWIN)
CFLAGS += -DASM_LEADING_UNDERSCORES=1
LDLIBS += -L/usr/local/lib
BINEXT := .exe
endif

# Darwin (Mac OS X) too
ifeq ($(UNAME),Darwin)
CFLAGS += -DASM_LEADING_UNDERSCORES=1 -mdynamic-no-pic -fast -I/opt/local/include
LDLIBS += -framework Cocoa -L/opt/local/lib -lSDLmain -lstdc++
#CC := clang
endif

ifeq ($(UNAME),Linux)
ifeq ($(ARCH),ppc)
CFLAGS += -fno-pic -mregnames
CFLAGS += -mcpu=7450
CFLAGS += -DWITH_TUNTAP=1
endif
ifeq ($(ARCH),ppc64)
CFLAGS += -mregnames -fno-pic
CFLAGS += -mcpu=970
endif
ifeq ($(ARCH),x86_64)
CFLAGS += -march=native
endif
CFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS += -D__LINUX=1
endif

all:: $(BUILDDIR)/$(TARGET)$(BINEXT) $(BUILDDIR)/$(TARGET).lst

OBJS := \
	main.o \
	config.o \
	debug.o \
	arm/arm.o \
	arm/arm_decoder.o \
	arm/thumb_decoder.o \
	arm/arm_ops.o \
	arm/thumb_ops.o \
	arm/mmu.o \
	arm/uop_dispatch.o \
	arm/cp15.o \
	util/atomic.o \
	util/atomic_asm.o \
	util/math.o

# the sys- dir will have it's own set of files to compile
include sys-$(SYSTARGET)/makefile

OBJS := $(addprefix $(BUILDDIR)/,$(OBJS))

DEPS := $(OBJS:.o=.d)

# The main target
$(BUILDDIR)/$(TARGET)$(BINEXT): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

# A dissassembly of it
$(BUILDDIR)/$(TARGET).lst: $(BUILDDIR)/$(TARGET)$(BINEXT)
ifeq ($(UNAME),Darwin)
	otool -Vt $< > $(BUILDDIR)/$(TARGET).lst
else
	$(OBJDUMP) -d $< > $(BUILDDIR)/$(TARGET).lst
	$(OBJDUMP) -S $< > $(BUILDDIR)/$(TARGET).g.lst
endif

clean:
	rm -f $(OBJS) $(DEPS) $(BUILDDIR)/$(TARGET)$(BINEXT) $(BUILDDIR)/$(TARGET).lst

spotless:
	rm -rf build-*

testbin:
	make -C test

testbinclean:
	make -C test clean

# makes sure the target dir exists
MKDIR = if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi

$(BUILDDIR)/%.o: %.c
	@$(MKDIR)
	@echo compiling $<
	@$(CC) $(CFLAGS) -c $< -MD -MT $@ -MF $(@:%o=%d) -o $@

$(BUILDDIR)/%.o: %.cpp
	@$(MKDIR)
	@echo compiling $<
	@$(CC) $(CFLAGS) -c $< -MD -MT $@ -MF $(@:%o=%d) -o $@

$(BUILDDIR)/%.o: %.S
	@$(MKDIR)
	@echo compiling $<
	@$(CC) $(CFLAGS) -c $< -MD -MT $@ -MF $(@:%o=%d) -o $@

# Empty rule for the .d files. The above rules will build .d files as a side
# effect. Only works on gcc 3.x and above, however.
%.d:

ifeq ($(filter $(MAKECMDGOALS), clean), )
-include $(DEPS)
endif
