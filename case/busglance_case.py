#!/usr/bin/env python3
"""Parametric BusGlance case - rounded Bauhaus, M2.5x6 BHCS assembly. Units = mm."""
import numpy as np, trimesh
from trimesh.creation import box, cylinder, icosphere

# ---------------- PARAMETERS ----------------
MOD        = 90.0
WIN_W, WIN_H = 87.0, 65.0
WIN_OFF_TOP  = 13.0
WIN_R      = 6.0
MODULE_T   = 1.6      # display PCB thickness (module locates in this front pocket)
SCREW_INSET= 5.0      # module corner hole centres, inset from each edge
WALL       = 2.6
CLEAR      = 0.5
DEPTH      = 20.0
BEZEL      = 2.6
BACKT      = 2.6
BODY_R     = 6.0
USB_W, USB_H = 12.0, 6.5
USB_OFF_X  = -18.0

# --- M2.5 x 6 BHCS machine screw fit (Bambu Lab) ---
BOSS_D  = 5.4        # boss outer dia (holds module + takes the screw)
PIP_D   = 2.6        # locating pip into the module's 3.0mm hole -> 0.4mm slide clearance
PIP_TIP = 2.0        # tapered tip for easy lead-in
TAP_D   = 2.1        # thread-forming pilot for M2.5 into plastic
TAP_DEP = 5.0        # screw thread engagement depth
CLR_D   = 2.8        # M2.5 shank clearance (back cover)
HEAD_D  = 5.3        # button-head counterbore dia
HEAD_H  = 1.7        # button-head height (sits flush on the wall face)
ENG = 'manifold'

INNER = MOD + 2*CLEAR
OUTER = INNER + 2*WALL
CX = CY = MOD/2
FS_DEPTH = BEZEL + DEPTH
USB_CX = CX + USB_OFF_X
Y_BOT  = CY - OUTER/2
holes = [(SCREW_INSET,SCREW_INSET),(MOD-SCREW_INSET,SCREW_INSET),
         (SCREW_INSET,MOD-SCREW_INSET),(MOD-SCREW_INSET,MOD-SCREW_INSET)]

def bx(sx,sy,sz, cx,cy,cz):
    m=box(extents=(sx,sy,sz)); m.apply_translation((cx,cy,cz)); return m
def cyl(r,h,cx,cy,cz):
    m=cylinder(radius=r,height=h,sections=48); m.apply_translation((cx,cy,cz)); return m
def frustum(rb,rt,h,cx,cy,cz):
    """Truncated cone: base radius rb (bottom), tip radius rt (top). Tapered lead-in."""
    b=cylinder(radius=rb,height=0.02,sections=48); b.apply_translation((0,0,-h/2))
    t=cylinder(radius=rt,height=0.02,sections=48); t.apply_translation((0,0, h/2))
    f=trimesh.util.concatenate([b,t]).convex_hull; f.apply_translation((cx,cy,cz)); return f
def rbox(sx,sy,sz,r,cx,cy,cz):
    r=min(r,sx/2-0.1,sy/2-0.1,sz/2-0.1); parts=[]
    for dx in(-1,1):
        for dy in(-1,1):
            for dz in(-1,1):
                s=icosphere(subdivisions=3,radius=r); s.apply_translation((dx*(sx/2-r),dy*(sy/2-r),dz*(sz/2-r))); parts.append(s)
    h=trimesh.util.concatenate(parts).convex_hull; h.apply_translation((cx,cy,cz)); return h
def rrect(w,h,d,r,cx,cy,cz):
    r=min(r,w/2-0.1,h/2-0.1); parts=[]
    for dx in(-1,1):
        for dy in(-1,1):
            c=cylinder(radius=r,height=d,sections=48); c.apply_translation((dx*(w/2-r),dy*(h/2-r),0)); parts.append(c)
    hh=trimesh.util.concatenate(parts).convex_hull; hh.apply_translation((cx,cy,cz)); return hh

# ---------------- FRONT SHELL (tray + window + bosses) ----------------
shell = rbox(OUTER,OUTER,FS_DEPTH, BODY_R, CX,CY, FS_DEPTH/2)
shell = shell.difference(bx(INNER,INNER,DEPTH+2, CX,CY, BEZEL+(DEPTH+2)/2), engine=ENG)   # cavity, open back
win_cy = MOD - WIN_OFF_TOP - WIN_H/2
shell = shell.difference(rrect(WIN_W,WIN_H, BEZEL+3, WIN_R, MOD/2,win_cy, BEZEL/2), engine=ENG)  # rounded window
usb_cz = FS_DEPTH - USB_H/2 - 3
shell = shell.difference(bx(USB_W, WALL+4, USB_H, USB_CX, Y_BOT+WALL/2, usb_cz), engine=ENG)      # snug USB-C
# 4 bosses: locating pip (into module hole) + tube behind, screwed from the back cover
for (hx,hy) in holes:
    pip  = frustum(PIP_D/2, PIP_TIP/2, MODULE_T, hx,hy, BEZEL+MODULE_T/2)
    tube = cyl(BOSS_D/2, DEPTH-MODULE_T,   hx,hy, BEZEL+MODULE_T+(DEPTH-MODULE_T)/2)
    shell = shell.union(pip, engine=ENG).union(tube, engine=ENG)
for (hx,hy) in holes:
    shell = shell.difference(cyl(TAP_D/2, TAP_DEP+1, hx,hy, FS_DEPTH-(TAP_DEP+1)/2+0.5), engine=ENG)  # thread pilot
shell.export('/Users/dickyagustiady/Projects/BusSchedule/case/front_shell.stl')

# ---------------- BACK COVER (plate + lip + countersunk screws + keyholes) ----------------
back = rbox(OUTER,OUTER,BACKT, BODY_R, CX,CY, -BACKT/2)
back = back.union(bx(INNER-0.6, INNER-0.6, 3.0, CX,CY, 1.5), engine=ENG)
for (hx,hy) in holes:                                          # M2.5 clearance + button-head counterbore
    back = back.difference(cyl(CLR_D/2, BACKT+3+2, hx,hy, (-BACKT+3)/2), engine=ENG)
    back = back.difference(cyl(HEAD_D/2, HEAD_H+0.2, hx,hy, -BACKT+HEAD_H/2-0.05), engine=ENG)
for kx in (CX-28, CX+28):                                      # wall-mount keyholes
    ky = MOD - 14
    back = back.difference(cyl(4.5, BACKT+2, kx,ky, -BACKT/2), engine=ENG)
    back = back.difference(bx(4.0, 11.0, BACKT+2, kx, ky+5.5, -BACKT/2), engine=ENG)
back.export('/Users/dickyagustiady/Projects/BusSchedule/case/back_cover.stl')

# ---------------- RENDER ----------------
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
def shade(m,base):
    L=np.array([0.3,0.5,0.85]); L/=np.linalg.norm(L); it=0.45+0.55*np.clip(m.face_normals@L,0,1)
    return np.clip(np.array(base)[None,:]*it[:,None],0,1)
def draw(ax,m,base,dz=0):
    v=m.vertices.copy(); v[:,2]+=dz
    ax.add_collection3d(Poly3DCollection(v[m.faces],facecolors=shade(m,base),edgecolors=(0,0,0,0.05),linewidths=0.1))
fig=plt.figure(figsize=(13,4.6))
ax=fig.add_subplot(1,3,1,projection='3d'); draw(ax,shell,(0.20,0.42,0.70))
ax.set_box_aspect((1,1,0.55)); ax.view_init(elev=-88,azim=-90); ax.set_axis_off(); ax.set_xlim(0,MOD); ax.set_ylim(0,MOD); ax.set_zlim(-6,FS_DEPTH+22); ax.set_title('front (rounded screen)',fontsize=10)
ax=fig.add_subplot(1,3,2,projection='3d'); draw(ax,shell,(0.20,0.42,0.70)); draw(ax,back,(0.75,0.35,0.30),dz=FS_DEPTH-BACKT+3)
ax.set_box_aspect((1,1,0.55)); ax.view_init(elev=22,azim=-60); ax.set_axis_off(); ax.set_xlim(0,MOD); ax.set_ylim(0,MOD); ax.set_zlim(-6,FS_DEPTH+22); ax.set_title('assembled',fontsize=10)
ax=fig.add_subplot(1,3,3,projection='3d'); draw(ax,back,(0.75,0.35,0.30))
ax.set_box_aspect((1,1,0.5)); ax.view_init(elev=-88,azim=-90); ax.set_axis_off(); ax.set_xlim(0,MOD); ax.set_ylim(0,MOD); ax.set_zlim(-BACKT-3,5); ax.set_title('back (keyholes + screw c-bores)',fontsize=10)
plt.tight_layout(); plt.savefig('/Users/dickyagustiady/Projects/BusSchedule/case/preview.png',dpi=115,bbox_inches='tight')
print("front_shell watertight:",shell.is_watertight," back_cover watertight:",back.is_watertight)
print("assembly: 4x M2.5 x 6 BHCS into corner bosses | outer",round(OUTER,1),"x",round(OUTER,1),"x",round(FS_DEPTH,1),"mm")
