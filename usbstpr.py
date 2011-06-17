#!/usr/bin/env python
"""
DESCRIPTION:
  Interface to stepper motor driver board v.0.1a.

AUTHOR: 
  piton_at_erg.biophys.msu.ru
"""
import sys
import time
import usb

class usbstpr:
  """
from usbstpr import usbstpr

stpr = usbstpr() # find device and create `usbstpr` instance binded to device OR raise an exception
stpr.set_del(5) # set delay to 5 ms
stpr.step(3645) # step 3645 steps (probably) clockwise
stpr.step(-2635) # step 2635 steps (probably) counterclockwise
stpr.stop() # stop motion immedeatly
stpr.pwr_down() # turn current through coils OFF
stpr.pwr_up() # turn current ON
  """
  # device description
  idVendor = 0x16c0
  idProduct = 0x05dc
  Manufacturer = "piton"
  Product = "stepper"
  mydevh = None
  
  def __init__(self, ibus=0, idev=0):
    if ibus and idev:
      print "Using explicitly defined device"
      raise NameError, "Not implemented."
    ndev = 0
    ibus = 0
# find device
    for bus in usb.busses():
      ibus += 1
      idev = 0
      for dev in bus.devices:
        idev += 1
        if dev.idVendor == self.idVendor and dev.idProduct == self.idProduct:
          devh = dev.open()
          if devh.getString(dev.iManufacturer, len(self.Manufacturer)) == self.Manufacturer \
          and  devh.getString(dev.iProduct, len(self.Product)) == self.Product:
            if not self.mydevh: self.mydevh = devh
            print "Device found. Bus %3d Device %3d" % (ibus, idev),
            if ndev: print ""
            else: print "*"
            ndev += 1
    if ndev >1: 
      print "Multiple devices found, using the first one. To use a different one - define device explicitly"
    if not self.mydevh:
      raise NameError, "Device not found."
#      sys.exit(1)
#mydevh = usbstub()
  
  def step(self, nsteps, m=0):
    if nsteps > 0: 
      dir = 1
    else:
      dir = 0
    nsteps = abs(nsteps)
    while nsteps > 0:
      if nsteps > 0xff:
        self._step(m, dir, 0xff)
        nsteps -= 0xff
      else:
        self._step(m, dir, nsteps)
        nsteps=0

  def _step(self, m, dir, nsteps):
    # wait for previous motion end
    stps_left = 0xff
    while stps_left > 0:
      time.sleep(0.01)
      stps_left = self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 1, 1, value=(m<<9))[0]
    # issue the command
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 0, 0, value = (m<<9)|(dir<<8)|(nsteps))
    # wait for the motion completion
    stps_left = 0xff
    while stps_left > 0:
      time.sleep(0.01)
      stps_left = self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 1, 1, value=(m<<9))[0]

  def set_del(self, d, m=0):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 2, 0, value = d)

  def stop(self, m=0):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 3, 0, value=(m<<9))

  def pwr_down(self, m=0):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 4, 0, value=(m<<9))

  def pwr_up(self):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 5, 0)

  def getsens(self, i):
    return self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 0x80, 1, value=(i<<9))[0]

if __name__ == "__main__":
  from optparse import OptionParser
  parser = OptionParser()
  parser.add_option('-s', dest='step', type='int', help='Step N steps. Use quotation for negative values.', metavar='N')
  parser.add_option('-m', dest='mot', type='int', help='Motor identifier. Motors are numbered starting from 0. Default is 0', metavar='ID')
  parser.add_option('-d', dest='dly', type='int', help='Set time per step to D ms.', metavar='D')
  parser.add_option('-p', dest='pwr', type='str', help='Power up/down a motor. Valid arguments are `on` or `off`.', metavar='on|off')
  parser.add_option('-t', dest='stop', action="store_true", help='Stop a motor immediately.')
  parser.add_option('-q', dest='qsns', action="store_true", help='Query sensor value. Specify sensor index using `-m` option')
  parser.set_defaults(mot=0)
  (opts, args) = parser.parse_args()

  stpr = usbstpr()
  if opts.stop:
    stpr.stop()
    sys.exit(0)

  if opts.qsns:
    print "Sensor #%d: %d" % (opts.mot, stpr.getsens(opts.mot))
    sys.exit()

  if opts.dly: stpr.set_del(opts.dly)
  if opts.step: stpr.step(opts.step, opts.mot)
  if opts.pwr == "off":
    stpr.pwr_down()
  elif opts.pwr == "on":
    stpr.pwr_up()

  print "Done"
