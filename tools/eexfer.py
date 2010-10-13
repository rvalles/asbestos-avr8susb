#!/usr/bin/python
# -*- coding: utf-8 -*-
from __future__ import division #1/2 = float, 1//2 = integer, python 3.0 behaviour in 2.6, to make future port to 3 easier.
from optparse import OptionParser
import time
import serial
import sys
import struct
def eeread(ser):
	size=eegetsize(ser)
	ser.write('r')
	i=0
	while i<size:
		buf=ser.read(256)
		sys.stdout.write(buf)
		i=i+len(buf)
def eewrite(ser,path,verbose):
	f=open(path,"rb")
	buf=f.read()
	f.close()
	eesetsize(ser,len(buf))
	f=open(path,"rb")
	ser.write('w')
	buf=f.read(28)
	while len(buf)!=0:
		ser.write(buf)
		#sys.stdout.write(buf)
		if verbose:
			sys.stderr.write('.')
		buf=f.read(28)
		while len(ser.read())==0:
			pass
def eegetsize(ser):
	ser.write('s')
	s=ser.read(2)
	if len(s) != 2:
		print >>sys.stderr,"programmer not answering."
		return
	size=struct.unpack("<H", s)
	size=size[0]
	return size
def eesetsize(ser,size):
	ser.write('t')
	ser.write(struct.pack("<H",int(size)))
def main():
	optparser = OptionParser("usage: %prog [options] <url>",version="%prog 0.1")
	optparser.add_option("--verbose", action="store_true", dest="verbose", help="verbose", default=False)
	optparser.add_option("--serial", dest="serial", help="specify serial port", default="/dev/ttyUSB0", metavar="dev")
	optparser.add_option("--read", action="store_true", dest="read", help="read eeprom to stdout", default=False)
	optparser.add_option("--write", dest="write",help="write eeprom from file", metavar="path")
	optparser.add_option("--getsize", action="store_true", dest="getsize", help="obtain current data size from eeprom", default=False)
	optparser.add_option("--setsize", dest="setsize", help="forcibly set eeprom data size", metavar="size")
	(options, args) = optparser.parse_args()
	if len(args) != 0:
		optparser.error("incorrect number of arguments")
	ser = serial.Serial(options.serial, 57600, timeout=1)
	time.sleep(3)
	trash=ser.read(10)
	if options.read:
		eeread(ser)
	if options.write:
		eewrite(ser,options.write,options.verbose)
	if options.getsize:
		print eegetsize(ser)
	if options.setsize:
		eesetsize(ser,options.setsize)
if __name__ == '__main__':
	main()
