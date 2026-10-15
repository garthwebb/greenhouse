"""Outdoor housing for a remote temp/humidity sensor: a stacked-plate radiation shield on top of a
sealed pod holding the driver board: a CR123A cell, boost converter and the ESP32.

Generates STEP and STL files in out/ for each printable part, plus a full assembly for viewing.
The driver board's outline and part positions are read from its KiCad file, so rerun this after
changing the board; the assembly STEP shows the board and its tall parts in orange.
Requires CadQuery (pip install cadquery).  Run: python sensor_housing.py

Every shield plate is the same 45 degree cone, sloping down toward the outside, stacked PITCH apart.
Because each cone is taller (RISE) than the gap between them (PITCH), there is no straight
downward path from the sun to the center column, but air can flow sideways between the plates.

    cap       solid top with a PCB holder hanging into the center column (print upside down)
    plate     open-center ring, print PLATE_COUNT of them
    base      closes the bottom of the shield with a cable hole at the apex; sits on the pod
    pod       sealed can with a grommeted cable entry in the top (print upside down)
    pod_lid   O-ring plug that closes the bottom of the pod; the driver board screws to its standoffs
    grommet   split TPU seal around the sensor cable where it enters the pod (print in TPU)

M3 screws run down through the cap, plates and base into heat-set inserts in blind holes in the
pod's thick top, so nothing pierces the sealed space.  The lid is held by radial M3 screws through
the pod wall into heat-set inserts in the side of the plug, below the O-ring.
"""

import math
import re
from pathlib import Path

import cadquery as cq

#----------------------------------------------------
# Shield dimensions (mm)

R_OUT = 32.0            # outer radius of every plate; matches the pod
R_IN = 12.0             # radius of the open center column
SLOPE_DEG = 45.0        # steep enough to print without supports
WALL = 1.6              # plate thickness, measured perpendicular to the surface
PITCH = 10.0            # vertical distance between plates
PLATE_COUNT = 3         # ring plates between the base and the cap

SKIRT_W = 2.0           # drip edge around each plate's rim; also gives a flat ring to print on
SKIRT_H = 2.0

# Placed so none of the screws' inserts in the pod top land near the cable grommet
POST_R = 23.0
POST_D = 7.0
ROD_HOLE_D = 3.4        # M3
POST_ANGLES = (90.0, 210.0, 330.0)

CABLE_HOLE_D = 8.0      # through the base apex

DISC_T = 3.0            # flat center of the cap
MOUNT_HOLE_D = 4.5      # two M4 holes in the cap for screwing to a bracket
MOUNT_HOLE_OFFSET = 8.0

# Sensor PCB, hanging vertically in the center column with the sensor at the bottom end
PCB_W = 15.0
PCB_T = 1.6
PCB_LEN = 15.0          # only used to check the board clears the base apex
MIN_APEX_CLEARANCE = 6.0  # room below the board for the wires to bend toward the cable hole
RAIL_LEN = 6.0          # how far the holder rails grip the PCB edges
RAIL_W = 4.5
RAIL_T = 6.0
SLOT_DEPTH = 1.5        # how far each PCB edge sits in its rail
SLOT_CLEARANCE = 0.2

#----------------------------------------------------
# Pod dimensions (mm)

POD_WALL = 2.0
POD_TOP_T = 6.5         # thick enough for blind heat-set inserts, so nothing hangs into the pod
POD_INTERIOR_H = 24.0   # standoffs, board and battery holder, with ~1 mm to spare

# The driver board (XIAO, CR123A holder, boost converter) screws to two standoffs on the lid, so battery
# swaps pull it all out with the lid.  Its outline and part positions come from the KiCad board.  It's
# centered in the pod with the battery holder toward -Y and the XIAO and Qwiic header toward +Y.
DRIVER_PCB = Path(__file__).parent.parent / "pcb" / "sensor-driver" / "sensor-driver" / "sensor-driver.kicad_pcb"
BOARD_T = 1.6
HOLDER_REF, XIAO_REF, CONNECTOR_REF = "U2", "U1", "J2"
HOLDER_H = 18.3         # Keystone 1051 CR123A holder, above the board
XIAO_STACK_H = 17.0     # bottom of the board to the top of the XIAO's USB-C, on female + male headers
CONNECTOR_H = 4.25      # JST-SH BM04B vertical header, above the board
PLUG_ROOM = 8.0         # above the header for the Qwiic plug and the cable dropping in from the grommet

STANDOFF_H = 3.0        # clears trimmed through-hole leads under the board
STANDOFF_D = 6.0
STANDOFF_INSERT_D = 3.5  # M2.5 heat-set insert; check your inserts' recommended hole
STANDOFF_INSERT_DEPTH = 5.0

INSERT_HOLE_D = 4.0     # M3 heat-set insert, 4 mm long
INSERT_DEPTH = 5.5      # blind, measured from the top of the pod

# The sensor connects with a Qwiic / STEMMA QT cable (JST-SH plugs on both ends).  The cable entry is
# big enough for a plug to pass through, so the shield and pod come apart without desoldering, and is
# sealed by a split TPU grommet that snaps around the cable.  It sits directly over the driver board's
# Qwiic header so the cable drops straight in.  The base cone keeps rain off it; a cable gland won't fit
# under the cone.
QWIIC_PLUG = (6.0, 3.0)  # JST-SH 4-pin plug cross-section
CABLE_ENTRY_D = 7.5

# Bore for the cable itself; measure yours.  Qwiic and STEMMA QT cables are four ~1 mm wires side by side.
CABLE_W = 4.2
CABLE_T = 1.2

GROMMET_FIT = 0.1       # interference with the hole, per side; TPU squeezes in
GROMMET_FLANGE_D = 11.0
GROMMET_FLANGE_T = 1.5
GROMMET_BARB_D = 8.4    # snaps in under the inside of the pod top
GROMMET_BARB_H = 1.2
GROMMET_SLIT_W = 0.4    # cut down one side to fit it around the cable

# Lid: a plug with a radial O-ring seal
LID_FLANGE_T = 2.0
PLUG_H = 11.0
PLUG_CLEARANCE = 0.2    # per side, between plug and pod bore
ORING_CS = 2.0          # O-ring cross-section
ORING_SQUEEZE = 0.2     # fraction the O-ring is compressed
GROOVE_W = 2.6
GROOVE_FROM_TOP = 1.5 + GROOVE_W / 2   # groove center, measured down from the top of the plug
LID_SCREW_ANGLES = (0.0, 120.0, 240.0)
LID_SCREW_FROM_FLANGE = 3.5             # height of the radial screws above the flange
LID_SCREW_HOLE_D = 3.4
LID_INSERT_DEPTH = 6.0

#----------------------------------------------------
# Derived

TAN = math.tan(math.radians(SLOPE_DEG))
WALL_V = WALL / math.cos(math.radians(SLOPE_DEG))   # wall thickness measured vertically
RISE = (R_OUT - R_IN) * TAN                         # height of each cone from rim to inner edge
CAP_Z = PITCH * (PLATE_COUNT + 1)
BIG = 1000.0

POD_TOP_Z = -SKIRT_H                                # the base's skirt and legs sit on the pod
POD_BORE_R = R_OUT - POD_WALL
POD_CEILING_Z = POD_TOP_Z - POD_TOP_T
PLUG_TOP_Z = POD_CEILING_Z - POD_INTERIOR_H
POD_BOTTOM_Z = PLUG_TOP_Z - PLUG_H                  # bottom rim of the pod, where the lid flange sits
PLUG_R = POD_BORE_R - PLUG_CLEARANCE
GROOVE_BOTTOM_R = POD_BORE_R - ORING_CS * (1 - ORING_SQUEEZE)

OUT_DIR = Path(__file__).parent / "out"


#----------------------------------------------------
# Driver board, read from KiCad

def _convex_hull(points):
    points = sorted(set(points))
    def half(pts):
        hull = []
        for p in pts:
            while len(hull) >= 2 and ((hull[-1][0] - hull[-2][0]) * (p[1] - hull[-2][1]) -
                                      (hull[-1][1] - hull[-2][1]) * (p[0] - hull[-2][0])) <= 0:
                hull.pop()
            hull.append(p)
        return hull
    lower, upper = half(points), half(points[::-1])
    return lower[:-1] + upper[:-1]


def _enclosing_circle(points):
    """Smallest circle around the points, brute force over the convex hull."""
    hull = _convex_hull(points)
    candidates = []
    for i, a in enumerate(hull):
        for b in hull[i + 1:]:
            candidates.append((((a[0] + b[0]) / 2, (a[1] + b[1]) / 2), math.dist(a, b) / 2))
            for c in hull:
                d = 2 * (a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1]))
                if abs(d) < 1e-9:
                    continue
                ux = ((a[0]**2 + a[1]**2) * (b[1] - c[1]) + (b[0]**2 + b[1]**2) * (c[1] - a[1]) + (c[0]**2 + c[1]**2) * (a[1] - b[1])) / d
                uy = ((a[0]**2 + a[1]**2) * (c[0] - b[0]) + (b[0]**2 + b[1]**2) * (a[0] - c[0]) + (c[0]**2 + c[1]**2) * (b[0] - a[0])) / d
                candidates.append(((ux, uy), math.dist((ux, uy), a)))
    return min((c for c in candidates if all(math.dist(c[0], p) <= c[1] + 1e-6 for p in hull)), key=lambda c: c[1])


def load_driver_board(path):
    """Outline, part footprints and mounting holes of the driver board, in pod coordinates (y up).

    The board is centered on the smallest circle around its outline and the XIAO (whose USB-C hangs
    past the board edge), and flipped from KiCad's y-down so the battery holder ends up toward -Y.
    """
    text = path.read_text()
    edge = re.search(r'\(gr_poly\s*\(pts(.*?\))\s*\)(?:(?!\(gr_).)*?\(layer "Edge\.Cuts"\)', text, re.S)
    assert edge, f"No Edge.Cuts polygon in {path}"
    outline = [(float(x), float(y)) for x, y in re.findall(r'\(xy ([-\d.]+) ([-\d.]+)\)', edge.group(1))]

    parts, holes = {}, {}
    for block in text.split("\n\t(footprint ")[1:]:
        ref = re.search(r'\(property "Reference" "([^"]+)"', block).group(1)
        ox, oy, rot = (list(map(float, re.search(r'\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', block).groups(default="0"))))
        if ref.startswith("H"):
            holes[ref] = (ox, oy)
            continue
        local = []
        for layer in ("F.CrtYd", "F.Fab"):
            for g in re.finditer(r'\(fp_(?:line|rect|poly)\b(.*?)\(layer "%s"\)' % re.escape(layer), block, re.S):
                local += [(float(x), float(y)) for x, y in re.findall(r'\((?:start|end|xy) ([-\d.]+) ([-\d.]+)\)', g.group(1))]
            if local:
                break
        r = math.radians(rot)
        pts = [(ox + x * math.cos(r) + y * math.sin(r), oy - x * math.sin(r) + y * math.cos(r)) for x, y in local]
        parts[ref] = pts

    (cx, cy), _ = _enclosing_circle(outline + parts[XIAO_REF])
    to_pod = lambda p: (p[0] - cx, -(p[1] - cy))
    box = lambda pts: (min(x for x, _ in pts), max(x for x, _ in pts), min(y for _, y in pts), max(y for _, y in pts))
    return {
        "outline": [to_pod(p) for p in outline],
        "parts": {ref: box([to_pod(p) for p in pts]) for ref, pts in parts.items() if pts},
        "holes": {ref: to_pod(p) for ref, p in holes.items()},
    }


DRIVER = load_driver_board(DRIVER_PCB)

# The cable drops straight into the Qwiic header
_j2 = DRIVER["parts"][CONNECTOR_REF]
CABLE_ENTRY_X, CABLE_ENTRY_Y = (_j2[0] + _j2[1]) / 2, (_j2[2] + _j2[3]) / 2

BOARD_BOTTOM_Z = PLUG_TOP_Z + STANDOFF_H
BOARD_TOP_Z = BOARD_BOTTOM_Z + BOARD_T


def revolve(points):
    """Revolve an (r, z) profile 360 degrees around the Z axis."""
    return cq.Workplane("XZ").polyline(points).close().revolve(360, (0, 0, 0), (0, 1, 0))


def cylinder(radius, z_from, z_to, x=0.0, y=0.0):
    return cq.Workplane("XY").workplane(offset=z_from).center(x, y).circle(radius).extrude(z_to - z_from)


def polar(radius, angle):
    return radius * math.cos(math.radians(angle)), radius * math.sin(math.radians(angle))


def underside_z(z0, r):
    """Height of the underside of the plate whose rim underside is at z0, at radius r."""
    return z0 + (R_OUT - r) * TAN


def above_underside(z0):
    """Everything above the (extended) underside cone of the plate at z0."""
    return revolve([(0, underside_z(z0, 0)), (R_OUT + 50, underside_z(z0, R_OUT + 50)), (R_OUT + 50, BIG), (0, BIG)])


def below_underside(z0):
    """Everything below the (extended) underside cone of the plate at z0."""
    return revolve([(0, underside_z(z0, 0)), (R_OUT + 50, underside_z(z0, R_OUT + 50)), (R_OUT + 50, -BIG), (0, -BIG)])


def cone(z0, r_in):
    """A sloped plate from the rim down at z0 up to an inner edge at r_in, with a drip skirt."""
    shell = revolve([
        (r_in, underside_z(z0, r_in)),
        (R_OUT, z0),
        (R_OUT, z0 + WALL_V),
        (r_in, underside_z(z0, r_in) + WALL_V),
    ])
    skirt = revolve([
        (R_OUT - SKIRT_W, z0 - SKIRT_H),
        (R_OUT, z0 - SKIRT_H),
        (R_OUT, z0 + WALL_V),
        (R_OUT - SKIRT_W, z0 + WALL_V),
    ])
    return shell.union(skirt)


def post_columns(diameter, z_from, z_to):
    result = None
    for angle in POST_ANGLES:
        column = cylinder(diameter / 2, z_from, z_to, *polar(POST_R, angle))
        result = column if result is None else result.union(column)
    return result


def posts_up_to_next(z0, z_bottom=None):
    """Posts rising from the plate at z0 that end exactly on the underside of the next plate up.

    With z_bottom the posts extend down to that height as legs; otherwise they start at the
    plate's own underside so nothing pokes out below it.
    """
    posts = post_columns(POST_D, -BIG if z_bottom is None else z_bottom, BIG)
    posts = posts.cut(above_underside(z0 + PITCH))
    if z_bottom is None:
        posts = posts.cut(below_underside(z0))
    return posts


def rod_holes(shape):
    return shape.cut(post_columns(ROD_HOLE_D, -BIG, BIG))


#----------------------------------------------------
# Shield parts, positioned as assembled

def make_plate(z0):
    return rod_holes(cone(z0, R_IN).union(posts_up_to_next(z0)))


def make_base():
    # The cone continues up to the cable hole, closing off the bottom of the center column so the
    # sensor can't see the ground.  Legs run down to the pod top alongside the skirt.
    base = cone(0, CABLE_HOLE_D / 2).union(posts_up_to_next(0, z_bottom=-SKIRT_H))
    return rod_holes(base)


def make_cap():
    z0 = CAP_Z
    disc_bottom = underside_z(z0, R_IN)
    disc_top = disc_bottom + DISC_T

    cap = cone(z0, R_IN)

    # Flat center, slightly wider than the opening so it fuses with the cone's inner edge
    cap = cap.union(cylinder(R_IN + 0.5, disc_bottom, disc_top))

    # Bosses on top give the screw heads a flat seat level with the disc
    bosses = post_columns(POST_D, -BIG, disc_top).cut(below_underside(z0))
    cap = cap.union(bosses)

    # Two rails hanging into the center column; the PCB slides up between them from below
    rail_inner_x = PCB_W / 2 - SLOT_DEPTH
    for side in (-1, 1):
        rail_center_x = side * (rail_inner_x + RAIL_W / 2)
        rail = (cq.Workplane("XY").workplane(offset=disc_bottom - RAIL_LEN)
                .center(rail_center_x, 0).rect(RAIL_W, RAIL_T).extrude(RAIL_LEN + 0.5))
        slot_w = SLOT_DEPTH + SLOT_CLEARANCE
        slot = (cq.Workplane("XY").workplane(offset=disc_bottom - RAIL_LEN - 1)
                .center(side * (rail_inner_x + slot_w / 2), 0)
                .rect(slot_w + 0.01, PCB_T + 2 * SLOT_CLEARANCE).extrude(RAIL_LEN + 1))
        cap = cap.union(rail).cut(slot)

    for y in (-MOUNT_HOLE_OFFSET, MOUNT_HOLE_OFFSET):
        cap = cap.cut(cylinder(MOUNT_HOLE_D / 2, disc_bottom - 1, disc_top + 1, 0, y))

    return rod_holes(cap)


#----------------------------------------------------
# Pod parts, positioned as assembled

def make_pod():
    pod = cylinder(R_OUT, POD_BOTTOM_Z, POD_TOP_Z)
    pod = pod.cut(cylinder(POD_BORE_R, POD_BOTTOM_Z - 1, POD_CEILING_Z))

    # Blind holes for the shield screws' heat-set inserts; they stop short of the inside
    for angle in POST_ANGLES:
        pod = pod.cut(cylinder(INSERT_HOLE_D / 2, POD_TOP_Z - INSERT_DEPTH, POD_TOP_Z + 1, *polar(POST_R, angle)))

    pod = pod.cut(cylinder(CABLE_ENTRY_D / 2, POD_CEILING_Z - 1, POD_TOP_Z + 1, CABLE_ENTRY_X, CABLE_ENTRY_Y))

    # Radial holes for the lid screws
    screw_z = POD_BOTTOM_Z + LID_SCREW_FROM_FLANGE
    for angle in LID_SCREW_ANGLES:
        hole = (cq.Workplane("YZ").workplane(offset=POD_BORE_R - 1).center(0, screw_z)
                .circle(LID_SCREW_HOLE_D / 2).extrude(POD_WALL + 2)
                .rotate((0, 0, 0), (0, 0, 1), angle))
        pod = pod.cut(hole)

    return pod


def make_pod_lid():
    lid = cylinder(R_OUT, POD_BOTTOM_Z - LID_FLANGE_T, POD_BOTTOM_Z)
    lid = lid.union(cylinder(PLUG_R, POD_BOTTOM_Z - 0.01, PLUG_TOP_Z))

    groove_z = PLUG_TOP_Z - GROOVE_FROM_TOP
    groove = revolve([
        (GROOVE_BOTTOM_R, groove_z - GROOVE_W / 2),
        (R_OUT, groove_z - GROOVE_W / 2),
        (R_OUT, groove_z + GROOVE_W / 2),
        (GROOVE_BOTTOM_R, groove_z + GROOVE_W / 2),
    ])
    lid = lid.cut(groove)

    screw_z = POD_BOTTOM_Z + LID_SCREW_FROM_FLANGE
    for angle in LID_SCREW_ANGLES:
        insert = (cq.Workplane("YZ").workplane(offset=PLUG_R - LID_INSERT_DEPTH).center(0, screw_z)
                  .circle(INSERT_HOLE_D / 2).extrude(LID_INSERT_DEPTH + 1)
                  .rotate((0, 0, 0), (0, 0, 1), angle))
        lid = lid.cut(insert)

    # Standoffs for the driver board's mounting holes, with M2.5 heat-set inserts
    for x, y in DRIVER["holes"].values():
        lid = lid.union(cylinder(STANDOFF_D / 2, PLUG_TOP_Z - 0.01, BOARD_BOTTOM_Z, x, y))
        lid = lid.cut(cylinder(STANDOFF_INSERT_D / 2, BOARD_BOTTOM_Z - STANDOFF_INSERT_DEPTH, BOARD_BOTTOM_Z + 1, x, y))

    return lid


def part_box(ref, z_from, z_to):
    xmin, xmax, ymin, ymax = DRIVER["parts"][ref]
    return (cq.Workplane("XY").workplane(offset=z_from).center((xmin + xmax) / 2, (ymin + ymax) / 2)
            .rect(xmax - xmin, ymax - ymin).extrude(z_to - z_from))


def make_grommet():
    """Split TPU grommet sealing the cable entry; press it in from outside after the cable is through."""
    x, y = CABLE_ENTRY_X, CABLE_ENTRY_Y
    flange = cylinder(GROMMET_FLANGE_D / 2, POD_TOP_Z, POD_TOP_Z + GROMMET_FLANGE_T, x, y)
    body = cylinder(CABLE_ENTRY_D / 2 + GROMMET_FIT, POD_CEILING_Z, POD_TOP_Z + 0.01, x, y)

    # Widest where it catches the inside of the pod top, tapering below so it pushes in easily
    barb = (cq.Workplane("XY").workplane(offset=POD_CEILING_Z - GROMMET_BARB_H).center(x, y)
            .circle(CABLE_ENTRY_D / 2 - 0.1).workplane(offset=GROMMET_BARB_H).circle(GROMMET_BARB_D / 2)
            .loft())
    grommet = flange.union(body).union(barb)

    z_from = POD_CEILING_Z - GROMMET_BARB_H - 1
    z_to = POD_TOP_Z + GROMMET_FLANGE_T + 1
    bore = (cq.Workplane("XY").workplane(offset=z_from).center(x, y)
            .slot2D(CABLE_W, CABLE_T).extrude(z_to - z_from))
    slit = (cq.Workplane("XY").workplane(offset=z_from).center(x + GROMMET_FLANGE_D / 4, y)
            .rect(GROMMET_FLANGE_D / 2, GROMMET_SLIT_W).extrude(z_to - z_from))
    return grommet.cut(bore).cut(slit)


def make_contents():
    """The driver board with its battery holder, XIAO and Qwiic header, for checking fit; not printed."""
    board = cq.Workplane("XY").workplane(offset=BOARD_BOTTOM_Z).polyline(DRIVER["outline"]).close().extrude(BOARD_T)
    board = board.union(part_box(HOLDER_REF, BOARD_TOP_Z - 0.01, BOARD_TOP_Z + HOLDER_H))
    board = board.union(part_box(XIAO_REF, BOARD_TOP_Z - 0.01, BOARD_BOTTOM_Z + XIAO_STACK_H))
    return board.union(part_box(CONNECTOR_REF, BOARD_TOP_Z - 0.01, BOARD_TOP_Z + CONNECTOR_H))


#----------------------------------------------------
# Checks

def check_fit():
    pcb_bottom = underside_z(CAP_Z, R_IN) - PCB_LEN
    apex_top = underside_z(0, CABLE_HOLE_D / 2) + WALL_V
    assert pcb_bottom - apex_top >= MIN_APEX_CLEARANCE, \
        f"PCB bottom is only {pcb_bottom - apex_top:.1f} mm above the base apex; add a plate or shorten the PCB"

    # The driver board, including the XIAO's USB-C overhang, has to fit the pod bore
    corners = DRIVER["outline"] + [(x, y) for xmin, xmax, ymin, ymax in DRIVER["parts"].values()
                                   for x in (xmin, xmax) for y in (ymin, ymax)]
    reach = max(math.hypot(x, y) for x, y in corners)
    assert reach < POD_BORE_R - 0.5, f"The driver board reaches {reach:.1f} mm from center; the pod bore is {POD_BORE_R:.1f} mm"

    # ...and its tall parts have to fit under the pod's top
    tops = {"battery holder": BOARD_TOP_Z + HOLDER_H, "XIAO": BOARD_BOTTOM_Z + XIAO_STACK_H,
            "Qwiic plug": BOARD_TOP_Z + CONNECTOR_H + PLUG_ROOM}
    for name, top in tops.items():
        assert top < POD_CEILING_Z - 0.5, f"The {name} is {top - POD_CEILING_Z + 0.5:.1f} mm too tall for the pod"

    # Nothing tall can sit under the grommet's barb
    for ref, top in [(HOLDER_REF, BOARD_TOP_Z + HOLDER_H), (XIAO_REF, BOARD_BOTTOM_Z + XIAO_STACK_H)]:
        xmin, xmax, ymin, ymax = DRIVER["parts"][ref]
        dx = max(xmin - CABLE_ENTRY_X, 0, CABLE_ENTRY_X - xmax)
        dy = max(ymin - CABLE_ENTRY_Y, 0, CABLE_ENTRY_Y - ymax)
        if math.hypot(dx, dy) < GROMMET_BARB_D / 2 + 0.5:
            assert top < POD_CEILING_Z - GROMMET_BARB_H - 1, f"The grommet sits on top of {ref}"

    # The standoffs' inserts have to stay clear of the lid's radial screw inserts
    standoff_insert_bottom = BOARD_BOTTOM_Z - STANDOFF_INSERT_DEPTH
    radial_insert_top = POD_BOTTOM_Z + LID_SCREW_FROM_FLANGE + INSERT_HOLE_D / 2
    for ref, (x, y) in DRIVER["holes"].items():
        assert math.hypot(x, y) + STANDOFF_D / 2 < PLUG_R, f"Standoff {ref} hangs off the lid"
        assert standoff_insert_bottom > radial_insert_top + 1 or math.hypot(x, y) + STANDOFF_INSERT_D / 2 < PLUG_R - LID_INSERT_DEPTH - 1, \
            f"Standoff {ref}'s insert runs into a lid screw insert"

    # A Qwiic plug has to pass through both cable holes so the parts come apart without desoldering
    plug_diagonal = math.hypot(*QWIIC_PLUG)
    for name, hole_d in [("base apex", CABLE_HOLE_D), ("pod top", CABLE_ENTRY_D)]:
        assert hole_d > plug_diagonal + 0.5, f"A Qwiic plug ({plug_diagonal:.1f} mm) won't pass the {name} hole"

    # The grommet's flange sits in the hollow under the base cone
    flange_room = underside_z(0, math.hypot(CABLE_ENTRY_X, CABLE_ENTRY_Y) + GROMMET_FLANGE_D / 2) - POD_TOP_Z
    assert flange_room > GROMMET_FLANGE_T + 3, f"Only {flange_room:.1f} mm under the base cone for the grommet"

    # The screw inserts in the pod top must clear the cable entry
    for angle in POST_ANGLES:
        x, y = polar(POST_R, angle)
        gap = math.hypot(x - CABLE_ENTRY_X, y - CABLE_ENTRY_Y) - INSERT_HOLE_D / 2 - GROMMET_FLANGE_D / 2
        assert gap > 1, f"The screw insert at {angle} degrees is too close to the cable entry"

    # The radial lid screws must stay below the O-ring
    groove_bottom_edge = PLUG_H - GROOVE_FROM_TOP - GROOVE_W / 2
    assert LID_SCREW_FROM_FLANGE + INSERT_HOLE_D / 2 < groove_bottom_edge, "Lid screws cut into the O-ring groove"

    return pcb_bottom - apex_top


#----------------------------------------------------
# Export

def to_print_bed(shape, flip=False):
    if flip:
        shape = shape.rotate((0, 0, 0), (1, 0, 0), 180)
    bb = shape.val().BoundingBox()
    return shape.translate((0, 0, -bb.zmin))


def export(name, shape):
    assert shape.val().isValid(), f"{name} is not a valid solid"
    cq.exporters.export(shape, str(OUT_DIR / f"{name}.step"))
    cq.exporters.export(shape, str(OUT_DIR / f"{name}.stl"), tolerance=0.05, angularTolerance=0.1)
    bb = shape.val().BoundingBox()
    print(f"{name:10s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:6.1f} mm")


def main():
    apex_clearance = check_fit()
    OUT_DIR.mkdir(exist_ok=True)

    base = make_base()
    plates = [make_plate(PITCH * (i + 1)) for i in range(PLATE_COUNT)]
    cap = make_cap()
    pod = make_pod()
    lid = make_pod_lid()
    grommet = make_grommet()

    export("base", to_print_bed(base))
    export("plate", to_print_bed(plates[0]))
    export("cap", to_print_bed(cap, flip=True))
    export("pod", to_print_bed(pod, flip=True))
    export("pod_lid", to_print_bed(lid))
    # Flange down on the bed
    export("grommet", to_print_bed(grommet.translate((-CABLE_ENTRY_X, -CABLE_ENTRY_Y, 0)), flip=True))

    assembly = cq.Assembly()
    for name, part in [("base", base), ("cap", cap), ("pod", pod), ("pod_lid", lid), ("grommet", grommet)]:
        assembly.add(part, name=name)
    for i, plate in enumerate(plates):
        assembly.add(plate, name=f"plate{i + 1}")
    compound = assembly.toCompound()
    cq.exporters.export(cq.Workplane().add(compound), str(OUT_DIR / "assembly.stl"), tolerance=0.05, angularTolerance=0.1)
    bb = compound.BoundingBox()
    print(f"{'assembly':10s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:6.1f} mm")

    # The STEP assembly also shows the pod's contents, for checking fit in a CAD viewer
    assembly.add(make_contents(), name="reference_contents", color=cq.Color("orange"))
    assembly.export(str(OUT_DIR / "assembly.step"))

    air_gap = (PITCH - WALL_V) * math.cos(math.radians(SLOPE_DEG))
    # Longest standard length that still stops short of the bottom of the blind insert hole, and
    # that reaches at least 3 mm into the insert
    max_screw = (underside_z(CAP_Z, R_IN) + DISC_T) - (POD_TOP_Z - INSERT_DEPTH) - 0.5
    screw_length = 5 * math.floor(max_screw / 5)
    assert max_screw - screw_length <= INSERT_DEPTH - 3.5, "Standard screw length won't reach far enough into the insert"
    oring_id = 2 * GROOVE_BOTTOM_R - 2 * 0.03 * GROOVE_BOTTOM_R
    print(f"\nAir gap between plates {air_gap:.1f} mm, plate overlap {RISE / PITCH:.1f}x the spacing, "
          f"PCB clears base apex by {apex_clearance:.1f} mm")
    print(f"Shield screws: {len(POST_ANGLES)}x M3 x {screw_length} mm into heat-set inserts")
    print(f"Driver board: {len(DRIVER['holes'])}x M2.5 into heat-set inserts in {STANDOFF_H:.0f} mm standoffs; "
          f"cable entry over the Qwiic header at ({CABLE_ENTRY_X:.1f}, {CABLE_ENTRY_Y:.1f})")
    print(f"O-ring: {ORING_CS:.1f} mm cross-section, ID about {oring_id:.0f} mm "
          f"(groove bottom {2 * GROOVE_BOTTOM_R:.1f} mm)")


if __name__ == "__main__":
    main()
