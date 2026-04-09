ifndef TARGET_COMPILE
    TARGET_COMPILE = /root/工作/arm-gnu-toolchain-12.2.rel1-aarch64-aarch64-none-elf/bin/aarch64-none-elf-
endif

ifndef KP_DIR
    KP_DIR = ../../KernelPatch
endif

CC  = $(TARGET_COMPILE)gcc
STRIP = $(TARGET_COMPILE)strip

INCLUDE_DIRS := include patch/include linux/include linux/arch/arm64/include linux/tools/arch/arm64/include
INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

TARGET := MyUname.kpm
SRC    := src/MyUname.c
OBJ    := src/MyUname.o

all: CFLAGS :=
all: $(TARGET)

debug: CFLAGS := -DMYUNAME_DEBUG
debug: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -r -o $@ $^
	$(STRIP) --strip-unneeded $@

$(OBJ): $(SRC)
	$(CC) -Wall -Wextra -O2 $(CFLAGS) $(INCLUDE_FLAGS) -c -o $@ $<

.PHONY: all debug clean
clean:
	rm -f *.kpm *.o src/*.o
