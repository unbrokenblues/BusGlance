#!/usr/bin/env python3
"""BusGlance case - CadQuery build (clean CSG style, filleted edges, STEP+STL out).
Board data is EXACT from WeAct-EpaperModule-4.2 Board 3D.step.

Coordinate frame: board CENTRED at origin. +X right, +Y up (toward header/top).
Front (viewing) face at Z=0; case extends into +Z; back opening at Z=FS_DEPTH."""
import cadquery as cq

# ============ EXACT board data (from the WeAct 4.2 STEP) ============
BOARD_W, BOARD_H = 91.8, 89.8      # width (header edge) x height
HOLE_INSET = 2.8                   # corner-hole centre inset from each edge
HX = BOARD_W/2 - HOLE_INSET        # 43.1
HY = BOARD_H/2 - HOLE_INSET        # 42.1
HOLES = [(sx*HX, sy*HY) for sx in (-1,1) for sy in (-1,1)]   # centred

# ============ Case params ============
CLEAR  = 1.0
WALL   = 2.6
BEZEL  = 4.0
BACKT  = 2.6
DEPTH  = 28.0
EDGE_R = 6.0
WIN_W, WIN_H = 86.0, 66.0
WIN_OFF_TOP  = 7.5
WIN_R = 5.0
PILOT_D   = 2.1
PILOT_DEP = 3.0
LIP_H      = 6.0
LIP_CLR    = 0.3
SNAP_BUMP  = 0.4
SNAP_GROOVE= 0.8
# ESP32 (measured): 50x25 envelope, USB-C bottom-left, 2 M2 holes
ESP_L, ESP_W = 50.0, 25.0
ESP_CLR   = 0.6
CR_WALL   = 2.0
CR_H      = 6.0
ESP_FLOOR = 2.0
ESP_PILOT_D = 1.5
USB_W, USB_H = 16.0, 8.0

INNER_W = BOARD_W + 2*CLEAR
INNER_H = BOARD_H + 2*CLEAR
OUTER_W = INNER_W + 2*WALL
OUTER_H = INNER_H + 2*WALL
FS_DEPTH = BEZEL + DEPTH
WIN_CY = BOARD_H/2 - WIN_OFF_TOP - WIN_H/2

# ESP32 placement in CENTRED coords (USB-C toward -Y bottom, on the left)
ESP_CX, ESP_CY = -16.0, -INNER_H/2 + ESP_L/2 + 1.0
# measured holes from the board's bottom-left corner (2,9)&(23,47) + 1mm Y overhang,
# mapped into the ESP envelope then into centred case coords:
_ex0, _ey0 = ESP_CX - ESP_W/2, ESP_CY - ESP_L/2          # envelope bottom-left
ESP_HOLES = [(_ex0 + 2, _ey0 + 10), (_ex0 + 23, _ey0 + 48)]
USB_CX = _ex0 + 6.5                                       # USB-C centre ~6.5mm from left edge
# USB-C sits at the ESP32's depth (near the BACK), not near the front:
USB_CZ = FS_DEPTH - ESP_FLOOR - 1.0

# ---------- helpers ----------
def rbox(w,h,d, cx,cy,cz, r=None):
    o = cq.Workplane("XY").box(w,h,d)
    if r: o = o.edges("|Z").fillet(r)
    return o.translate((cx,cy,cz))
def rcyl(rad,h, cx,cy,cz):
    return cq.Workplane("XY").cylinder(h,rad).translate((cx,cy,cz))

def report(name, wp):
    bb = wp.val().BoundingBox()
    print(f"  {name}: X {bb.xmin:6.1f}..{bb.xmax:5.1f}  Y {bb.ymin:6.1f}..{bb.ymax:5.1f}  Z {bb.zmin:5.1f}..{bb.zmax:5.1f}")

# ================= FRONT SHELL =================
front = rbox(OUTER_W, OUTER_H, FS_DEPTH, 0,0, FS_DEPTH/2, r=EDGE_R)
front = front.cut(rbox(INNER_W, INNER_H, DEPTH+2, 0,0, BEZEL+(DEPTH+2)/2))          # cavity
front = front.cut(rbox(WIN_W, WIN_H, BEZEL*2+2, 0, WIN_CY, BEZEL/2, r=WIN_R))       # rounded window
for (hx,hy) in HOLES:                                                                # display pilots
    front = front.cut(rcyl(PILOT_D/2, PILOT_DEP, hx, hy, BEZEL - PILOT_DEP/2))
front = front.cut(rbox(USB_W, WALL+4, USB_H, USB_CX, -INNER_H/2 - WALL/2, USB_CZ))  # USB slot (at back)
snap_z = FS_DEPTH - (LIP_H - 1.0 + BACKT)                                            # snap groove
front = front.cut(rbox(INNER_W+2*SNAP_GROOVE, INNER_H+2*SNAP_GROOVE, 1.4, 0,0, snap_z))
report("FRONT", front)
cq.exporters.export(front, "front_shell.stl")
cq.exporters.export(front, "front_shell.step")

# ================= BACK COVER ================= (own frame: plate Z -BACKT..0, features +Z)
back = rbox(OUTER_W, OUTER_H, BACKT, 0,0, -BACKT/2, r=EDGE_R)
back = back.union(rbox(INNER_W-2*LIP_CLR, INNER_H-2*LIP_CLR, LIP_H, 0,0, LIP_H/2))               # lip
back = back.union(rbox(INNER_W-2*LIP_CLR+2*SNAP_BUMP, INNER_H-2*LIP_CLR+2*SNAP_BUMP, 1.0, 0,0, LIP_H-1.0, r=EDGE_R))  # ridge
cr_ow, cr_oh = ESP_W+2*CR_WALL+ESP_CLR, ESP_L+2*CR_WALL+ESP_CLR
cradle = rbox(cr_ow, cr_oh, CR_H, ESP_CX, ESP_CY, CR_H/2)
cradle = cradle.cut(rbox(ESP_W+ESP_CLR, ESP_L+ESP_CLR, CR_H, ESP_CX, ESP_CY, ESP_FLOOR+CR_H/2))  # pocket above floor
cradle = cradle.cut(rbox(USB_W, CR_WALL+2, CR_H, ESP_CX, ESP_CY - cr_oh/2, ESP_FLOOR+CR_H/2))     # open USB end
back = back.union(cradle)
for (hx,hy) in ESP_HOLES:                                                                         # ESP32 pilots
    back = back.cut(rcyl(ESP_PILOT_D/2, ESP_FLOOR+2, hx, hy, 0))
for kx in (-28, 28):                                                                              # keyholes
    ky = INNER_H/2 - 12
    back = back.cut(rcyl(4.5, BACKT+2, kx, ky, -BACKT/2))
    back = back.cut(rbox(4.0, 11.0, BACKT+2, kx, ky+5.5, -BACKT/2))
report("BACK ", back)
cq.exporters.export(back, "back_cover.stl")
cq.exporters.export(back, "back_cover.step")
print("done")
