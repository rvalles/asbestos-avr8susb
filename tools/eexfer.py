#!/usr/bin/python
# -*- coding: utf-8 -*-
from __future__ import division #1/2 = float, 1//2 = integer, python 3.0 behaviour in 2.6, to make future port to 3 easier.
from optparse import OptionParser
import time
#import serial
import usb.core
import usb.util
import sys
import struct
def dbgprint(dev,msg):
	assert dev.ctrl_transfer(0x40, 1, 0, 0, msg)
	return
def dbgprintnl(dev,msg):
	dbgprint(dev,msg)
	dbgprint(dev,"\n")
	return
def eeread(dev,eeid):
	size=eegetsize(dev,eeid)
	dbgprint(dev,"eexfer> Requesting read eeprom.\n")
	i=0
	while i+8<size:
		buf=dev.ctrl_transfer(0xc0, 8, eeid, i, 8)
		sys.stdout.write(''.join([chr(x) for x in buf]))
		i+=8
	while i<size:
		buf=dev.ctrl_transfer(0xc0, 6, eeid, i, 1)
		sys.stdout.write(''.join([chr(x) for x in buf]))
		i+=1
	return
"""def eewrite(path,verbose):
	f=open(path,"rb")
	buf=f.read()
	f.close()
	if len(buf)>65534:
		print >>sys.stderr,"file is too big."
		return
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
	if verbose:
		sys.stderr.write('\n')"""
def eegetsize(dev,eeid):
	dbgprint(dev,"eexfer> Requesting eeprom data size.\n")
	s=dev.ctrl_transfer(0xc0, 4, eeid, 0, 2)
	if len(s) != 2:
		print >>sys.stderr,"something didn't go well.."
		return
	size=struct.unpack("<H", s)
	size=size[0]
	return size
def eesetsize(dev,eeid,size):
	dbgprint(dev,"eexfer> Setting eeprom data size.\n")
	z=dev.ctrl_transfer(0x40, 5, eeid, size, None)
	if eegetsize(dev,eeid) != size:
		print >>sys.stderr,"something didn't go well..."
		return
	#ser.write(struct.pack("<H",int(size)))
	return
def main():
	optparser = OptionParser("usage: %prog [options]",version="%prog 0.1")
	optparser.add_option("--verbose", action="store_true", dest="verbose", help="be verbose", default=False)
	optparser.add_option("--print", dest="debugprint",help="print string through device's debug output", metavar="msg")
	optparser.add_option("--printn", dest="debugprintn",help="print string through device's debug output without terminating newline", metavar="msg")
	optparser.add_option("--eeid", dest="eeid", help="specify eeprom i2c address", default="0xa0", metavar="hexaddr")
	optparser.add_option("--read", action="store_true", dest="read", help="read eeprom to stdout", default=False)
	optparser.add_option("--write", dest="write",help="write eeprom from file", metavar="path")
	optparser.add_option("--getsize", action="store_true", dest="getsize", help="obtain current data size from eeprom", default=False)
	optparser.add_option("--setsize", dest="setsize", help="forcibly set eeprom data size", metavar="size")
	(options, args) = optparser.parse_args()
	if len(args) != 0:
		optparser.error("incorrect number of arguments")
	eeid=int(options.eeid,16)
	dev = usb.core.find(idVendor=0xaaaa, idProduct=0x3713)
	if dev is None:
	    print >>sys.stderr,"usb device not found."
	    return
	dev.set_configuration()
	if options.debugprint:
		dbgprintnl(dev, options.debugprint)
	if options.debugprintn:
		dbgprint(dev, options.debugprintn)
	if options.read:
		eeread(dev, eeid)
	"""if options.write:
		eewrite(options.write,options.verbose)"""
	if options.getsize:
		print eegetsize(dev, eeid)
	if options.setsize:
		eesetsize(dev, eeid, int(options.setsize))
if __name__ == '__main__':
	main()
