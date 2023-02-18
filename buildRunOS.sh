#!/bin/bash
make builds
qemu-system-i386 -s -cdrom bin/OS2023.iso
