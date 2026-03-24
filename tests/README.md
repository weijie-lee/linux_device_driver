# User-Space Test Programs

This directory contains user-space programs for testing the kernel modules in this repository.
They are kept separate from the kernel module source to avoid confusion between kernel-space
and user-space code.

## Directory Structure

```
tests/
├── globalfifo/
│   ├── globalfifo_poll.c    — Tests poll()/select() on /dev/global_mem_*
│   ├── globalfifo_fasync.c  — Tests asynchronous notification (SIGIO/fasync)
│   └── globalfifo_epoll.c   — Tests epoll on /dev/global_mem_*
└── seconds/
    └── second_test.c        — Reads elapsed seconds from /dev/second
```

## Building

Each test program can be compiled independently with gcc:

```bash
# globalfifo tests
gcc -o globalfifo_poll   tests/globalfifo/globalfifo_poll.c
gcc -o globalfifo_fasync tests/globalfifo/globalfifo_fasync.c
gcc -o globalfifo_epoll  tests/globalfifo/globalfifo_epoll.c

# seconds test
gcc -o second_test tests/seconds/second_test.c
```

## Prerequisites

Load the corresponding kernel module before running a test:

```bash
# For globalfifo tests:
sudo insmod global_fifo/globalfifo.ko

# For second_test:
sudo insmod seconds/second.ko
```
