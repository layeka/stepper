#!/usr/bin/env python
"""
DESCRIPTION:
  Interface to stepper motor driver board v.0.1b.

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
    ## check firmware version
    ver=1
    ver_ok=False
    version = self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 0xff, 1, value = ver)
    if version:
      if version[0]==0:
        ver_ok=True
    if not ver_ok: raise NameError, "Incompatible firmware version!"
#      sys.exit(1)
#mydevh = usbstub()
  
  def step(self, nsteps, m=0):
    steps_done=0
    while nsteps:
      if nsteps > 0x7fff:
        self._step(m, 0x7fff)
        nsteps -= 0x7fff
      elif nsteps < -0x8000:
        self._step(m, -0x8000)
        nsteps += 0x8000
      else:
        self._step(m, nsteps)
        nsteps=0
      ## report how many steps were actually stepped
      s=self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 6, 2, index=m)
      steps_done+=s[0]+(s[1]<<8)
    return steps_done

  def wait(self, m):
    stps_left = 1
    while stps_left:
      time.sleep(0.1)
      s = self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 1, 2, index=m)
      stps_left=s[0]+(s[1]<<8)

  def _step(self, m, nsteps):
    ## wait for previous motion end
    self.wait(m)
    ## issue the command
#    print "_step(%d)"%nsteps
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 0, 0, value=nsteps, index=m)
    # wait for the motion completion
    self.wait(m)

  def set_del(self, d, m=0):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 2, 0, value = d)

  def stop(self, m=0):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 3, 0, index=m)

  def pwr_down(self, m=0):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 4, 0, index=m)

  def pwr_up(self, m=0):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 5, 0, index=m)

  def getsens(self, i):
    return self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 0x80, 1, index=i)[0]

  def bind_sens(self, m, s):
    self.mydevh.controlMsg(usb.TYPE_VENDOR | usb.RECIP_DEVICE | usb.ENDPOINT_IN, 0x81, 0, index=m, value=s)

if __name__ == "__main__":
  from optparse import OptionParser
  parser = OptionParser()
  parser.add_option('-s', dest='step', type='int', help='Step N steps. Use quotation for negative values. Program return value contains the number of steps actually taken.', metavar='N')
  parser.add_option('-m', dest='mot', type='int', help='Motor identifier. Motors are numbered starting from 0. Default is 0', metavar='ID')
  parser.add_option('-d', dest='dly', type='int', help='Set time per step to D ms.', metavar='D')
  parser.add_option('-p', dest='pwr', type='str', help='Power up/down a motor. Valid arguments are `on` or `off`.', metavar='on|off')
  parser.add_option('-t', dest='stop', action="store_true", help='Stop a motor immediately.')
  parser.add_option('-q', dest='qsns', action="store_true", help='Query sensor value. Specify sensor index using `-m` option')
  parser.add_option('-b', dest='bind', type="str", help='Bind sensor S to motor M', metavar='S@M')
  parser.set_defaults(mot=0)
  (opts, args) = parser.parse_args()

  stpr = usbstpr()
  if opts.stop:
    stpr.stop()
  elif opts.qsns:
    sval = stpr.getsens(opts.mot)
    print "Sensor #%d: %d" % (opts.mot, sval)
  elif opts.dly:
    stpr.set_del(opts.dly)
  elif opts.step:
    n=stpr.step(opts.step, opts.mot)
  elif opts.pwr == "off":
    stpr.pwr_down(opts.mot)
  elif opts.pwr == "on":
    stpr.pwr_up(opts.mot)
  elif opts.bind:
    isns,imot=opts.bind.split('@')
    stpr.bind_sens(int(imot), int(isns))
    print "Sensor #%s assigned to motor #%s"%(isns, imot)

  print "Done"
