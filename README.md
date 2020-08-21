Arduino Nano baed programmer for 28C256 EEPROM.
Writen by Jason Figge, 2020

      /=== Data>   /============= Address ===========\
     //////       G////A      H////A      A0\\\\\\\\A14
 +----------+   +-------+   +-------+   +---------------+
 |  Nano    |   | sr-hi |   | sr-lo |   | 28C256 EEPROM |
 +----------+   +-------+   +-------+   +---------------+
   \   \   \\     //   \---<---/ //        /////   /   /
    WE> OE> \\---//--------->---//   <Data====/ <OE <WE


Wiring:  
 low word shift-register (sr-lo):
   sr-lo.SER => Nano.A3
   sr-lo.clockPin => Nano.A4
   sr-lo.latchPin =>Nano. A5
   sr-lo.Qa-Qh => EEPROM.A0-A7
 
 high word shift-register (sr-hi):
   sr-hi.SER => lo.QH' (overflow)
   sr-hi.clockPin => Nano.A4
   sr-hi.latchPin => Nano.A2
   sr-hi.Qa-Qg => EEPROM.A8-A14
   sr-hi.Qh => NC
  
 Nano.11 => EEPROM.OE
 Nano.12 => EEPROM.WE
 Nano.2-9  => EEPROM.I/O0-7
 
 Warning: Changing pin assignments with require significant
 reworking of all port logic.

This code was constructed after watching Ben Eater's videos
on how to build a 6502 computer, not wanting to pay $57 for
a programmer, and because I received an SDP enabled chip from
JameCo.com (is it authentic?).

After writing a program using standard arduino high level 
functions I was able to read from the chip, but due to the SDP
nothing I did would allow me to write any data.  Even after
trying the unlock sequence. After connecting my o-scope I 
found the time to write a single byte far exceeded the max
time allowed to perform a page.  Summizing the lock sequence
had to be completed within a page, I set about using registry
manipulation to speed up the process.  Now byte writes are
in the micro-second range and I can unlock the SDP
