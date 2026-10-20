#!/usr/bin/env python3
"""Run explicitly on muOS while ONLY Nightfall owns the display; inject its mapped evdev controls."""
import os,struct,time,subprocess,json
from pathlib import Path
root=Path('/mnt/mmc/ports/nightfall')
fd=os.open('/dev/input/event1',os.O_WRONLY|os.O_NONBLOCK)
def event(kind,code,value):
 now=time.time();stamp=(int(now),int((now%1)*1000000))
 os.write(fd,struct.pack('llHHi',*stamp,kind,code,value))
 os.write(fd,struct.pack('llHHi',*stamp,0,0,0))
def button(code):
 event(1,code,1);time.sleep(.09);event(1,code,0);time.sleep(.22)
def axis(code,value,duration=.1):
 end=time.monotonic()+duration
 while time.monotonic()<end:
  event(3,code,value);time.sleep(.003)
 event(3,code,0);time.sleep(.2)
def down(n=1):
 for _ in range(n):axis(17,1)
def up(n=1):
 for _ in range(n):axis(17,-1)
def capture(name):
 subprocess.run(['fbgrab',str(root/name)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
try:
 def state():
  return json.loads((root/'hardware-live.json').read_text())
 def expect(view):
  deadline=time.monotonic()+2
  while time.monotonic()<deadline:
   d=state()
   if d['view']==view:return d
   time.sleep(.1)
  raise AssertionError(('expected view',view,d))
 expect(0)
 time.sleep(.5)
 capture('hardware-font-menu.png')
 down(3);button(304);expect(4);capture('hardware-help.png');button(305);expect(0)
 up(3);button(304);before=expect(1)
 axis(2,3000,1);after_stick=state()
 axis(16,-1,1);after_dpad=state()
 stick=((after_stick['x']-before['x'])**2+(after_stick['y']-before['y'])**2)**.5
 dpad=((after_dpad['x']-after_stick['x'])**2+(after_dpad['y']-after_stick['y'])**2)**.5
 assert stick>15,('stick movement',stick,before,after_stick)
 assert dpad>15,('dpad movement',dpad,after_stick,after_dpad)
 (root/'hardware-input-assertions.json').write_text(json.dumps({'stick_distance':stick,'dpad_distance':dpad,'stick':after_stick,'dpad':after_dpad},indent=2)+'\n')
 button(311);expect(3);capture('hardware-pause.png');button(305);expect(1)
 time.sleep(.5)
 button(311);expect(3);down();button(304);expect(0)
 down(2);button(304);expect(2);axis(17,1);capture('hardware-join.png');button(305);expect(0)
 down(2);button(304);button(304) # sound off then on
 down();button(304) # quit
 print('Injected: help/back, solo, stick, D-pad, pause/resume, leave, IP entry, sound toggle, quit')
finally:
 for code in (2,3,16,17):event(3,code,0)
 for code in (304,305,311):event(1,code,0)
 os.close(fd)
