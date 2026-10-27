import socket,subprocess,time,os,sys
from PIL import Image
sock='/tmp/qmon.sock'
if os.path.exists(sock): os.remove(sock)
q=subprocess.Popen(['qemu-system-i386','-cdrom','petal-os.iso','-m','256','-display','none','-monitor','unix:%s,server,nowait'%sock,'-audiodev','none,id=a','-device','sb16,audiodev=a'])
time.sleep(1.5)
s=socket.socket(socket.AF_UNIX);s.connect(sock);s.settimeout(0.3)
def cmd(c,w=0.15):
    s.send((c+'\n').encode());time.sleep(w)
    try: s.recv(65536)
    except: pass
def shot(n):
    cmd('screendump /tmp/s.ppm',1.0);Image.open('/tmp/s.ppm').save('shots/%s.png'%n);print(n)
def key(k,w=0.3): cmd('sendkey '+k,w)
def mouse(dx,dy): 
    while dx or dy:
        sx=max(-100,min(100,dx));sy=max(-100,min(100,dy));cmd('mouse_move %d %d'%(sx,sy),0.05);dx-=sx;dy-=sy
time.sleep(int(sys.argv[1]) if len(sys.argv)>1 else 12)
shot('01-desktop')
for i,(k,n) in enumerate([('f1','02-files'),('f2','03-notes'),('f3','04-calculator'),('f4','05-calendar'),('f5','06-media-player'),('f7','08-browser'),('f8','09-settings'),('f10','11-noughts-crosses')]):
    key(k,0.6)
    if n=='04-calculator':
        for c in ['1','2','asterisk','3','equal']: key(c)
    if n=='06-media-player': key('n',0.4)
    shot(n); key('esc',0.4)
key('f9',0.5);key('spc',0.3);time.sleep(0.5);key('down',0.2);time.sleep(0.4);key('right',0.2);time.sleep(0.3);shot('10-snake');key('esc',0.4)
key('f6',0.6)
mouse(-150,-60)
cmd('mouse_button 1',0.1)
for i in range(30): cmd('mouse_move 8 %d'%((i%10)-5),0.03)
cmd('mouse_button 0',0.2);shot('07-paint');key('esc',0.4)
key('f8',0.5);key('l',0.4);key('esc',0.3);key('f4',0.5);key('f1',0.5);shot('12-japanese');key('esc');key('esc')
key('f12',0.5);shot('13-menu')
q.kill()
