#!/bin/sh
cat cfg1hdr.bin stage1.bin >cfg1.bin
dd if=/dev/zero bs=1 count=$((3840-`stat -L -c %s stage1.bin`)) >>cfg1.bin
tools/bin2header/bin2header cfg1.bin cfg1.h port1_config_descriptor

