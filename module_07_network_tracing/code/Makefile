# Makefile for send_trace exercise
# Builds: userspace sender + receiver + kernel modules (send_trace_hw + recv_trace_hw)

# Kernel modules
obj-m := send_trace_hw.o recv_trace_hw.o
KDIR := /lib/modules/$(shell uname -r)/build

# Userspace
CC := gcc
CFLAGS := -Wall -g

.PHONY: all clean load unload test load_recv unload_recv test_both

all: sender receiver modules

sender: sender.c
	$(CC) $(CFLAGS) -o sender sender.c

receiver: receiver.c
	$(CC) $(CFLAGS) -o receiver receiver.c

modules:
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm -f sender receiver
	make -C $(KDIR) M=$(PWD) clean

# Helper targets for send_trace_hw
load:
	sudo insmod send_trace_hw.ko

unload:
	sudo rmmod send_trace_hw

# Helper targets for recv_trace_hw
load_recv:
	sudo insmod recv_trace_hw.ko

unload_recv:
	sudo rmmod recv_trace_hw

# Test sender only
test: sender
	@echo "=== Running sender ==="
	./sender
	@echo ""
	@echo "=== dmesg output ==="
	dmesg | tail -20

# Test both sender + receiver with kprobes
test_both: sender receiver
	@echo "=== Loading modules ==="
	-sudo insmod send_trace_hw.ko 2>/dev/null
	-sudo insmod recv_trace_hw.ko 2>/dev/null
	@echo "=== Running receiver (background) + sender ==="
	(./receiver &) && sleep 1 && ./sender && sleep 2
	@echo ""
	@echo "=== dmesg output ==="
	dmesg | tail -30
