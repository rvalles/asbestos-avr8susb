#!/bin/sh
dd if=../stage2.bin of=../stage2a.bin bs=1 count=32767
dd if=../stage2.bin of=../stage2b.bin bs=1 skip=32767 count=32767
bin2header/bin2header ../stage2a.bin ../stage2a.h stage2a
bin2header/bin2header ../stage2b.bin ../stage2b.h stage2b
