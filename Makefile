# Makefile
#
# Copyright (C) 2017

CROSS = riscv64-unknown-elf-
CC = gcc
CFLAGS = -Ienv -Icommon -std=gnu99 -O3 -DPREALLOCATE=1
LDFLAGS = -T ./common/test.ld -nostdlib -nostartfiles -lc

diag: diag.o syscalls.o crt.o
	$(CROSS)$(CC) $^ $(LDFLAGS) -o $@

.PHONY: clean
clean:
	rm -f *.o diag

%.o: %.c
	$(CROSS)$(CC) $(CFLAGS) -o $@ -c $<

%.o: common/%.c
	$(CROSS)$(CC) $(CFLAGS) -o $@ -c $<

%.o: common/%.S
	$(CROSS)$(CC) $(CFLAGS) -o $@ -c $<

# Rebuilding other components of lowrisc-chip

.PHONY: rebuild-fesvr

rebuild-fesvr:
	$(MAKE) -C $(TOP)/riscv-tools/riscv-fesvr/build install

.PHONY: run-diag

run-diag: diag
	$(TOP)/emulator/emulator-DefaultCPPConfig +disk=/dev/null diag
