#!/bin/sh
cat cfg1hdr.bin payload1.bin >cfg1_payload1.bin
dd if=/dev/zero bs=1 count=$((3840-`stat -L -c %s payload1.bin`)) >>cfg1_payload1.bin
tools/bin2header/bin2header cfg1_payload1.bin cfg1_payload1.h port1_config_descriptor_payload1
cat cfg1hdr.bin payload2.bin >cfg1_payload2.bin
dd if=/dev/zero bs=1 count=$((3840-`stat -L -c %s payload2.bin`)) >>cfg1_payload2.bin
tools/bin2header/bin2header cfg1_payload2.bin cfg1_payload2.h port1_config_descriptor_payload2
cat cfg1hdr.bin payload3.bin >cfg1_payload3.bin
dd if=/dev/zero bs=1 count=$((3840-`stat -L -c %s payload3.bin`)) >>cfg1_payload3.bin
tools/bin2header/bin2header cfg1_payload3.bin cfg1_payload3.h port1_config_descriptor_payload3
