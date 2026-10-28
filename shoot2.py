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
time.sleep(12)
shot('20-icons-desktop')
key('f1',0.6)
for i in range(6): key('down',0.2)
key('ret',0.8);shot('21-png-viewer');key('esc',0.4);key('esc',0.4)
key('f8',0.6);shot('22-settings-clock-before')
mouse(108,-44);cmd('mouse_button 1',0.2);cmd('mouse_button 0',0.6);shot('23-settings-japan-time')
mouse(142,176);cmd('mouse_button 1',0.2)
for i in range(12): cmd('mouse_move 10 8',0.05)
cmd('mouse_button 0',0.6);shot('24-window-resized')
q.kill()
