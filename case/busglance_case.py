#!/usr/bin/env python3
"""Parametric BusGlance case - FLAT front/back, rounded vertical edges, 2-board stack.
Display module screwed to the front; ESP32 cradled on the back plate. Units = mm."""
import numpy as np, trimesh
from trimesh.creation import box, cylinder

# ================= MEASURED PARAMETERS =================
# Display module (the screen PCB) - measured off the ruler photos
MOD_W, MOD_H = 91.0, 89.0     # long edge x short edge
MODULE_T     = 2.0            # PCB thickness
HOLE_D       = 3.0            # corner mounting-hole diameter
# corner holes: 86mm apart (along 91) and 84mm apart (along 89) => 2.5mm inset
HX = [2.5, MOD_W - 2.5]       # -> 2.5, 88.5
HY = [2.5, MOD_H - 2.5]       # -> 2.5, 86.5
holes = [(x, y) for x in HX for y in HY]

# Case shell
CLEAR  = 1.25                 # per-side gap so the board actually DROPS IN
WALL   = 2.6
BEZEL  = 4.0                  # flat front thickness - thick enough to take the screw pilots
BACKT  = 2.6                  # back plate thickness
DEPTH  = 28.0                 # cavity depth: 25mm display+wire stack + a little margin
BODY_R = 6.0                  # rounded vertical-edge radius (front/back stay FLAT)

# Window (matches the active glass ~84.8x63.6, offset UP on the board)
WIN_W, WIN_H = 86.0, 66.0
WIN_OFF_TOP  = 9.0            # glass sits ~9mm below the top (header) edge
WIN_R = 6.0

# Display screws straight into the thick flat front (short M2.5). No poles.
PILOT_D    = 2.1            # M2.5 self-tap pilot bored into the front
PILOT_DEP  = 3.0           # thread depth into the front (bezel is 4mm, leaves 1mm skin)

# Snap-fit back cover
LIP_H      = 6.0           # lip depth that inserts into the cavity
LIP_CLR    = 0.3           # slip clearance per side
SNAP_BUMP  = 0.4           # how far the lip's ridge sticks past the wall (interference)
SNAP_GROOVE= 0.8           # groove depth cut into the wall (the ridge clicks into this)

# ESP32 cradle + screws on the back plate. 50 x 25 envelope (incl. USB-C overhang).
# USB-C exits BOTTOM-LEFT. Screwed down through its 2 measured mounting holes.
ESP_L, ESP_W = 50.0, 25.0
ESP_CLR   = 0.6            # slip fit in the cradle
CR_WALL   = 2.0           # cradle wall thickness
CR_H      = 6.0           # cradle wall height
ESP_FLOOR = 2.0           # raised cradle floor: full board support + M2 thread depth
ESP_PILOT_D = 1.5         # M2 self-tap pilot
ESP_X0, ESP_Y0 = 17.5, -1.25   # placement: board's bottom-left (USB corner) in case coords
# measured holes from the board's bottom-left corner: (2, 9) & (23, 47);
# +1mm on Y for the USB-C overhang the 50mm envelope includes.
ESP_HOLES = [(ESP_X0 + 2, ESP_Y0 + 10), (ESP_X0 + 23, ESP_Y0 + 48)]
USB_W, USB_H = 16.0, 8.0  # USB-C slot (wide to tolerate the port offset)
USB_CX    = 24.0          # USB-C centre X - bottom-LEFT (~7mm from the board's left edge)

ENG = 'manifold'

# ---- derived ----
INNER_W = MOD_W + 2*CLEAR
INNER_H = MOD_H + 2*CLEAR
OUTER_W = INNER_W + 2*WALL
OUTER_H = INNER_H + 2*WALL
CX, CY  = MOD_W/2, MOD_H/2
FS_DEPTH = BEZEL + DEPTH
Y_BOT   = CY - OUTER_H/2         # outer bottom edge (USB-C exits here)
WIN_CY  = MOD_H - WIN_OFF_TOP - WIN_H/2

def bx(sx,sy,sz, cx,cy,cz):
    m=box(extents=(sx,sy,sz)); m.apply_translation((cx,cy,cz)); return m
def cyl(r,h,cx,cy,cz):
    m=cylinder(radius=r,height=h,sections=48); m.apply_translation((cx,cy,cz)); return m
def rrect(w,h,d,r,cx,cy,cz):
    r=min(r,w/2-0.1,h/2-0.1); parts=[]
    for dx in(-1,1):
        for dy in(-1,1):
            c=cylinder(radius=r,height=d,sections=48); c.apply_translation((dx*(w/2-r),dy*(h/2-r),0)); parts.append(c)
    hh=trimesh.util.concatenate(parts).convex_hull; hh.apply_translation((cx,cy,cz)); return hh
def frustum(rb,rt,h,cx,cy,cz):
    b=cylinder(radius=rb,height=0.02,sections=48); b.apply_translation((0,0,-h/2))
    t=cylinder(radius=rt,height=0.02,sections=48); t.apply_translation((0,0, h/2))
    f=trimesh.util.concatenate([b,t]).convex_hull; f.apply_translation((cx,cy,cz)); return f

# ================= FRONT SHELL =================
# Flat front + rounded vertical edges (rrect), hollow cavity open at the back.
shell = rrect(OUTER_W, OUTER_H, FS_DEPTH, BODY_R, CX, CY, FS_DEPTH/2)
shell = shell.difference(bx(INNER_W, INNER_H, DEPTH+2, CX, CY, BEZEL+(DEPTH+2)/2), engine=ENG)
# rounded window through the flat front
shell = shell.difference(rrect(WIN_W, WIN_H, BEZEL+3, WIN_R, CX, WIN_CY, BEZEL/2), engine=ENG)
# USB-C slot through the bottom wall (bottom-LEFT, aligned to USB_CX)
shell = shell.difference(bx(USB_W, WALL+4, USB_H, USB_CX, Y_BOT+WALL/2, BEZEL+MODULE_T+USB_H/2+2), engine=ENG)
# Display screw pilots: bored straight into the THICK flat front. No protruding bosses -
# the display just rests on the flat inner ledge and 4 screws pull it tight to the front.
for (hx,hy) in holes:
    shell = shell.difference(cyl(PILOT_D/2, PILOT_DEP, hx, hy, BEZEL - PILOT_DEP/2), engine=ENG)
# Snap-fit groove around the inner wall - the back cover's lip ridge clicks in here.
SNAP_Z = FS_DEPTH - (LIP_H - 1.0 + BACKT)   # aligns with the lip ridge when seated
shell = shell.difference(bx(INNER_W+2*SNAP_GROOVE, INNER_H+2*SNAP_GROOVE, 1.4, CX, CY, SNAP_Z), engine=ENG)
shell.export('/Users/dickyagustiady/Projects/BusSchedule/case/front_shell.stl')

# ================= BACK PLATE =================
# Flat back + rounded vertical edges, inner lip to locate in the shell.
back = rrect(OUTER_W, OUTER_H, BACKT, BODY_R, CX, CY, -BACKT/2)
# Snap lip that inserts into the cavity + an outward ridge near its tip that clicks in.
back = back.union(bx(INNER_W-2*LIP_CLR, INNER_H-2*LIP_CLR, LIP_H, CX, CY, LIP_H/2), engine=ENG)
back = back.union(rrect(INNER_W-2*LIP_CLR+2*SNAP_BUMP, INNER_H-2*LIP_CLR+2*SNAP_BUMP, 1.0,
                        BODY_R, CX, CY, LIP_H-1.0), engine=ENG)   # snap ridge

# ESP32 cradle: raised-floor pocket (long axis along Y), USB-C end open toward the bottom wall.
ecx, ecy = ESP_X0 + ESP_W/2, ESP_Y0 + ESP_L/2
ow, oh = ESP_L + 2*CR_WALL + ESP_CLR, ESP_W + 2*CR_WALL + ESP_CLR
cradle = bx(oh, ow, CR_H, ecx, ecy, CR_H/2)                                                   # solid block
cradle = cradle.difference(bx(ESP_W+ESP_CLR, ESP_L+ESP_CLR, CR_H, ecx, ecy, ESP_FLOOR+CR_H/2), engine=ENG)  # pocket above the 2mm floor
cradle = cradle.difference(bx(USB_W, CR_WALL+2, CR_H, ecx, ecy-ow/2, ESP_FLOOR+CR_H/2), engine=ENG)          # open USB end
back = back.union(cradle, engine=ENG)
# 2 screw pilots (M2) through the raised floor into the back plate, at the measured holes
for (hx,hy) in ESP_HOLES:
    back = back.difference(cyl(ESP_PILOT_D/2, ESP_FLOOR+2.0, hx, hy, 0.0), engine=ENG)

# wall-mount keyholes (unchanged idea)
for kx in (CX-28, CX+28):
    ky = MOD_H - 12
    back = back.difference(cyl(4.5, BACKT+2, kx,ky, -BACKT/2), engine=ENG)
    back = back.difference(bx(4.0, 11.0, BACKT+2, kx, ky+5.5, -BACKT/2), engine=ENG)
back.export('/Users/dickyagustiady/Projects/BusSchedule/case/back_cover.stl')

# ================= RENDER =================
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
ax.set_box_aspect((1,1,0.55)); ax.view_init(elev=-88,azim=-90); ax.set_axis_off(); ax.set_xlim(0,MOD_W); ax.set_ylim(0,MOD_H); ax.set_zlim(-6,FS_DEPTH+22); ax.set_title('front (flat, rounded edges)',fontsize=10)
ax=fig.add_subplot(1,3,2,projection='3d'); draw(ax,back,(0.75,0.35,0.30))
ax.set_box_aspect((1,1,0.55)); ax.view_init(elev=24,azim=-50); ax.set_axis_off(); ax.set_xlim(0,MOD_W); ax.set_ylim(0,MOD_H); ax.set_zlim(-BACKT-3,CR_H+22); ax.set_title('inside: snap lip + ESP32 cradle (filament box)',fontsize=10)
ax=fig.add_subplot(1,3,3,projection='3d'); draw(ax,back,(0.75,0.35,0.30))
ax.set_box_aspect((1,1,0.5)); ax.view_init(elev=88,azim=-90); ax.set_axis_off(); ax.set_xlim(0,MOD_W); ax.set_ylim(0,MOD_H); ax.set_zlim(-BACKT-3,CR_H+5); ax.set_title('outside: flat back + keyholes',fontsize=10)
plt.tight_layout(); plt.savefig('/Users/dickyagustiady/Projects/BusSchedule/case/preview.png',dpi=115,bbox_inches='tight')
print("front watertight:",shell.is_watertight," back watertight:",back.is_watertight)
print("outer:",round(OUTER_W,1),"x",round(OUTER_H,1),"x",round(FS_DEPTH,1),"mm | cavity",round(INNER_W,1),"x",round(INNER_H,1))
print("display holes:",holes)
