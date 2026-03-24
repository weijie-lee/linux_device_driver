# Top-level Makefile for Linux Device Driver Examples
# Copyright (C) 2026 weijie-lee

# List all subdirectories with driver code
DRIVERS := \
	container_of \
	dma \
	global_fifo \
	global_mem \
	kfifo \
	linked_lists \
	seconds \
	snull \
	vmem_disk

.PHONY: all clean $(DRIVERS)

all: $(DRIVERS)

$(DRIVERS):
	@echo "=== Building $@ ==="
	$(MAKE) -C $@

clean: $(DRIVERS:%=%-clean)

%-clean:
	@echo "=== Cleaning $* ==="
	$(MAKE) -C $* clean

help:
	@echo "Linux Device Driver Examples Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make all        - Build all drivers"
	@echo "  make clean      - Clean all drivers"
	@echo "  make <dir>      - Build specific driver (e.g., make container_of)"
	@echo "  make help       - Show this help"
