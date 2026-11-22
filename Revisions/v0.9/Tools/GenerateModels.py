"""Build the original Chronicles v0.9 mesh library from editable Python recipes."""

from __future__ import annotations

import argparse
import collections
import json
import math
import random
import re
from pathlib import Path


REVISION = Path(__file__).resolve().parents[1]
DATA = REVISION / "Game" / "Content" / "Data"
MODELS = DATA / "Models"
FACTIONS = ("nephites", "lamanites", "gadianton", "coriantumr", "shiz", "laban")

COLORS = {
    "ivory": "e3d0a6", "linen": "f0dfbb", "sand": "c8a273", "ochre": "be8a42",
    "amber": "daa352", "gold": "e0ad58", "bronze": "a97647", "copper": "ba724b",
    "iron": "5b6869", "lightiron": "85999a", "horizon": "819493", "charcoal": "383849",
    "plum": "754963", "wine": "783c48", "rust": "a9503d", "red": "ac4e47",
    "saffron": "ca854c", "moss": "626645", "olive": "77774a", "forest": "475c4c",
    "sage": "8b9164", "teal": "487b7b", "turquoise": "65a39a", "slate": "556c83",
    "clay": "a7654a", "darkclay": "76493d", "earth": "886653", "soil": "705346",
    "bark": "5e483b", "cedar": "92654c", "wood": "ac7950", "leather": "715048",
    "ink": "282638", "blackhair": "302b32", "brownhair": "543934", "auburn": "75473d",
    "grayhair": "9d9380", "skin": "bd895e", "lightskin": "c9946e", "darkskin": "9f684c",
    "stone": "a39a85", "rock": "81796f", "graphite": "656672", "cement": "c6bc9f",
    "purple": "625580", "berry": "a73d56", "grape": "724966", "wool": "ded2ba",
    "white": "efe1c7", "glowing": "d7e9ca", "goldgrain": "dcad59",
}


def rgb(color):
    color = COLORS.get(color, color)
    return [round(int(color[i:i + 2], 16) / 255, 4) for i in (0, 2, 4)] + [1.0]


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def mul(a, b):
    return tuple(x * b for x in a)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(vector):
    return mul(vector, 1 / math.sqrt(dot(vector, vector)))


def sides(count, phase=0):
    return [(math.cos(phase + i * math.tau / count), math.sin(phase + i * math.tau / count)) for i in range(count)]


def rectangle(cut=.15):
    return [(1-cut, 1), (-1+cut, 1), (-1, 1-cut), (-1, -1+cut), (-1+cut, -1), (1-cut, -1), (1, -1+cut), (1, 1-cut)]


class Mesh:
    def __init__(self, id, kind, transform=None, bone_prefix="", root_bone="root"):
        self.id = id
        self.kind = kind
        self.parts = []
        self.rig = {"root": {"pivot": [0, 0, 0]}}
        self.animation = {}
        self.counts = collections.Counter()
        self.transform = transform or (lambda vertex: vertex)
        self.bone_prefix = bone_prefix
        self.root_bone = root_bone

    def bone_name(self, bone):
        return self.root_bone if bone == "root" else self.bone_prefix + bone

    def bone(self, name, pivot, parent="root"):
        self.rig[self.bone_name(name)] = {
            "pivot": [round(p, 4) for p in self.transform(pivot)], "parent": self.bone_name(parent),
        }

    def socket(self, name, position, bone):
        self.animation.setdefault("sockets", {})[name] = {
            "position": [round(p, 4) for p in self.transform(position)], "bone": self.bone_name(bone),
        }

    def part(self, name, color, vertices, faces, bone="root", team=False):
        self.counts[name] += 1
        suffix = self.counts[name]
        part = {
            "name": name if suffix == 1 else f"{name}_{suffix}",
            "bone": self.bone_name(bone),
            "vertices": [[round(float(n), 4) for n in self.transform(v)] for v in vertices],
            "triangles": [index for face in faces for index in face], "color": rgb(color),
        }
        if team:
            part["team"] = True
        self.parts.append(part)

    def rings(self, name, color, rings, bone="root", team=False):
        """Close a mesh between matching counterclockwise rings."""
        count = len(rings[0])
        faces = [(0, i + 1, i) for i in range(1, count - 1)]
        for level in range(len(rings) - 1):
            bottom = level * count
            top = bottom + count
            for i in range(count):
                j = (i + 1) % count
                faces.extend(((bottom + i, bottom + j, top + j), (bottom + i, top + j, top + i)))
        faces.extend((top, top + i, top + i + 1) for i in range(1, count - 1))
        self.part(name, color, [vertex for ring in rings for vertex in ring], faces, bone, team)

    def loft(self, name, color, sections, bone="root", n=8, cut=None, team=False):
        """Sections list center x/y/z followed by half widths in x/y."""
        outline = rectangle(cut) if cut is not None else sides(n, math.pi / n)
        self.rings(name, color, [[(x + a * dx, y + b * dy, z) for a, b in outline]
                                 for x, y, z, dx, dy in sections], bone, team)

    def box(self, name, color, center, size, bone="root", team=False, bevel=0):
        x, y, z = center
        dx, dy, dz = (v / 2 for v in size)
        if bevel:
            r = min(bevel, dx * .4, dy * .4, dz * .4)
            self.loft(name, color, [(x, y, z-dz, dx-r, dy-r), (x, y, z-dz+r, dx, dy),
                                    (x, y, z+dz-r, dx, dy), (x, y, z+dz, dx-r, dy-r)], bone,
                      cut=.14, team=team)
        else:
            self.loft(name, color, [(x, y, z-dz, dx * math.sqrt(2), dy * math.sqrt(2)),
                                    (x, y, z+dz, dx * math.sqrt(2), dy * math.sqrt(2))], bone,
                      n=4, team=team)

    def beam(self, name, color, a, b, r, end=None, bone="root", n=6, aspect=1, team=False):
        end = end if end is not None else r
        direction = unit(sub(b, a))
        axis = (0, 0, 1) if abs(direction[2]) < .95 else (0, 1, 0)
        u = unit(cross(axis, direction))
        v = cross(direction, u)
        outline = sides(n, math.pi / n)
        self.rings(name, color, [[add(p, add(mul(u, x * radius), mul(v, y * radius * aspect)))
                                  for x, y in outline] for p, radius in ((a, r), (b, end))], bone, team)

    def gem(self, name, color, center, radii, bone="root", n=7, skew=0):
        x, y, z = center
        dx, dy, dz = radii
        vertices = [(x-skew*dx, y, z-dz), (x+skew*dx, y, z+dz)]
        for i, (a, b) in enumerate(sides(n, .1)):
            vertices.append((x+a*dx, y+b*dy, z + dz * (.15 if i % 2 else -.15)))
        faces = []
        for i in range(n):
            j = (i+1) % n
            faces.extend(((0, j+2, i+2), (1, i+2, j+2)))
        self.part(name, color, vertices, faces, bone)

    def prism(self, name, color, x, half_depth, outline, bone="root", team=False):
        """Extrude a convex counterclockwise Y/Z profile along X."""
        self.rings(name, color, [[(x + depth, y, z) for y, z in outline]
                                 for depth in (-half_depth, half_depth)], bone, team)

    def insert(self, child, prefix):
        for part in child.parts:
            part["name"] = prefix + part["name"]
            self.parts.append(part)
        self.rig.update({name: entry for name, entry in child.rig.items() if name != "root"})

    def document(self, display_name, faction=None):
        verts = [v for part in self.parts for v in part["vertices"]]
        lo = [min(v[i] for v in verts) for i in range(3)]
        hi = [max(v[i] for v in verts) for i in range(3)]
        result = {"version": 1, "id": self.id, "kind": self.kind, "name": display_name,
                  "height": round(hi[2] - lo[2], 4), "footprint": [round(2 * max(abs(lo[i]), abs(hi[i])), 4) for i in range(2)],
                  "bounds": [lo, hi], "rig": self.rig, "parts": self.parts}
        if faction:
            result["faction"] = faction
        if self.animation:
            result["animation"] = self.animation
        return result


def path(mesh, name, color, points, radius, bone="root", n=5, widths=None):
    widths = widths or [1] * len(points)
    for i in range(1, len(points)):
        mesh.beam(name, color, points[i-1], points[i], radius * widths[i-1], radius * widths[i], bone, n=n)


def flag(mesh, name, color, x, y, z, width, height, direction=-1, bone="root"):
    mesh.part(name, color, [(x,y,z), (x+direction*width*.52,y+.06,z-.06*height),
                           (x+direction*width,y-.05,z-.12*height), (x+direction*width*.77,y-.02,z-.62*height),
                           (x+direction*width,y-.05,z-height), (x+direction*width*.50,y+.05,z-.93*height),
                           (x,y,z-.91*height)], [(0,1,6),(1,5,6),(1,3,5),(1,2,3),(3,4,5)], bone, True)


def jar(mesh, name, position, radius, height, color="clay", bone="root", trim="ivory"):
    x, y, z = position
    mesh.loft(name, color, [(x,y,z,radius*.56,radius*.56), (x,y,z+.18*height,radius*.88,radius*.88),
                            (x,y,z+.62*height,radius,radius), (x,y,z+.84*height,radius*.53,radius*.53),
                            (x,y,z+height,radius*.48,radius*.48)], bone, n=6)
    mesh.loft(name + "_rim", trim, [(x,y,z+.92*height,radius*.50,radius*.50),
                                    (x,y,z+height,radius*.63,radius*.63)], bone, n=6)


def basket(mesh, name, position, radius, height, color="ochre", bone="root"):
    x, y, z = position
    mesh.loft(name, color, [(x,y,z,radius*.73,radius*.73), (x,y,z+height,radius,radius)], bone, n=7)
    mesh.loft(name+"_rim", "wood", [(x,y,z+height*.90,radius*1.035,radius*1.035),
                                    (x,y,z+height*1.035,radius*1.035,radius*1.035)], bone, n=7)


# Builds use the same base anatomy and keep their role readable through clothing and equipment.
# Tuple order: tunic, mantle, body, hair or headgear, right hand, left hand, stature.
PEOPLE = {
    "aaron": ("ochre", "sand", "scale", "graylong", "spear", "oval", 1.07),
    "akish": ("plum", "", "leather", "swept", "cimeter", "", .96),
    "alma": ("linen", "teal", "robe", "graybeard", "staff", "book", 1.04),
    "amalickiah": ("wine", "red", "scale", "crown", "sword", "", 1.14),
    "ammon": ("ochre", "linen", "light", "tail", "sword", "sling", 1.10),
    "ammonite_covenant_keeper": ("sage", "linen", "robe", "short", "wrapped_bow", "", .97),
    "ammoron": ("rust", "rust", "scale", "crested", "cimeter", "tower_shield", 1.09),
    "band_captain": ("moss", "red", "leather", "mask", "cleaver", "pouch", 1.04),
    "bowman": ("sand", "teal", "light", "short", "", "bow", .99),
    "brother_of_jared": ("ivory", "earth", "robe", "graybeard", "staff", "stone", 1.07),
    "captain_moroni": ("ivory", "teal", "plate", "ridge", "standard", "sword", 1.17),
    "chief_captain": ("wine", "wine", "plate", "ridge", "spear", "square", 1.09),
    "cimeter_bearer": ("rust", "saffron", "leather", "band", "cimeter", "round", 1.02),
    "coriantumr": ("ochre", "forest", "scale", "war_crown", "greatsword", "tower_shield", 1.19),
    "drunken_concubine": ("plum", "linen", "dress", "braid", "goblet", "jar", .99),
    "emer": ("amber", "ochre", "robe", "goldbeard", "scepter", "tablet", 1.06),
    "gadianton": ("charcoal", "wine", "leather", "hood", "dagger", "dagger", 1.03),
    "gadianton_robber": ("forest", "moss", "leather", "mask", "dagger", "dagger", .91),
    "giddianhi": ("charcoal", "rust", "scale", "beard", "cimeter", "", 1.13),
    "gilead": ("stone", "slate", "plate", "baldbeard", "mace", "round", 1.08),
    "helaman": ("ivory", "linen", "plate", "long", "pennant", "round", 1.06),
    "industrious_laborer": ("ivory", "teal", "worker", "tail", "mallet", "plank", .93),
    "jacob_gadianton": ("olive", "moss", "robe", "swept", "scroll", "dagger", 1.02),
    "jared": ("linen", "ochre", "robe", "beard", "staff", "satchel", 1.11),
    "jaredite_archer": ("slate", "horizon", "scale", "helmet", "", "longbow", 1.10),
    "jaredite_shield_bearer": ("ochre", "iron", "scale", "helmet", "spear", "tower_shield", 1.14),
    "jaredite_warrior": ("rust", "ochre", "harness", "wild", "hammer", "", 1.18),
    "keeper_of_flocks": ("ivory", "ochre", "robe", "wrap", "crook", "basket", .94),
    "king_noah": ("wine", "teal", "royal", "goldbeard", "goblet", "scepter", 1.12),
    "kishkumen": ("charcoal", "purple", "leather", "mask", "dagger", "scroll", .97),
    "korihor": ("plum", "linen", "robe", "swept", "", "book", 1.03),
    "laban": ("wine", "ivory", "royal", "goldbeard", "goblet", "sword", 1.12),
    "laman": ("leather", "rust", "leather", "longbeard", "cimeter", "", 1.10),
    "lawyer": ("slate", "ochre", "robe", "beard", "scroll", "tablet", 1.00),
    "lemuel": ("sand", "ochre", "leather", "tail", "spear", "round", 1.02),
    "lib": ("moss", "forest", "scale", "crown", "spear", "horn", 1.10),
    "man_of_moroni": ("ivory", "teal", "plate", "helmet", "sword", "oval", 1.05),
    "man_of_the_sword": ("ivory", "sand", "leather", "short", "sword", "round", 1.01),
    "morianton": ("clay", "slate", "scale", "auburncrown", "axe", "tablet", 1.10),
    "mormon": ("ochre", "teal", "plate", "graybeard", "sword", "plates", 1.12),
    "moroni_son": ("sand", "ivory", "robe", "graylong", "sword", "plates", 1.07),
    "nehor": ("wine", "gold", "royal", "wild", "", "bowl", 1.10),
    "nephi": ("ivory", "teal", "harness", "long", "sword", "plates", 1.13),
    "oath_keeper": ("iron", "charcoal", "robe", "helmet", "spear", "square", 1.07),
    "omer": ("sand", "olive", "robe", "graycrown", "staff", "satchel", 1.01),
    "one_of_the_fifty": ("wine", "gold", "plate", "crested", "cimeter", "square", 1.07),
    "pahoran": ("ivory", "teal", "robe", "beard", "staff", "tablet", 1.05),
    "priestcrafter": ("plum", "gold", "royal", "priest", "bowl", "", 1.04),
    "sam": ("ochre", "ivory", "robe", "beard", "staff", "bedroll", 1.01),
    "shared": ("rust", "earth", "scale", "baldbeard", "doubleaxe", "", 1.15),
    "shiz": ("charcoal", "wine", "scale", "iron_crown", "greatsword", "", 1.24),
    "slinger_of_stones": ("sand", "moss", "light", "band", "sling", "pouch", .93),
    "stripling_warrior": ("linen", "teal", "light", "band", "spear", "round", .92),
    "teacher_of_the_word": ("ivory", "moss", "robe", "graybeard", "staff", "book", 1.02),
    "tiller_of_the_earth": ("ochre", "linen", "worker", "straw", "hoe", "basket", .94),
    "trader_in_precious_things": ("ochre", "teal", "worker", "wrap", "scales", "cargo", 1.00),
    "tubaloth": ("ivory", "red", "scale", "war_crown", "spear", "oval", 1.13),
    "wilderness_spy": ("olive", "moss", "light", "hood", "", "bow", .93),
    "worker_in_ore": ("rust", "ivory", "worker", "wrap", "pick", "ore_basket", .99),
    "zemnarihah": ("charcoal", "forest", "leather", "hood", "pennant", "round", 1.07),
    "zerahemnah": ("earth", "saffron", "scale", "wild", "cimeter", "spiked", 1.14),
    "zoram": ("horizon", "ivory", "robe", "tail", "keys", "sword", 1.02),
    "zoramite_captain": ("rust", "red", "plate", "priest", "spear", "square", 1.11),
    "lehi": ("sand", "ivory", "robe", "graybeard", "staff", "scroll", 1.09),
    "amulek": ("teal", "sand", "robe", "beard", "book", "", 1.06),
    "teancum": ("ivory", "teal", "harness", "tail", "pennant", "dagger", 1.12),
}


def blade(mesh, name, color, handle, length, width, bone, curl=0, lean=.2):
    x, y, z = handle
    sections = [(x,y,z,width*.48,.032), (x+lean*.26,y,z+length*.24,width,.036),
                (x+lean*.75+curl*.29,y,z+length*.70,width*.72,.026),
                (x+lean+curl,y,z+length,width*.04,.006)]
    mesh.loft(name, color, sections, bone, n=4)


def shield(mesh, kind, grip, bone):
    x, y, z = add(grip, (.12, 0, .04))
    if kind in {"round", "spiked", "oval"}:
        rx, rz = (.25, .38) if kind == "oval" else (.245, .245)
        mesh.prism("shield_rim", "bronze", x, .043, [(y+a*rx,z+b*rz) for a,b in sides(9)], bone)
        mesh.prism("shield_hide", "wood", x+.043, .018, [(y+a*rx*.83,z+b*rz*.85) for a,b in sides(9)], bone)
        mesh.gem("shield_boss", "gold", (x+.083,y,z), (.072,.076,.077), bone, n=6)
        if kind == "spiked":
            for a, b in sides(5):
                mesh.beam("shield_stud", "gold", (x+.054,y+a*.166,z+b*.166), (x+.125,y+a*.183,z+b*.183),
                          .028, .005, bone, n=4)
    else:
        width, height = (.255,.48) if kind == "tower_shield" else (.26,.32)
        mesh.prism("shield_rim", "bronze", x, .043, [(y+a*width,z+b*height) for a,b in rectangle(.24)], bone)
        mesh.prism("shield_panel", "iron" if kind == "tower_shield" else "leather", x+.043, .019,
                   [(y+a*width*.84,z+b*height*.88) for a,b in rectangle(.23)], bone)
        mesh.box("shield_spine", "gold", (x+.072,y,z), (.036,.037,height*1.58), bone)
        for side in (-1,1):
            mesh.gem("shield_stud", "gold", (x+.074,y+side*.149,z+.11), (.027,.033,.033), bone, n=4)


def equipment(mesh, item, hand, bone, tunic):
    x, y, z = hand
    if item in {"round", "oval", "spiked", "square", "tower_shield"}:
        shield(mesh, item, hand, bone)
    elif item in {"sword", "greatsword", "cimeter", "dagger", "cleaver"}:
        length, width, curl, lean = {"sword": (.65,.070,0,.14), "greatsword": (1.07,.110,0,.14),
                                    "cimeter": (.65,.085,.32,.07), "dagger": (.31,.044,.09,.12),
                                    "cleaver": (.46,.122,.14,.05)}[item]
        mesh.beam("blade_handle", "bark", (x-.04,y,z-.14), (x,y,z+.105), .031, bone=bone, n=5)
        mesh.beam("blade_crossguard", "gold", (x-.12,y,z+.09), (x+.13,y,z+.095), .027, bone=bone, n=5)
        mesh.gem("blade_pommel", "bronze", (x-.043,y,z-.139), (.044,.038,.050), bone, n=5)
        blade(mesh, item, "gold" if item == "cimeter" else "lightiron", (x,y,z+.10), length, width, bone, curl, lean)
    elif item in {"spear", "pennant", "staff", "standard", "crook", "scepter", "pick", "hoe", "hammer", "axe", "doubleaxe", "mace", "mallet"}:
        top = {"spear": .98, "pennant": .91, "staff": .80, "standard": 1.07, "crook": .82, "scepter": .56,
               "pick": .53, "hoe": .43, "hammer": .68, "axe": .63, "doubleaxe": .73, "mace": .58, "mallet": .31}[item]
        long = item in {"spear","pennant","staff","standard","crook"}
        mesh.beam("haft", "wood", (x-.10,y,z-(.78 if long else .30)), (x+.12,y,z+top),
                  .025 if long else .039, .019 if long else .032, bone, n=5)
        tip = (x+.125,y,z+top)
        if item in {"spear", "pennant", "standard"}:
            mesh.gem("spearhead", "gold" if item == "standard" else "lightiron", add(tip,(.02,0,.11)),
                     (.059,.042,.18), bone, n=4, skew=.35)
        if item in {"pennant", "standard"}:
            flag(mesh, "standard_cloth", "ivory" if item == "standard" else tunic, x+.11,y,z+top-.04,
                 .58 if item == "standard" else .33, .52 if item == "standard" else .23, bone=bone)
            if item == "standard":
                mesh.beam("standard_crossbar", "bronze", (x+.13,y,z+top), (x-.48,y,z+top-.055), .024, bone=bone, n=5)
        if item == "crook":
            path(mesh, "crook_hook", "wood", [tip,(x+.14,y,z+top+.12),(x+.25,y,z+top+.16),(x+.34,y,z+top+.095),
                                              (x+.36,y,z+top+.01)], .027, bone)
        if item in {"staff", "scepter", "mace"}:
            radii = (.095,.094,.108) if item == "mace" else (.063,.056,.078)
            mesh.gem(f"{item}_finial", "gold" if item == "scepter" else "bronze", tip, radii, bone)
        if item in {"hammer", "mallet"}:
            size = (.39,.20,.18) if item == "hammer" else (.21,.125,.117)
            mesh.box("hammer_head", "stone" if item == "hammer" else "cedar", tip, size, bone, bevel=.034)
            mesh.box("hammer_binding", "leather", tip, (size[0]*.24,size[1]+.011,size[2]+.011), bone)
        if item in {"pick","hoe"}:
            path(mesh, item + "_head", "lightiron", [(x-.15,y,z+top+.17),tip,(x+.38,y,z+top+.05)], .052, bone, widths=[.16,1,.22])
            if item == "hoe":
                mesh.box("hoe_blade", "iron", (x+.26,y,z+top-.04), (.055,.155,.21), bone, bevel=.015)
        if item in {"axe","doubleaxe"}:
            for side in ((-1,1) if item == "doubleaxe" else (1,)):
                mesh.gem("axe_head", "bronze", (x+.12+side*.10,y,z+top+.03), (.147,.049,.16), bone, n=5, skew=side*.47)
    elif item in {"bow", "longbow", "wrapped_bow"}:
        wrapped = item == "wrapped_bow"
        height = .75 if item == "longbow" else .58
        points = [(x-.23,y,z-height), (x-.12,y,z-height*.74), (x-.029,y,z-height*.34),
                  hand, (x-.028,y,z+height*.33), (x-.12,y,z+height*.72), (x-.23,y,z+height)]
        if wrapped:
            points = [(x+(px-x)*.62+(pz-z)*.70,py,z+(pz-z)*.70) for px,py,pz in points]
        path(mesh, "bow_stave", "ivory" if wrapped else "wood", points, .043, bone, widths=[.38,.72,.89,1,.89,.72,.38])
        if not wrapped:
            middle = mul(add(points[0],points[-1]),.5)
            mesh.beam("bowstring_lower", "linen", points[0], middle, .009, bone=bone, n=4)
            mesh.beam("bowstring_upper", "linen", middle, points[-1], .009, bone=bone, n=4)
            mesh.socket("bowstring_lower", points[0], bone)
            mesh.socket("bowstring_upper", points[-1], bone)
        else:
            mesh.beam("woven_bow_binding", "sage", points[2], points[4], .050, bone=bone, n=5)
    elif item == "sling":
        path(mesh, "sling_strap", "leather", [(x,y,z),(x+.19,y+.07,z+.08),(x+.35,y+.04,z+.12),
                                               (x+.44,y-.01,z+.04)], .022, bone, n=4)
        mesh.gem("sling_stone", "rock", (x+.35,y+.035,z+.095), (.078,.047,.053), bone, n=5)
    elif item in {"book", "tablet", "plates"}:
        center = (x+.102,y,z+.03)
        if item == "book":
            for side in (-1,1):
                mesh.box("book_cover", "leather", (center[0],y+side*.087,z+.027), (.23,.172,.058), bone, bevel=.013)
                mesh.box("linen_pages", "linen", (center[0],y+side*.087,z+.058), (.204,.15,.026), bone)
            mesh.beam("bound_spine", "bronze", (x-.016,y,z+.04),(x+.22,y,z+.04), .02, bone=bone, n=4)
        if item == "tablet":
            mesh.box("law_tablet", "sand", center, (.14,.31,.27), bone, bevel=.03)
            for i in range(3):
                mesh.box("tablet_inlay", "bronze", (center[0]+.073,y,z+.08-i*.068), (.012,.20,.015), bone)
        if item == "plates":
            for i in range(4):
                mesh.box("record_plate", "gold" if i % 2 else "bronze", (center[0],y,z-.02+i*.033), (.23,.285,.028), bone)
            for side in (-1,1):
                mesh.beam("record_bands", "leather", (x+.06,y+side*.08,z-.037), (x+.06,y+side*.08,z+.108), .021, bone=bone, n=4)
    elif item in {"basket","ore_basket", "jar", "goblet","bowl"}:
        if item == "jar":
            jar(mesh, "amphora", (x+.065,y,z-.15), .125,.36, bone=bone)
        elif item in {"basket","ore_basket"}:
            basket(mesh, item, (x+.13,y,z-.10), .157,.17, bone=bone)
            for i, (a,b) in enumerate(sides(5)):
                mesh.gem("basket_contents", "lightiron" if item == "ore_basket" else ("goldgrain" if i % 2 else "amber"),
                         (x+.13+a*.09,y+b*.092,z+.083), (.056,.046,.055), bone, n=4)
        else:
            stem = .10 if item == "goblet" else .02
            mesh.beam("vessel_foot", "bronze", (x+.055,y,z-.025), (x+.055,y,z+stem), .059,.042,bone,n=5)
            mesh.loft(item, "gold", [(x+.055,y,z+stem,.053,.053), (x+.055,y,z+stem+.07,.089,.089),
                                     (x+.055,y,z+stem+.13,.103,.103)], bone, n=6)
            mesh.loft("vessel_interior", "wine", [(x+.055,y,z+stem+.1301,.078,.078), (x+.055,y,z+stem+.137,.079,.079)], bone, n=6)
    elif item == "scroll":
        mesh.beam("rolled_parchment", "ivory", (x-.015,y,z-.14),(x+.08,y,z+.17),.057,bone=bone,n=6)
        mesh.beam("scroll_seal", "rust", (x+.022,y,z-.025),(x+.045,y,z+.05),.062,bone=bone,n=6)
    elif item == "keys":
        for i in range(6):
            a, b = sides(6)[i], sides(6)[(i+1)%6]
            mesh.beam("key_ring", "bronze", (x+.1,y+a[0]*.083,z+.035+a[1]*.083),
                      (x+.1,y+b[0]*.083,z+.035+b[1]*.083), .018, bone=bone, n=4)
        for i in range(3):
            mesh.box("household_key", "gold", (x+.08+i*.02,y+.04-i*.052,z-.13-i*.018), (.033,.035,.19), bone)
    elif item == "scales":
        mesh.beam("scale_pole", "bronze", (x,y,z-.053),(x,y,z+.19),.020,bone=bone,n=5)
        mesh.beam("scale_crossbar", "gold", (x,y-.155,z+.162),(x,y+.155,z+.162),.019,bone=bone,n=4)
        for sign in (-1,1):
            mesh.beam("scale_tether", "wood", (x,y+sign*.14,z+.16),(x,y+sign*.15,z+.028),.009,bone=bone,n=4)
            mesh.gem("balance_pan", "bronze", (x,y+sign*.15,z+.025),(.085,.085,.027),bone,n=6)
    elif item == "plank":
        mesh.beam("timber_load", "wood", (x-.66,y,z-.09),(x+.60,y,z+.14),.149,bone=bone,n=4,aspect=.56)
        mesh.beam("timber_binding", "ivory", (x-.19,y,z-.005),(x-.10,y,z+.011),.155,bone=bone,n=4,aspect=.59)
    elif item == "stone":
        mesh.gem("glowing_stone", "glowing", (x+.08,y,z+.083), (.088,.079,.097), bone, n=6)
    elif item == "horn":
        path(mesh,"hunting_horn","ivory",[(x-.05,y,z+.12),(x+.04,y,z+.03),(x+.16,y,z),(x+.27,y,z+.04)],.058,
             bone,n=6,widths=[.54,.72,.92,1.46])
    elif item == "pouch":
        mesh.gem("stone_pouch", "leather", (x+.012,y,z-.08), (.099,.087,.13), bone,n=6)
    elif item == "satchel":
        mesh.box("travel_satchel", "leather", (x,y,z-.13), (.17,.21,.255), bone, bevel=.035)
        mesh.box("travel_satchel_clasp", "bronze", (x+.091,y,z-.08), (.023,.05,.045), bone)
    elif item in {"cargo","bedroll",""}:
        return
    else:
        raise ValueError(f"Unknown equipment {item}")


def head(mesh, role, skin, width):
    bone = "head"
    hair = "grayhair" if role.startswith("gray") else "auburn" if role.startswith("auburn") else "blackhair"
    mesh.loft("face", skin, [( .026,0,1.48,.090,.087*width), (.045,0,1.54,.124,.102*width),
                            (.023,0,1.66,.136,.116*width), (0,0,1.77,.114,.105*width),
                            (-.012,0,1.82,.074,.072*width)], bone,n=8)
    mesh.gem("nose", skin, (.151,0,1.646),(.045,.032,.044),bone,n=5)
    for sign in (-1,1):
        mesh.box("brow", hair, (.139,sign*.058,1.697),(.028,.057,.016),bone)
        mesh.box("eye", "ink", (.151,sign*.058,1.674),(.018,.027,.010),bone)
        mesh.gem("ear",skin,(.011,sign*.116*width,1.638),(.04,.035,.053),bone,n=4)
    if role not in {"baldbeard","hood","mask","wrap","straw","helmet","ridge","crested","priest"}:
        mesh.loft("hair_cap", hair, [(-.047,0,1.664,.111,.125*width),(-.026,0,1.77,.155,.131*width),
                                     (-.031,0,1.829,.124,.106*width),(-.045,0,1.859,.063,.075*width)],bone,n=7)
    if role in {"long","graylong","longbeard","hood","mask","wild"}:
        fabric = "forest" if role == "hood" else "charcoal" if role == "mask" else hair
        for side in (-1,1):
            mesh.loft("head_side_drape",fabric,[(-.072,side*.107,1.43,.076,.030),(-.068,side*.107,1.60,.112,.051),
                                               (-.023,side*.096,1.78,.139,.062)],bone,n=6)
        if role in {"hood","mask"}:
            mesh.loft("hood_top",fabric,[(-.055,0,1.715,.15,.138),(-.056,0,1.837,.146,.122),
                                         (-.095,0,1.92,.070,.074),(-.127,0,1.947,.02,.023)],bone,n=6)
            if role == "mask":
                mesh.box("face_wrap", "charcoal", (.131,0,1.566), (.075,.198,.097), bone, bevel=.015)
    if role in {"tail","braid","swept"}:
        end = (-.166,0,1.46 if role == "tail" else 1.593)
        path(mesh,"tied_hair",hair,[(-.106,0,1.685),(-.184,0,1.635),end],.058,bone,n=5,widths=[1,1,.49])
        if role == "braid":
            mesh.gem("hair_coil",hair,(-.121,0,1.855),(.090,.077,.084),bone,n=6)
    if "beard" in role or role in {"graylong","graycrown","war_crown","iron_crown","auburncrown"}:
        tip = 1.354 if role in {"graybeard","goldbeard","graycrown"} else 1.445
        mesh.loft("beard",hair,[(.088,0,tip,.037,.032),(.103,0,1.499,.085,.092),
                                        (.092,0,1.567,.072,.112)],bone,n=6)
        mesh.box("moustache", hair, (.162,0,1.601),(.035,.127,.025),bone,bevel=.005)
    if role in {"helmet","crested","ridge","priest"}:
        crown = "bronze" if role != "priest" else "wine"
        mesh.loft("helmet",crown,[(-.012,0,1.682,.155,.143),(-.022,0,1.795,.147,.134),
                                   (-.03,0,1.879,.085,.089),(-.03,0,1.90,.043,.051)],bone,cut=.32)
        mesh.loft("helmet_brow", "gold", [(.004,0,1.688,.157,.143),(.004,0,1.727,.157,.143)],bone,cut=.35)
        for sign in (-1,1):
            mesh.box("cheek_guard", "bronze", (.012,sign*.128,1.603),(.098,.065,.195),bone,bevel=.019)
        if role == "crested":
            mesh.prism("helmet_plume", "red", -.018,.17,[(-.041,1.852),(.041,1.852),(.051,1.99),(-.051,1.99)],bone,True)
        if role == "ridge":
            mesh.beam("helmet_ridge","gold",(-.143,0,1.846),(.12,0,1.899),.046,bone=bone,n=5)
        if role == "priest":
            mesh.loft("headdress", "wine", [(0,0,1.775,.153,.157),(-.02,0,1.969,.104,.119)], bone,cut=.24)
            mesh.box("headdress_facing","gold",(.084,0,1.889),(.037,.137,.126),bone,bevel=.018)
    elif role == "straw":
        mesh.loft("straw_hat", "goldgrain", [(0,0,1.725,.26,.279),(0,0,1.77,.271,.288),
                                               (0,0,1.812,.142,.153),(0,0,1.90,.101,.108)],bone,n=10)
        mesh.loft("hat_band", "cedar", [(0,0,1.796,.166,.181),(0,0,1.814,.153,.165)],bone,n=10)
    elif role == "wrap":
        mesh.loft("headcloth", "ivory", [(-.015,0,1.663,.151,.143),(-.018,0,1.751,.16,.15),
                                           (-.042,0,1.844,.128,.138),(-.053,0,1.884,.065,.079)],bone,n=8)
        mesh.beam("headcloth_fold", "sand", (.124,-.10,1.729),(.115,.12,1.785), .026,bone=bone,n=5)
    elif role in {"crown","war_crown","iron_crown","goldbeard","graycrown","auburncrown","band"}:
        metal = "iron" if role == "iron_crown" else "teal" if role == "band" else "gold"
        level = 1.728 if role == "band" else 1.789
        mesh.loft("circlet",metal,[(-.026,0,level,.154,.135),(-.028,0,level+.056,.156,.14)],bone,n=8,team=role=="band")
        if role not in {"band","graycrown"}:
            crown_height = .087 if role in {"war_crown","iron_crown","goldbeard"} else .048
            for i, (a,b) in enumerate(sides(5)):
                mesh.gem("crown_point",metal, (-.026+a*.116,b*.117,level+.058),
                         (.039,.037,crown_height*(1.15 if i==0 else 1)),bone,n=4)


def human_style(coat, right_item, bow_held):
    if bow_held or right_item == "sling":
        return "ranged"
    if coat == "worker":
        return "worker"
    if coat == "dress":
        return "concubine"
    if right_item in {"spear", "pennant", "standard"}:
        return "spear"
    if right_item in {"sword", "greatsword", "dagger", "cimeter", "cleaver", "mace", "axe", "doubleaxe", "hammer"}:
        return "sword"
    return "staff"


def arm_pose(sign, shoulders, style, item):
    shoulder = (0,sign*.255*shoulders,1.34)
    elbow = (.005,sign*.365*shoulders,1.092)
    grip = (.23,sign*.39*shoulders,1.016)
    if item in {"bow", "longbow"}:
        return shoulder,(.288,-.239,1.234),(.536,-.171,1.247)
    if style == "ranged" and item == "":
        return shoulder,(-.103,.242*shoulders,1.138),(.219,-.153,1.284)
    if item == "sling":
        return shoulder,(.036,.362,1.206),(.35,.438,1.140)
    if item == "plank":
        return shoulder,(-.048,-.38,1.137),(.114,-.322,1.390)
    if item in {"basket", "ore_basket", "jar", "pouch"}:
        return shoulder,(.020,sign*.367*shoulders,1.09),(.136,sign*.48*shoulders,.886)
    if item in {"mallet", "hoe", "pick"}:
        return shoulder,(.048,.367,1.104),(.265,.436,.963)
    if item in {"goblet", "bowl", "book"}:
        return shoulder,(.10,sign*.306*shoulders,1.145),(.346,sign*.299*shoulders,1.295)
    if item in {"round", "oval", "spiked", "square", "tower_shield"}:
        return shoulder,(.035,sign*.382*shoulders,1.106),(.219,sign*.333*shoulders,1.130)
    if item in {"spear", "pennant", "standard"}:
        return shoulder,(-.051,sign*.366*shoulders,1.120),(.219,sign*.46*shoulders,1.09)
    if item in {"greatsword", "hammer", "doubleaxe"}:
        return shoulder,(.051,sign*.370*shoulders,1.153),(.313,sign*.371*shoulders,1.045)
    if style == "worker" and not item:
        return shoulder,elbow,(.161,sign*.37,.965)
    return shoulder,elbow,grip


def person(id, kind, faction=None, offset=(0,0,0), scale=1, prefix="", root_bone="root"):
    tunic, mantle, coat, hair, right_item, left_item, stature = PEOPLE[id]
    if faction and id == "industrious_laborer":
        tunic, mantle, coat, hair, right_item, left_item, stature = {
            "laban": ("ivory","wine","worker","short","hoe","basket",.96),
            "gadianton": ("earth","moss","worker","wrap","mallet","pouch",.92),
            "lamanites": ("sand","rust","worker","wrap","mallet","basket",.95),
            "coriantumr": ("ivory","horizon","worker","wrap","mallet","plank",1.00),
            "shiz": ("clay","wine","worker","band","pick","plank",1.02),
        }.get(faction, PEOPLE[id])
    elif faction and kind != "hero":
        palette = {"nephites":"teal", "lamanites":"rust", "gadianton":"forest", "coriantumr":"horizon",
                   "shiz":"wine", "laban":"plum"}
        if mantle and coat not in {"dress","royal"}:
            mantle = palette[faction]
    scale *= stature
    mesh = Mesh(id, kind, lambda v: add(mul(v,scale),offset), prefix, root_bone)
    bow_held = left_item in {"bow", "longbow"}
    style = human_style(coat,right_item,bow_held)
    mesh.animation = {"style":style,"equipment":left_item if bow_held else right_item}
    shoulders = 1.18 if coat in {"scale","royal","plate","harness"} else .90 if coat in {"dress", "worker"} else 1.0
    skin = "darkskin" if id in {"laman","lemuel","zerahemnah","curelom","cimeter_bearer","tubaloth"} else "skin"
    mesh.bone("torso", (0,0,.89))
    mesh.bone("head", (0,0,1.444), "torso")
    stance = .170 if shoulders > 1 else .122 if coat=="worker" else .139
    long_robe = coat in {"robe","dress","royal"}
    for sign, side in ((-1,"l"),(1,"r")):
        arm, leg, forearm, shin = (f"{joint}_{side}" for joint in ("arm","leg","forearm","shin"))
        item = right_item if side == "r" else left_item
        shoulder,elbow,grip = arm_pose(sign,shoulders,style,item)
        lead = -.055 if side == "r" and bow_held else .061 if side == "l" and bow_held else .034*sign
        knee = (lead+.011,sign*stance,.455)
        ankle = (lead-.021,sign*stance*1.06,.12)
        mesh.bone(arm, shoulder, "torso")
        mesh.bone(forearm, elbow, arm)
        mesh.bone(leg, (0,sign*stance,.845))
        mesh.bone(shin, knee, leg)
        mesh.beam("clothed_thigh", "leather" if coat in {"scale","plate"} else tunic,
                  (0,sign*stance,.87), knee,.11*shoulders,.094,leg,n=6)
        mesh.gem("covered_knee", "leather" if coat in {"scale","plate"} else tunic, knee,(.084,.093,.080),leg,n=5)
        mesh.beam("wrapped_calf", "sand" if coat in {"light","worker","harness"} else "leather",
                  knee,ankle,.087,.067,shin,n=6)
        mesh.box("sandaled_foot", "bark", (ankle[0]+.061,ankle[1],.063),(.245,.162,.126),shin,bevel=.027)
        mesh.beam("ankle_binding", "ivory" if coat=="worker" else "cedar", (ankle[0],ankle[1],.137),
                  (ankle[0],ankle[1],.209),.075,bone=shin,n=6)
        sleeve = tunic if long_robe or coat=="worker" else skin
        mesh.beam("upper_arm", sleeve, shoulder, elbow,.104*shoulders,.082,arm,n=6)
        mesh.gem("elbow_joint",sleeve,elbow,(.078,.08,.074),arm,n=5)
        mesh.beam("forearm", tunic if long_robe else skin, elbow, grip,.078,.058,forearm,n=6)
        mesh.gem("hand", skin, grip,(.071,.054,.058),forearm,n=5)
        if coat == "worker":
            mesh.beam("rolled_work_sleeve","ivory",add(elbow,mul(sub(shoulder,elbow),.12)),
                      add(elbow,mul(sub(shoulder,elbow),.31)),.089,.095,arm,n=5)
        if coat in {"scale","plate","royal","leather"}:
            mesh.loft("pauldron", "bronze" if coat!="leather" else "leather",[(0,shoulder[1],1.26,.12,.113),
                                                                               (-.009,shoulder[1],1.37,.148,.132),
                                                                               (-.011,shoulder[1],1.437,.092,.073)],arm,n=6)
            mesh.beam("vambrace","bronze",add(elbow,mul(sub(grip,elbow),.26)),
                      add(elbow,mul(sub(grip,elbow),.81)),.081,.069,forearm,n=6)
        equipped_side = "l" if bow_held else "r"
        gear_bone = forearm
        if side == equipped_side:
            mesh.bone("weapon",grip,forearm)
            mesh.socket("weapon_grip",grip,"weapon")
            gear_bone = "weapon"
        equipment(mesh,item,grip,gear_bone,mantle or tunic)
        if bow_held and side == "r":
            mesh.socket("draw_hand",add(grip,(.018,-.008,.024)),forearm)
        if bow_held and side == "l":
            mesh.socket("projectile_release",add(grip,(.045,.011,.033)),"weapon")
        if item == "sling":
            mesh.socket("projectile_release",add(grip,(.35,.035,.095)),gear_bone)

    skirt_bottom = .28 if coat == "dress" else .37 if long_robe else .68
    mesh.loft("pleated_hem",tunic,[(0,0,skirt_bottom,.198*shoulders,.275*shoulders),
                                  (-.022,0,.82,.171*shoulders,.218*shoulders),
                                  (-.009,0,1.01,.138*shoulders,.199*shoulders)],"torso",n=8)
    mesh.loft("hem_binding",mantle or "leather",[(0,0,skirt_bottom+.003,.202*shoulders,.278*shoulders),
                                                 (-.003,0,skirt_bottom+.055,.198*shoulders,.270*shoulders)],"torso",n=8,team=True)
    mesh.loft("fitted_tunic",tunic,[(0,0,.884,.156*shoulders,.204*shoulders),(-.007,0,1.123,.182*shoulders,.249*shoulders),
                                   (-.025,0,1.362,.159*shoulders,.273*shoulders),(-.02,0,1.425,.092,.096)],"torso",n=8)
    mesh.loft("waist_sash",mantle or "leather",[(-.009,0,.888,.165*shoulders,.218*shoulders),
                                               (-.016,0,.978,.163*shoulders,.217*shoulders)],"torso",n=8,team=True)
    mesh.box("waist_clasp","bronze",(.178*shoulders,0,.933),(.065,.100,.082),"torso",bevel=.01)
    mesh.beam("neck",skin,(-.004,0,1.395),(.014,0,1.522),.087,.078,"head",n=6)
    if mantle and (kind == "hero" or coat in {"dress","royal","plate","robe"} or hair in {"hood","mask"}):
        length = .77 if kind=="hero" else .51
        cape = [(-.07,-.24*shoulders,1.407),(-.22,0,1.424),(-.075,.24*shoulders,1.409),
                (-.33,-.284*shoulders,1.39-length),(-.47,.054,1.39-length-.08),(-.30,.28*shoulders,1.39-length+.03)]
        faces = [(0,3,1),(1,3,4),(1,4,5),(1,5,2)]
        mesh.part("folded_mantle",mantle,cape,faces,"torso",True)
        mesh.part("mantle_lining",mantle,[(x+.012,y,z) for x,y,z in cape],[(c,b,a) for a,b,c in faces],"torso",True)
        mesh.beam("mantle_clasp_band",mantle,(.122,-.207,1.337),(.147,.212,1.395),.047,bone="torso",n=5,team=True)
        mesh.gem("mantle_fibula","gold",(.188,.130,1.376),(.036,.057,.058),"torso",n=6)
    if coat in {"plate","scale","leather"}:
        material = "iron" if id=="shiz" else "bronze" if coat in {"plate","scale"} else "leather"
        mesh.prism("chest_armor",material,.16*shoulders,.055,[(-.218,1.01),(.164,1.01),(.242,1.24),(.167,1.361),
                                                               (-.17,1.361),(-.242,1.238)],"torso")
        if coat == "scale":
            for z in (1.065,1.145,1.227):
                for y in (-.144,-.049,.049,.144):
                    mesh.gem("lamellar_plate", "bronze" if material == "iron" else "gold",(.217*shoulders,y,z),
                             (.026,.040,.038),"torso",n=4)
        if coat == "plate":
            mesh.gem("cuirass_ridge","gold",(.218*shoulders,0,1.184),(.064,.086,.13),"torso",n=5)
    elif coat in {"worker","harness","royal","dress"}:
        if coat == "worker":
            apron = "leather" if faction == "gadianton" else mantle
            mesh.prism("working_apron",apron,.158,.034,[(-.193,.63),(.178,.63),(.191,.89),(.120,1.29),
                                                          (-.128,1.29),(-.195,.86)],"torso",True)
            mesh.box("apron_pocket","leather" if apron != "leather" else "wood",(.202,.064,.887),(.060,.167,.146),"torso")
        if coat == "harness":
            for sign in (-1,1):
                mesh.beam("chest_harness","leather",(.156,-sign*.175,.923),(.136,sign*.224,1.397),.038,
                          bone="torso",n=4,aspect=.5)
        if coat in {"royal","dress"}:
            mesh.beam("gold_torc","gold",(.139,-.183,1.338),(.156,.178,1.336),.051,bone="torso",n=5)
            mesh.gem("medallion","gold",(.195,0,1.257),(.034,.066,.071),"torso",n=7)
    if bow_held:
        start, end = (-.259,-.103,.807),(-.259,.185,1.451)
        if id == "wilderness_spy":
            start,end = (-.212,-.095,.887),(-.224,.150,1.354)
        mesh.beam("quiver", "leather",start,end,.105,.125,"torso",n=7)
        for a,b in sides(4):
            mesh.beam("quivered_arrow", "wood",add(end,(a*.055,b*.059,-.086)),add(end,(a*.060,b*.069,.22)),.013,bone="torso",n=4)
            mesh.gem("arrow_fletching", "ivory",add(end,(a*.061,b*.067,.18)),(.028,.022,.064),"torso",n=4)
        if id == "wilderness_spy":
            equipment(mesh,"horn",(-.178,-.196,.915),"torso",mantle)
    if left_item in {"cargo","bedroll"}:
        mesh.box("backpack", "leather", (-.264,0,1.09),(.31,.33,.47),"torso",bevel=.055)
        mesh.beam("rolled_blanket", "ivory",(-.33,-.25,1.34),(-.33,.25,1.34),.095,bone="torso",n=6)
        if left_item == "cargo":
            jar(mesh,"trade_cargo",(-.26,-.192,.87),.103,.34,bone="torso")
            jar(mesh,"trade_cargo",(-.28,.199,.86),.111,.30,"teal","torso")
    head(mesh,hair,skin,shoulders)
    return mesh


ANIMALS = {
    # Half length/depth, body height, shoulder height, coat, underside, hoof.
    "curelom": (1.10,.55,.63,1.23,"earth","darkclay","bark"),
    "cumelom": (1.16,.43,.48,1.49,"ochre","sand","bark"),
    "elephant": (1.33,.73,.74,1.68,"graphite","horizon","iron"),
    "sheep": (.59,.38,.33,.74,"wool","ivory","bark"),
    "wild_game": (.77,.27,.41,1.04,"cedar","sand","bark"),
    "horse": (.92,.34,.50,1.22,"clay","sand","bark"),
}


def animal(id, kind, offset=(0,0,0), scale=1, prefix="", root_bone="root", color=None):
    length, width, depth, shoulder, hide, pale, hoof = ANIMALS[id]
    hide = color or hide
    mesh = Mesh(id,kind,lambda vertex:add(mul(vertex,scale),offset),prefix,root_bone)
    mesh.animation = {"style":"beast", "equipment":""}
    mesh.bone("torso", (0,0,shoulder*.94))
    mesh.bone("tail", (-length*.84,0,shoulder+depth*.32),"torso")
    neck = (length*.58,0,shoulder+depth*.26)
    mesh.bone("head",neck,"torso")
    mesh.loft("ribcage",hide,[(-length*.11,0,shoulder-depth*.53,length*.69,width*.71),
                              (-length*.045,0,shoulder-depth*.16,length*.95,width*.97),
                              (-length*.014,0,shoulder+depth*.27,length,width),
                              (.09*length,0,shoulder+depth*.76,length*.81,width*.75),
                              (.14*length,0,shoulder+depth*.94,length*.51,width*.51)],"torso",n=9)
    if id in {"horse","wild_game","cumelom"}:
        neck_height = .64 if id == "cumelom" else .45 if id=="horse" else .39
        poll = (length*.95,0,shoulder+depth*.35+neck_height)
        mesh.beam("neck",hide,neck,poll,width*.73,width*.46,"head",n=7,aspect=.84)
        muzzle = (poll[0]+length*.27,0,poll[2]-.23)
        mesh.beam("jaw",hide,poll,muzzle,width*.53,width*.33,"head",n=7)
        mesh.gem("muzzle",pale,muzzle,(width*.48,width*.39,width*.36),"head",n=6)
        if id == "horse":
            mesh.beam("mane","brownhair",add(neck,(-.135,0,.155)),add(poll,(-.116,0,.105)),.095,.088,"head",n=5)
            mesh.beam("nose_blaze","ivory",add(poll,(.10,0,-.01)),add(muzzle,(.026,0,.073)),.047,.035,"head",n=4,aspect=.49)
    else:
        poll = (length*.94,0,shoulder+depth*.25)
        muzzle = (length*1.21,0,shoulder-depth*.12)
        mesh.beam("neck",hide,neck,poll,width*.83,width*.70,"head",n=7)
        mesh.gem("head_mass",hide,poll,(length*.36,width*.65,depth*.54),"head",n=8)
        if id != "elephant":
            mesh.beam("muzzle",pale,poll,muzzle,width*.45,width*.31,"head",n=6)
    for sign in (-1,1):
        eye = (poll[0]+.08,poll[1]+sign*width*.48,poll[2]+depth*.078)
        mesh.gem("eye", "ink", eye, (.049,.035,.043),"head",n=5)
        if id == "elephant":
            center = add(poll,(-.19,sign*.41,.06))
            mesh.gem("scalloped_ear","horizon",center,(.30,.275,.41),"head",n=7,skew=.26)
            path(mesh,"tusk","ivory",[add(muzzle,(-.032,sign*.21,-.045)), add(muzzle,(.22,sign*.25,-.24)),
                                     add(muzzle,(.47,sign*.29,-.21)),add(muzzle,(.62,sign*.29,-.04))],.106,"head",n=6,
                 widths=[1,.71,.38,.048])
        else:
            start = add(poll,(-.10,sign*width*.30,.06))
            end = add(start,(-.02,sign*width*(.55 if id in {"curelom","sheep"} else .30),.12 if id=="sheep" else .24))
            mesh.beam("ear",hide,start,end,width*.20,width*.04,"head",n=5)
        if id == "curelom":
            start = add(poll,(-.13,sign*.245,.19))
            path(mesh,"horn","ivory",[start,add(start,(-.11,sign*.28,.09)),add(start,(.01,sign*.45,.28))],.118,"head",
                 n=6,widths=[1,.67,.043])
        if id == "wild_game":
            start = add(poll,(-.06,sign*.15,.11))
            tip = add(start,(-.31,sign*.21,.46))
            path(mesh,"antler","ivory",[start,add(start,(-.11,sign*.12,.22)),tip],.048,"head",widths=[1,.68,.11])
            path(mesh,"antler_tine","ivory",[add(start,(-.15,sign*.14,.27)),add(start,(.05,sign*.22,.44)),
                                             add(start,(.09,sign*.23,.53))],.039,"head",widths=[1,.5,.10])
    if id == "elephant":
        path(mesh,"trunk",hide,[add(muzzle,(.035,0,.13)),add(muzzle,(.244,0,-.09)),add(muzzle,(.29,0,-.48)),
                                add(muzzle,(.37,0,-.75)),add(muzzle,(.56,0,-.76)),add(muzzle,(.62,0,-.66))],
             .199,"head",n=7,widths=[1.23,1.02,.81,.55,.40,.32])
    if id == "curelom":
        mesh.gem("shaggy_mane","bark",(length*.54,0,shoulder+depth*.18),(.441,width*.96,.63),"torso",n=8,skew=.25)
        for sign in (-1,1):
            for x in (.43,.72,.95):
                mesh.gem("mane_tuft","earth",(x,sign*.38,shoulder-.10),(.18,.13,.29),"torso",n=5,skew=-.16)
    if id == "cumelom":
        for x in (-.47,.39):
            mesh.loft("pack_hump","sand",[(x,0,shoulder+.27,.42,.35),(x+.05,0,shoulder+.57,.31,.27),
                                          (x-.035,0,shoulder+.82,.18,.12)],"torso",n=7)
    if id == "sheep":
        for i, (x,y,z) in enumerate(((-.48,0,.92),(-.18,-.24,.94),(-.17,.23,.97),(.17,0,1.00),(.36,-.20,.89),(.36,.20,.94))):
            mesh.gem("wool_tuft","wool" if i%2 else "white",(x,y,z),(.27,.20,.21),"torso",n=6)
    positions = [(length*.59,"arm"),(-length*.66,"leg")]
    if id == "cumelom":
        positions.insert(1,(-.05,"leg_mid"))
    for index, (x, label) in enumerate(positions):
        for sign, side in ((-1,"l"),(1,"r")):
            y = sign*width*(.57 if id != "elephant" else .67)
            thigh = (x,y,shoulder+.01)
            leg = f"{label}_{side}"
            mesh.bone(leg,thigh,"torso")
            knee = (x- (.15*length if index==0 else -.19*length),y,shoulder*.48)
            ankle = (x+sign*.064,y+sign*.027,.117 if id=="elephant" else .095)
            lower = f"shin_{'front' if label=='arm' else 'mid' if label=='leg_mid' else 'back'}_{side}"
            mesh.bone(lower,knee,leg)
            radius = .25 if id=="elephant" else .114 if id=="sheep" else .126 if id in {"wild_game","horse","cumelom"} else .187
            mesh.beam("upper_leg",hide,thigh,knee,radius*1.28,radius*.82,leg,n=6)
            mesh.beam("lower_leg",hide,knee,ankle,radius*.87,radius*.68,lower,n=6)
            mesh.loft("hoof",hoof,[(ankle[0]+.022,ankle[1],0,radius*.98,radius*.81),
                                  (ankle[0]+.006,ankle[1],ankle[2]+.031,radius*.77,radius*.68)],lower,n=6)
    tail = [(-length*.86,0,shoulder+depth*.32),(-length*1.10,0,shoulder+depth*.12),
            (-length*1.25,.073,shoulder*.68)]
    if id != "sheep":
        path(mesh,"tail",hide,tail,.088 if id=="horse" else .064,"tail",n=6,widths=[1.18,.91,.51])
        mesh.gem("tail_tuft", "brownhair" if id=="horse" else hoof, tail[-1],(.095,.08,.16),"tail",n=5,skew=.36)
    else:
        mesh.gem("wool_tail","white",(-length*.96,0,shoulder+.21),(.17,.17,.16),"tail",n=6)
    if id in {"curelom","cumelom","elephant"}:
        z = shoulder+depth*.79
        blanket = {"curelom":"ochre","cumelom":"wine","elephant":"red"}[id]
        mesh.loft("woven_blanket",blanket,[(-.16,0,z-.27,length*.47,width*.98),(-.16,0,z+.136,length*.49,width*.62)],
                  "torso",cut=.11,team=True)
        for sign in (-1,1):
            mesh.box("blanket_border","gold",(-.15,sign*width*.885,z-.16),(length*.89,.042,.069),"torso")
        if id=="elephant":
            mesh.box("howdah_platform","wood",(-.07,0,z+.17),(1.37,1.03,.13),"torso",bevel=.033)
            for x in (-.65,.50):
                for y in (-.46,.46):
                    mesh.beam("howdah_post","cedar",(x,y,z+.19),(x,y,z+.67),.075,bone="torso",n=5)
            for sign in (-1,1):
                mesh.box("howdah_side","cedar",(-.074,sign*.45,z+.44),(1.18,.092,.28),"torso",bevel=.018)
            mesh.beam("cargo_spindle","bronze",(-.65,-.48,z+.66),(-.65,.48,z+.66),.055,bone="torso",n=5)
        elif id=="cumelom":
            for sign in (-1,1):
                basket(mesh,"pack_pannier",(-.12,sign*.48,z-.32),.24,.41,bone="torso")
        else:
            for sign in (-1,1):
                mesh.beam("pack_frame","wood",(-.53,sign*.41,z-.07),(.53,sign*.35,z+.12),.05,bone="torso",n=4)
    if id=="horse":
        mesh.beam("harness_belt","leather",(poll[0]+.022,-width*.49,poll[2]-.105),(poll[0]+.022,width*.49,poll[2]-.105),
                  .035,bone="head",n=5)
        mesh.loft("horse_collar","leather",[(length*.32,0,shoulder-.18,.09,width*.88),
                                               (length*.52,0,shoulder+.31,.115,width*.80)],"torso",n=7,team=True)
    return mesh


def chariot(id):
    mesh = Mesh(id,"chariot")
    mesh.animation = {"style":"chariot", "equipment":""}
    for side, y, color in (("l",-.53,"clay"),("r",.53,"darkclay")):
        horse = animal("horse", "beast", offset=(1.08,y,0), scale=.96, prefix=f"horse_{side}_",color=color)
        mesh.insert(horse, f"horse_{side}_")
    mesh.box("chariot_deck","cedar",(-.96,0,.578),(1.25,1.41,.128),bevel=.045)
    mesh.loft("bronze_breastwork","bronze",[(-.48,0,.56,.16,.70),(-.48,0,.75,.19,.72),
                                              (-.41,0,1.02,.13,.52)],cut=.22)
    for sign, side in ((-1,"l"),(1,"r")):
        x,y,z = (-1.09,sign*.82,.54)
        bone = f"wheel_{side}"
        mesh.bone(bone,(x,y,z))
        wheel_outline = sides(12)
        rim_rings = [(y+.052,.526),(y-.052,.526),(y-.052,.434),(y+.052,.434)]
        vertices = [(x+a*r, yy, z+b*r) for yy,r in rim_rings for a,b in wheel_outline]
        faces = []
        for ring in range(4):
            for i in range(12):
                left, right = ring*12, ((ring+1)%4)*12
                j = (i+1)%12
                faces.extend(((left+i,right+j,right+i),(left+i,left+j,right+j)))
        mesh.part("bronze_wheel_rim","bronze",vertices,faces,bone)
        for i,(a,b) in enumerate(wheel_outline):
            if i%2==0:
                mesh.beam("wheel_spoke","wood",(x,y,z),(x+a*.467,y,z+b*.467),.046,bone=bone,n=4)
        mesh.beam("wheel_hub","gold",(x,y-.135,z),(x,y+.135,z),.115,bone=bone,n=7)
        mesh.beam("chariot_shaft","wood",(-1.51,sign*.61,.65),(1.12,sign*.58,1.16),.069,.051,n=5)
        mesh.beam("chariot_rail","bronze",(-1.52,sign*.571,.84),(-.51,sign*.58,1.04),.049,n=6)
        mesh.beam("draft_rein","leather",(-.70,sign*.22,1.63),(1.89,sign*.53,1.65),.013,n=4)
    driver = person("chief_captain","chariot",offset=(-1.07,0,.62),scale=.82,prefix="driver_")
    mesh.insert(driver,"driver_")
    return mesh


def vessel(id, faction):
    mesh = Mesh(id,"ship")
    mesh.animation = {"style":"ship", "equipment":""}
    mesh.bone("hull",(0,0,.63))
    accent = PALETTES[faction or "nephites"]["accent"]
    barge = id=="jaredite_barge"
    x_length = 2.38 if barge else 2.70
    x_positions = (-x_length,-x_length*.74,-x_length*.36,0,x_length*.36,x_length*.74,x_length)
    hull_widths = (.12,.61,.96,1,.96,.61,.12)
    hull_heights = (.49,.75,.94,1,.94,.75,.54)
    hull_shape = [(0,-.49),(.53,-.41),(.86,-.20),(1,.12),(.93,.28),(-.93,.28),(-1,.12),(-.86,-.20),(-.53,-.41)]
    rings = [[(x,y*w*(.96 if barge else 1.15),.63+z*h+abs(x/x_length)**2*.36) for y,z in hull_shape]
             for x,w,h in zip(x_positions,hull_widths,hull_heights)]
    mesh.rings("sewn_cedar_hull","cedar",rings)
    for sign in (-1,1):
        for idx,(offset,color) in enumerate(((.19,"wood"),(.025,"bark"),(-.11,"wood"))):
            points = [(x,sign*w*(.94 if barge else 1.13)*(1-.105*idx),.63+offset*h+abs(x/x_length)**2*.36)
                      for x,w,h in zip(x_positions,hull_widths,hull_heights)]
            path(mesh,"strake",color,points,.038,n=5,widths=[.5,.9,1,1,1,.9,.5])
        path(mesh,"woven_gunwale",accent,[(x,sign*w*(.96 if barge else 1.15),.63+.28*h+abs(x/x_length)**2*.36)
                                          for x,w,h in zip(x_positions,hull_widths,hull_heights)],.050,n=5)
    if barge:
        roof_shape = [(0,-.05),(.93,.08),(.82,.43),(.48,.65),(0,.76),(-.48,.65),(-.82,.43),(-.93,.08)]
        rooftops = [[(x,y*w,.83+z*h+abs(x/x_length)**2*.28) for y,z in roof_shape]
                    for x,w,h in zip(x_positions,hull_widths,hull_heights)]
        mesh.rings("vaulted_waterproof_cover","wood",rooftops)
        for i in (1,2,3,4,5):
            x = x_positions[i]
            points = [(x,y*hull_widths[i]*1.025,.855+z*hull_heights[i]+abs(x/x_length)**2*.28) for y,z in roof_shape[1:]]
            path(mesh,"cover_rib","bark",points,.053,n=5)
        mesh.box("watertight_hatch","cedar",(.28,0,1.64),(.67,.57,.14),bevel=.038)
        mesh.box("hatch_hinge","bronze",(.09,0,1.713),(.109,.59,.028))
        mesh.gem("glowing_pilot_stone","glowing",(1.65,0,1.34),(.11,.18,.15),n=7)
    else:
        mesh.beam("mast","wood",(-.17,0,.79),(-.21,0,3.92),.079,.042,n=7)
        mesh.beam("yard","bark",(-.20,-1.24,3.55),(-.20,1.24,3.56),.050,n=6)
        for side in (-1,1):
            mesh.beam("bound_stay","leather",(-.22,0,3.83),(1.65*side,side*.64,1.00),.018,n=4)
        sail = [(-.21,-1.10,3.56),(-.16,0,3.59),(-.21,1.11,3.56),
                (.24,-1.22,2.66),(.49,0,2.54),(.24,1.22,2.66),
                (-.05,-.95,1.77),(.21,0,1.64),(-.05,.96,1.77)]
        faces = [(0,3,1),(1,3,4),(1,4,5),(1,5,2),(3,6,7),(3,7,4),(4,7,5),(5,7,8)]
        mesh.bone("sail",(-.20,0,3.56),"hull")
        mesh.part("bellying_sail","linen",sail,faces,"sail")
        mesh.part("sail_reverse","ivory",[(x-.015,y,z) for x,y,z in sail],[(c,b,a) for a,b,c in faces],"sail")
        path(mesh,"sail_border",accent,[add(sail[i],(.014,0,.014)) for i in (0,3,6,7,8,5,2)],.035,"sail",n=5)
        for sign in (-1,1):
            oar = "oar_l" if sign<0 else "oar_r"
            mesh.bone(oar,(-1.35,sign*.74,1.15),"hull")
            mesh.beam("steering_oar","wood",(-1.35,sign*.74,1.15),(-2.39,sign*1.27,.37),.055,bone=oar,n=5)
            mesh.box("oar_blade","cedar",(-2.34,sign*1.26,.38),(.31,.17,.077),oar,bevel=.02)
        for x,y in ((.8,.43),(.91,-.51),(-.79,-.26),(-.77,.44)):
            mesh.box("deck_cargo","wood",(x,y,1.01),(.37,.30,.30),bevel=.036)
            mesh.box("cargo_bond","leather",(x,y,1.016),(.095,.32,.315))
        mesh.beam("carved_prow","cedar",(2.13,0,.83),(2.69,0,1.48),.119,.076,n=6)
        mesh.gem("prow_cap","gold",(2.70,0,1.48),(.14,.092,.13),n=5,skew=.45)
    # Hull roll carries fixed fittings while sail and oars keep their own joints.
    for part in mesh.parts:
        if part["bone"]=="root":
            part["bone"]="hull"
    return mesh


PALETTES = {
    "nephites": {"wall":"cement","trim":"ivory","roof":"teal","base":"sand","accent":"teal","beam":"cedar","roof_type":"ridge"},
    "lamanites": {"wall":"clay","trim":"amber","roof":"rust","base":"darkclay","accent":"saffron","beam":"bark","roof_type":"shed"},
    "gadianton": {"wall":"rock","trim":"moss","roof":"forest","base":"earth","accent":"plum","beam":"bark","roof_type":"tent"},
    "coriantumr": {"wall":"cement","trim":"stone","roof":"horizon","base":"rock","accent":"ochre","beam":"iron","roof_type":"flat"},
    "shiz": {"wall":"rock","trim":"sand","roof":"iron","base":"graphite","accent":"wine","beam":"bark","roof_type":"spire"},
    "laban": {"wall":"linen","trim":"sand","roof":"wine","base":"ivory","accent":"plum","beam":"bronze","roof_type":"hip"},
}


class Architecture:
    def __init__(self, mesh, faction):
        self.m = mesh
        self.faction = faction
        self.p = PALETTES[faction]

    def plinth(self, x, y, z, w, d, h=.22, trim=True, *, base_cut=.13, trim_cut=.10):
        self.m.loft("dressed_foundation",self.p["base"],[(x,y,z,w*.50,d*.50),(x,y,z+h*.38,w*.50,d*.50),
                                                          (x,y,z+h,w*.473,d*.473)],cut=base_cut)
        if trim:
            self.m.loft("foundation_course",self.p["trim"],[(x,y,z+h*.67,w*.483,d*.483),
                                                            (x,y,z+h+.004,w*.483,d*.483)],cut=trim_cut)

    def column(self, x, y, z, height, width=.17, decorated=True, color=None):
        self.m.loft("column",color or self.p["wall"],[(x,y,z,width*.73,width*.73),
                                                       (x,y,z+height*.77,width*.61,width*.61),
                                                       (x,y,z+height,width*.67,width*.67)],n=6)
        if decorated:
            self.m.box("pillar_foot",self.p["trim"],(x,y,z+.079),(width*1.95,width*1.95,.158))
            self.m.box("pillar_capital",self.p["trim"],(x,y,z+height-.044),(width*1.88,width*1.88,.115))

    def door(self, x, y, z, width, height, surround=True):
        profile = [(y-width*.50,z),(y+width*.50,z),(y+width*.50,z+height*.80),
                   (y+width*.32,z+height),(y-width*.32,z+height),(y-width*.50,z+height*.80)]
        self.m.prism("shadowed_doorway","ink",x,.035,profile)
        if surround:
            for sign in (-1,1):
                self.m.box("door_jamb",self.p["trim"],(x+.018,y+sign*width*.53,z+height*.45),(.17,.164,height*.90),bevel=.017)
            self.m.box("door_lintel",self.p["trim"],(x+.029,y,z+height),(.20,width*1.26,.16),bevel=.02)

    def stairs(self, x, y, z, width, rise, length, count=5):
        for step in range(count):
            depth = length/count + .015
            high = rise * (1 - step / count)
            self.m.box("entry_tread",self.p["trim"] if step%2==0 else self.p["wall"],
                       (x+(step+.5)*length/count,y,z+high/2),(depth,width,high))

    def roof(self, x, y, z, w, d, h, kind=None, color=None, seams=True):
        kind = kind or self.p["roof_type"]
        color = color or self.p["roof"]
        rx, ry = w/2, d/2
        if kind == "ridge":
            profile = [(y-ry,z),(y+ry,z),(y+.045,z+h)]
            self.m.prism("gable_roof",color,x,rx,profile)
            self.m.beam("roof_ridge",self.p["trim"],(x-rx-.043,y+.045,z+h),(x+rx+.043,y+.045,z+h),.047,n=5)
            self.m.prism("gable_inlay", self.p["trim"],x+rx+.014,.019,
                         [(y-ry*.69,z+.04),(y+ry*.69,z+.04),(y+.032,z+h*.72)],team=True)
        elif kind in {"hip","spire","tent"}:
            crown_y = ry*(.46 if kind=="hip" else .19 if kind=="tent" else .17)
            crown_x = rx*(.47 if kind=="hip" else .67 if kind=="tent" else .25)
            self.m.loft("hipped_roof" if kind == "hip" else "angled_roof",color,
                        [(x,y,z,rx,ry),(x-.05,y,z+h*.70,rx*.68,ry*.64),(x-.12,y,z+h,crown_x,crown_y)],cut=.13)
            if kind == "hip" and seams:
                for xoff in (-.47,-.26,0,.26,.47):
                    path(self.m,"terracotta_standing_seam",self.p["accent"],
                         [(x+xoff*rx,y-ry-.017,z+.015), (x-.05+xoff*rx*.68,y-ry*.64-.017,z+h*.70+.015),
                          (x-.12+xoff*crown_x,y-crown_y-.017,z+h+.015)],.022,n=4)
            elif kind == "tent":
                for dx in (-rx*.70,rx*.70):
                    for dy in (-ry*.82,ry*.82):
                        self.m.beam("tent_guy",self.p["beam"],(x+dx,y+dy,z),(x+dx*.60,y+dy*.25,z+h*.90),.036,n=4)
        elif kind == "shed":
            self.m.prism("leaning_roof",color,x,rx,[(y-ry,z+.04),(y+ry,z+h*.43),(y+ry,z+h*.69),(y-ry,z+h*.24)])
            self.m.beam("roof_timber",self.p["beam"],(x-rx-.1,y-ry,z+.09),(x+rx+.1,y-ry,z+.09),.058,n=5)
        elif kind == "flat":
            self.m.loft("battered_roof",color,[(x,y,z,rx,ry),(x,y,z+h*.62,rx*.93,ry*.91),
                                               (x-.025,y,z+h*.81,rx*.81,ry*.75)],cut=.18)
            for dx in (-.67,.67):
                self.m.box("parapet_block",self.p["trim"],(x+dx*rx,y,z+h*.71),(w*.14,d*.81,h*.43),bevel=.027)
        else:
            raise ValueError(f"Unknown roof {kind}")
        edge = self.p["beam"] if kind=="tent" else self.p["trim"]
        self.m.loft("fascia",edge,[(x,y,z-.09,rx*.976,ry*.966),(x,y,z+.035,rx*.99,ry*.99)],cut=.11)

    def room(self, x, y, z, w, d, h, roof=.45, door=True, style=None, body=None):
        style = style or self.p["roof_type"]
        levels = [(x,y,z,w*.49,d*.49),(x-.023,y,z+h,w*(.40 if style in {"flat","shed","spire"} else .475),
                                                   d*(.415 if style in {"flat","shed","spire"} else .475))]
        self.m.loft("masonry_mass",body or self.p["wall"],levels,cut=.11 if style=="hip" else .19)
        if door:
            self.door(x+w*.48,y,z,w*.24 if d>w else d*.29,h*.68)
        if roof:
            self.roof(x-.026,y,z+h,w*1.12,d*1.12,roof,style)

    def banners(self,x,y,z,width=1.5, height=.62):
        for side in (-1,1):
            yy = y+side*width*.50
            outline = [(yy-.15,z-height*.77),(yy,z-height),(yy+.15,z-height*.77),(yy+.15,z),(yy-.15,z)]
            self.m.prism("woven_hanging",self.p["accent"],x,.025,outline,team=True)
            self.m.beam("hanging_batten",self.p["beam"],(x,yy-.21,z+.012),(x,yy+.21,z+.012),.030,n=4)

    def flag(self, x,y,z,height=.89):
        self.m.beam("banner_bough",self.p["beam"],(x,y,z),(x,y,z+height),.043,.029,n=5)
        flag(self.m,"banner_cloth", self.p["accent"],x,y,z+height,.56,.36)
        self.m.gem("banner_finial","gold",(x,y,z+height+.045),(.048,.049,.071),n=5)


def citadel(a):
    m, f, p = a.m, a.faction, a.p
    if f == "gadianton":
        a.plinth(-.12,0,0,3.9,3.9,.18,False)
        for i, (x,y,size) in enumerate(((-1,0,1.4),(-.52,-1.12,1.09),(-.84,1.17,1.08),(-1.49,.72,.72))):
            m.gem("covered_boulder", "rock" if i%2 else "earth",(x,y,.48),(size,.71,.60),n=7,skew=.33)
        a.room(-.15,0,.13,3.61,3.44,1.16,.76,style="tent",body="darkclay")
        a.roof(.96,0,.96,1.75,2.72,.60,"tent","moss")
        for y in (-1.06,1.06):
            a.column(1.66,y,.13,.88,.111,False,"bark")
        a.banners(1.56,0,.92,1.5,.38)
    elif f == "lamanites":
        a.plinth(-.23,0,0,4.22,4.54,.42)
        a.stairs(1.79,0,0,1.53,.43,.96,4)
        a.plinth(-.44,0,.42,3.62,3.88,.47)
        a.room(-.75,0,.89,2.81,2.53,1.26,.48,style="shed")
        for y in (-1.47,1.47):
            m.loft("tapered_flank","clay",[(-.55,y,.39,1.25,.48),(-.68,y,1.71,.65,.32)],cut=.29)
            a.flag(-.63,y,1.72,.68)
        a.stairs(.58,0,.42,1.18,.49,1.09,4)
        a.banners(.64,0,1.92,1.59,.69)
    elif f == "coriantumr":
        a.plinth(-.05,0,0,4.95,4.76,.31)
        a.room(-.39,0,.31,4.17,4.0,1.47,.34,style="flat")
        a.room(-1.02,0,1.90,2.59,2.97,1.16,.36,False,"flat")
        for y in (-1.62,1.62):
            m.loft("massive_buttress",p["base"],[(1.37,y,.27,.72,.48),(1.10,y,1.88,.47,.32)],cut=.15)
            m.box("buttress_band",p["accent"],(1.32,y,1.09),(.82,.75,.17),team=True)
        a.stairs(1.51,0,0,1.51,.32,1.11,4)
        a.flag(-1.29,0,3.36,.78)
    elif f == "shiz":
        a.plinth(-.06,0,0,4.55,4.16,.28)
        a.room(-.55,0,.28,3.50,2.73,1.87,.95,style="spire")
        for y in (-1.39,1.39):
            a.room(.30,y,.29,2.33,1.30,1.23,.56,False,"spire")
            m.beam("front_spike",p["trim"],(1.35,y,1.12),(1.68,y,2.09),.128,.019,n=5)
        a.stairs(1.21,0,0,1.4,.30,1.23,4)
        a.banners(1.20,0,1.99,1.73,.89)
        a.flag(-.46,0,3.1,.70)
    elif f == "laban":
        a.plinth(-.10,0,0,4.24,4.09,.30)
        a.room(-.66,0,.30,3.08,3.62,1.51,.55,style="hip")
        a.room(-.89,0,1.81,1.85,2.19,.88,.43,False,"hip")
        for y in (-1.48,-.59,.59,1.48):
            a.column(1.53,y,.30,1.43,.16)
        a.roof(1.13,0,1.75,1.47,3.65,.43,"hip")
        a.stairs(1.40,0,0,1.41,.31,1.04,4)
        a.banners(1.48,0,1.67,1.50,.56)
        for y in (-1.37,1.37):
            jar(m,"courtyard_amphora",(2.06,y,0),.20,.50,"clay",trim="gold")
    else:
        a.plinth(-.10,0,0,4.70,4.49,.32)
        a.room(-.63,0,.32,3.32,3.14,1.78,.62,style="ridge")
        for y in (-1.52,1.52):
            a.room(-.91,y,.32,2.51,1.17,1.13,.34,False,"flat")
            a.column(1.57,y,.31,1.40,.185)
        a.roof(1.24,0,1.72,1.29,3.31,.37,"flat")
        a.stairs(1.51,0,0,1.55,.33,.97,4)
        a.banners(1.58,0,1.63,1.66,.66)
        a.flag(-.84,0,2.65,.73)


def shrine(a):
    m, f, p = a.m, a.faction, a.p
    if f == "lamanites":
        a.plinth(-.24,0,0,4.43,4.06,.29)
        m.loft("rameumptom_shaft",p["wall"],[(-.43,0,.25,1.32,1.16),(-.48,0,1.65,.99,.92),
                                               (-.57,0,2.62,.81,.72)],cut=.25)
        a.stairs(.42,0,0,1.25,2.53,2.10,9)
        a.plinth(-.42,0,2.62,2.58,2.29,.29)
        for y in (-.87,.87):
            a.column(-.44,y,2.84,.53,.17,False,p["base"])
            m.beam("open_balustrade",p["trim"],(-1.55,y,3.19),(.64,y,3.19),.088,n=5)
        m.loft("exposed_prayer_dais",p["accent"],[(-.64,0,2.86,.42,.57),(-.63,0,3.24,.40,.49)],cut=.26,team=True)
        a.banners(.16,0,2.73,1.54,.70)
        a.flag(-1.33,.78,3.19,.82)
    elif f == "laban":
        a.plinth(-.03,0,0,4.60,4.65,.38,base_cut=.34,trim_cut=.34)
        floors = ((.38,4.86,4.90,1.30),(1.83,4.48,4.66,1.38),(3.36,3.72,4.08,1.30))
        for z,w,d,h in floors:
            ground = z < .5
            a.plinth(-.28,0,z,w,d*(.96 if ground else 1),.18,False,base_cut=.33 if ground else .13)
            a.room(-.63 if ground else -.68,0,z+.17,w*.68,d*(.82 if ground else .89),h,0,ground,"hip")
            outer = d*(.34 if ground else .39)
            for y in (-outer,-d*.13,d*.13,outer):
                a.column(w*(.31 if ground else .33),y,z+.16,h,.181,decorated=ground)
            for x in (-w*.30,-w*.09):
                m.box("palace_side_opening","darkclay",(x,-d*.425,z+.88),(.245,.055,.41))
                m.box("palace_sill","ivory",(x,-d*.425,z+.67),(.35,.100,.059))
            a.roof(.22,0,z+h+.15,w*.85,d*1.01,.36 if z<3 else .76,"hip",seams=z > 3)
            a.banners(w*.41,0,z+h+.09,d*.38,.63)
        a.stairs(1.91,0,0,2.05,.56,.66,5)
        for y in (-1.98,1.98):
            a.flag(1.60,y,3.44,.72)
    elif f in {"coriantumr","shiz"}:
        a.plinth(-.05,0,0,4.66,4.70,.32)
        for z,w,d,h in ((.32,4.08,4.13,.67),(.99,3.50,3.53,.67),(1.66,2.84,2.91,.65)):
            a.plinth(-.26,0,z,w,d,h,True)
        a.stairs(.92,0,0,1.35,2.31,1.74,10)
        a.room(-.63,0,2.31,1.65,2.26,.92,.50,False,"flat" if f=="coriantumr" else "spire")
        m.box("sanctuary_door","ink",(.199,0,2.69),(.07,1.07,.72))
        for y in (-.63,.63):
            m.gem("glowing_foundation_stone","glowing",(.36,y,2.76),(.18,.15,.20),n=6)
            a.column(.25,y,2.30,.60,.15,False,p["trim"])
        a.banners(.32,0,3.28,1.85,.53)
    else:
        a.plinth(-.16,0,0,4.62,4.54,.38)
        a.plinth(-.31,0,.38,3.84,3.97,.35)
        a.room(-.77,0,.73,2.63,3.05,1.50,.56,style="ridge")
        for y in (-1.33,-.72,.72,1.33):
            a.column(1.03,y,.73,1.47,.165)
        a.roof(.73,0,2.22,1.69,3.48,.51,"ridge")
        a.stairs(1.10,0,0,1.75,.75,1.47,7)
        a.banners(1.64,0,2.21,1.60,.68)


def housing(a):
    m, p, f = a.m, a.p, a.faction
    if f in {"nephites","gadianton","lamanites"}:
        a.plinth(-.12,0,0,2.38,2.42,.13,False)
        if f=="lamanites":
            a.room(-.27,0,.13,1.81,1.94,.66,.72,style="shed")
        else:
            a.room(-.25,-.30,.12,1.71,1.51,.52,.78,style="tent",body="earth" if f=="gadianton" else "ochre")
            a.roof(-.57,.64,.40,1.37,.94,.49,"tent",p["accent"])
            a.column(.02,.97,.08,.36,.073,False,p["beam"])
        jar(m,"household_water",(.79,.69,.02),.17,.43)
        a.banners(.71,-.29,.61,1.05,.32)
    else:
        a.plinth(-.08,0,0,2.47,2.38,.18)
        a.room(-.34,0,.18,1.81,1.87,1.13,.41,style=p["roof_type"])
        if f=="laban":
            a.roof(.60,.19,1.09,1.40,1.58,.27,"hip")
            for y in (-.42,.71):
                a.column(1.04,y,.19,.91,.123)
            jar(m,"domestic_crock",(1.04,-.71,.01),.14,.36)
        else:
            m.box("vented_flue",p["trim"],(-.82,.57,1.66),(.32,.31,.60),bevel=.032)
            a.banners(.59,0,1.07,.83,.42)


def crops(a):
    m, f, p = a.m, a.faction, a.p
    a.plinth(0,0,0,7.06,6.68,.075,False)
    m.box("loam_patch","soil",(0,0,.09),(6.84,6.44,.12),bevel=.037)
    grape = f=="laban"
    for i,y in enumerate((-2.60,-1.57,-.55,.56,1.61,2.65)):
        m.loft("raised_planting_row","earth",[(0,y,.15,3.11,.416),(0,y,.26,2.97,.31)],cut=.13)
        if grape:
            for x in (-2.86,-.97,.97,2.86):
                m.beam("vineyard_stake",p["beam"],(x,y,.19),(x-.03,y,.92),.055,n=4)
            m.beam("trellis_wire","wood",(-2.96,y,.68),(2.96,y,.68),.020,n=4)
            for j,x in enumerate((-2.58,-1.72,-.86,.02,.87,1.74,2.55)):
                offset = .091 if j%2 else -.073
                m.gem("grape_canopy","moss" if j%2 else "forest",(x,y+offset,.64),(.36,.255,.25),n=5,skew=.45)
                m.gem("grape_cluster","grape",(x+.13,y+.17,.49),(.13,.115,.13),n=5)
        else:
            for j,x in enumerate((-2.63,-1.77,-.87,.03,.86,1.76,2.63)):
                tilt = -.13 if (j+i)%2 else .10
                height = .61+ .06*((j+i)%3)
                m.gem("bundle_of_grain","goldgrain" if j%2 else "amber",(x,y+tilt*.41,height),(.21,.152,.33),n=5,skew=tilt*2.6)
                m.beam("braided_grain_stem","ochre",(x-.031,y,.26),(x+tilt,y+tilt*.42,height+.062),.043,.019,n=4)
    for sign in (-1,1):
        m.box("irrigation_kerb",p["trim"],(0,sign*3.24,.148),(6.80,.12,.14))
        basket(m,"harvest_basket",(3.15,sign*1.03,.16),.27,.31,"ochre")
    for y in (-2.95,2.95):
        a.flag(-3.19,y,.15,.95)


def flock(a):
    m,p,f = a.m,a.p,a.faction
    a.plinth(0,0,0,6.94,6.41,.12,False)
    for x in (-3.19,-1.65,0,1.64,3.18):
        for y in (-2.86,2.86):
            a.column(x,y,.07,.87,.127,False,p["beam"])
    for y in (-2.86,2.86):
        for h in (.34,.66):
            m.beam("corral_rail",p["beam"],(-3.22,y,h),(3.22,y,h),.092,n=5)
    for x in (-3.19,3.18):
        a.column(x,-.99,.09,.93,.127,False,p["beam"])
        a.column(x,.99,.09,.93,.127,False,p["beam"])
        for y in ((-2.86,-.97),(.97,2.86)) if x>0 else ((-2.86,2.86),):
            for h in (.34,.66):
                m.beam("corral_end_rail",p["beam"],(x,y[0],h),(x,y[1],h),.083,n=5)
    a.room(-2.05,0,.10,2.07,2.65,1.23,.53,style="shed" if f in {"lamanites","coriantumr"} else None)
    for y in (-1.83,1.83):
        m.box("feed_trough",p["beam"],(-.72,y,.305),(1.76,.53,.31),bevel=.052)
        m.box("fodder","goldgrain",(-.72,y,.473),(1.49,.35,.075),bevel=.025)
    for x,y in ((.79,-1.73),(1.31,1.75),(2.32,-1.38)):
        m.beam("hay_roll","goldgrain",(x,y-.21,.31),(x,y+.21,.31),.24,n=7)
    a.flag(2.92,-.98,.72,.84)


def quarry(a):
    m,p = a.m,a.p
    a.plinth(-.13,0,0,3.18,3.12,.115,False)
    for i,(x,y,z,rx,ry,rz) in enumerate(((-.83,0,.67,.85,1.14,.79),(-.45,-.85,.53,.75,.62,.57),
                                         (-.60,.91,.48,.96,.56,.51),(-1.40,.33,.48,.51,.67,.52),(.17,.53,.21,.63,.73,.31))):
        m.gem("quarried_ledge","stone" if i%2 else "rock",(x,y,z),(rx,ry,rz),n=7,skew=.47)
        m.gem("ore_lode","bronze" if i%2 else "ochre",(x+rx*.58,y-.056,z+.123),(.13,.24,.18),n=5,skew=.58)
    for y in (-.49,.49):
        a.column(.77,y,.114,1.29,.121,False,p["beam"])
    m.beam("mine_headframe",p["beam"],(.75,-.68,1.39),(.75,.70,1.39),.103,n=5)
    m.beam("mine_brace",p["beam"],(.76,-.47,.72),(.76,.35,1.35),.046,n=5)
    a.roof(.47,0,1.40,1.54,1.67,.34,"shed")
    m.box("copper_ore_barrel","cedar",(1.30,-.67,.27),(.43,.47,.40),bevel=.069)
    m.gem("barrel_ore","gold",(1.31,-.69,.48),(.15,.18,.14),n=5)


def workshop(a):
    m,p,f = a.m,a.p,a.faction
    a.plinth(-.10,0,0,3.93,3.80,.20)
    a.room(-.68,.11,.20,2.68,3.04,1.29,.52,style="flat" if f=="coriantumr" else None)
    a.roof(.86,-.53,1.36,1.72,1.92,.31,"shed" if f!="laban" else "hip")
    for y in (-1.29,.17):
        a.column(1.50,y,.20,1.17,.125,False,p["beam"])
    stack_rise = .42 if f == "laban" else 0.0
    m.loft("furnace_stack",p["base"],[(-.75,.93,.19,.42,.41),(-.78,.93,1.72,.35,.35),
                                       (-.81,.93,2.48+stack_rise,.25,.28)],cut=.16)
    m.loft("smoke_collar",p["trim"],[(-.81,.93,2.36+stack_rise,.30,.33),(-.81,.93,2.58+stack_rise,.31,.34)],cut=.13)
    m.box("smoke_throat","ink",(-.81,.93,2.585+stack_rise),(.45,.52,.034))
    m.box("forge_anvil","iron",(1.37,-.62,.59),(.57,.33,.24),bevel=.045)
    m.gem("anvil_horn","iron",(1.65,-.62,.67),(.28,.13,.11),n=5,skew=.53)
    m.box("workbench","wood",(.68,1.07,.75),(.77,1.04,.16),bevel=.023)
    for y in (.76,1.39):
        m.box("workbench_leg","cedar",(.70,y,.46),(.16,.13,.54))
    a.banners(1.04,.72,1.43,1.05,.42)


def market(a):
    m,p,f = a.m,a.p,a.faction
    a.plinth(0,0,0,3.75,3.86,.17)
    for side,y in ((-1,-.94),(1,.94)):
        x = -.24 if side<0 else .06
        for dx in (-1.16,.86):
            for dy in (-.53,.53):
                a.column(x+dx,y+dy,.16,1.37 if side==-1 else 1.52,.102,f=="laban",p["beam"] if f!="laban" else None)
        top = 1.54 if side==-1 else 1.69
        a.roof(x-.18,y,top,2.71,1.49,.46,"hip" if f=="laban" else "tent" if f=="gadianton" else "shed",
               p["accent"] if side<0 else "ivory")
        m.box("market_counter",p["beam"],(x+.19,y,.67),(1.08,1.03,.17),bevel=.023)
        for yy in (-.32,.32):
            m.box("market_counter_leg",p["beam"],(x+.19,y+yy,.38),(.13,.14,.59))
        jar(m,"displayed_crock",(x+.32,y-.29,.754),.161,.31,"clay" if side<0 else "teal")
        basket(m,"market_wares",(x+.54,y+.18,.754),.19,.17)
    a.flag(1.42,0,.16,1.94)


def storage(a):
    m,p,f = a.m,a.p,a.faction
    a.plinth(-.16,0,0,3.12,3.12,.23)
    a.room(-.49,0,.23,2.34,2.74,1.21,.47,style="tent" if f=="gadianton" else None)
    for y in (-1.13,1.13):
        a.column(.73,y,.21,1.24,.14,False,p["beam"])
    a.roof(.89,0,1.36,1.29,2.75,.33,"shed")
    for i,(x,y,z) in enumerate(((1.17,.82,.41),(1.10,-.85,.39),(1.36,.73,.71),(.88,1.29,.35))):
        m.box("tied_provisions",("sand","wood","ochre","cedar")[i],(x,y,z),(.45,.50,.36),bevel=.045)
        m.box("provision_tie",p["beam"],(x,y,z),(.088,.515,.375))
    a.banners(1.49,0,1.37,1.52,.41)


def barracks(a):
    m,p,f = a.m,a.p,a.faction
    if f=="gadianton":
        lodge(a, camp=True)
        return
    a.plinth(-.04,0,0,4.14,3.93,.18)
    a.room(-.87,0,.18,2.12,3.33,1.32,.64,style=p["roof_type"])
    for y in (-1.40,1.40):
        a.column(1.53,y,.16,1.55,.159)
        a.flag(1.51,y,1.67,.74)
        m.box("training_dummy_base",p["base"],(.47,y,.25),(.65,.55,.32),bevel=.043)
        m.beam("training_dummy_post",p["beam"],(.47,y,.36),(.47,y,1.29),.082,n=5)
        m.beam("dummy_crossbar",p["trim"],(.48,y-.29,1.03),(.48,y+.29,1.03),.074,n=5)
    m.box("weapon_rack",p["beam"],(.59,.01,.56),(.22,1.27,.78),bevel=.052)
    for i,y in enumerate((-.54,-.27,0,.27,.54)):
        m.beam("rack_spear",p["trim"],(.54,y,.40),(.79,y,1.47+(i%2)*.18),.031,n=4)
        m.gem("rack_spear_tip","bronze",(.815,y,1.49+(i%2)*.18),(.049,.036,.136),n=4)


def tower(a, cement):
    m,p,f = a.m,a.p,a.faction
    a.plinth(0,0,0,2.05,2.00,.24)
    height = 3.40 if cement else 2.87
    if f=="gadianton" and not cement:
        for x,y in ((-.68,-.65),(-.68,.65),(.68,-.65),(.68,.65)):
            m.beam("watchtower_support",p["beam"],(x,y,.17),(x*.68,y*.68,height),.092,n=5)
        for s in (-1,1):
            m.beam("watchtower_crossbrace",p["beam"],(-.64,s*.67,.41),(.48,s*.48,2.41),.074,n=5)
            m.beam("watchtower_crossbrace",p["beam"],(.64,s*.67,.38),(-.48,s*.48,2.39),.074,n=5)
    else:
        m.loft("battered_watchtower",p["wall"],[(0,0,.17,.85,.81),(-.03,0,height*.41,.66,.68),
                                                 (-.06,0,height,.55,.55)],cut=.27)
        for z in (.72,1.42,2.07):
            m.loft("watchtower_course",p["trim"],[(0,0,z,.80-z*.080,.77-z*.077),(0,0,z+.09,.80-z*.081,.77-z*.077)],cut=.22)
        a.door(.80,0,.24,.47,.87)
        for yy in (-.28,.28):
            m.box("arrow_slit","ink",(.64,yy,2.04),(.053,.088,.32))
    a.plinth(-.06,0,height,1.93,1.89,.21)
    for x in (-.71,.59):
        for y in (-.67,.67):
            a.column(x,y,height+.14,.83,.113,False,p["beam"])
    if cement:
        for x in (-.75,-.32,.22,.66):
            for y in (-.79,.79):
                m.box("cement_crenel",p["wall"],(x,y,height+.39),(.27,.27,.38),bevel=.032)
        a.flag(-.06,0,height+.25,1.26)
    else:
        a.roof(-.08,0,height+.94,2.02,1.95,.53)
    a.banners(.877,0,height+.34,1.06,.51)


def fortification(a, cement, gate):
    m,p,f = a.m,a.p,a.faction
    half = 1.077 if cement else 2.075
    thickness = .59 if cement else .65
    height = 1.58 if cement else 1.59
    if gate:
        for sign in (-1,1):
            x = sign*half*.77
            m.loft("gate_bastion",p["wall"],[(x,0,0,half*.26,thickness*.60),(x,0,height+ .37,half*.23,thickness*.46)],cut=.17)
            m.box("gate_capstone",p["trim"],(x,0,height+.36),(half*.60,thickness*1.37,.19),bevel=.033)
            a.column(sign*half*.57,.37,.04,height+.08,.098,False,p["beam"])
        m.box("raised_gate_lintel",p["beam"],(0,0,height+.15),(half*1.60,.64,.35),bevel=.05)
        m.box("gate_cornice",p["trim"],(0,0,height+.38),(half*1.61,.73,.15),bevel=.045)
        for i in (-2,-1,0,1,2):
            m.beam("raised_gate_tooth",p["beam"],(i*half*.204,0,height+.19),(i*half*.204,0,height-.18),.044,.022,n=4)
        a.flag(0,0,height+.47,.73)
    elif cement or f in {"coriantumr","shiz","nephites","laban"}:
        m.loft("wall_batter",p["wall"],[(0,0,0,half,thickness*.58),(0,0,.39,half,thickness*.54),
                                          (0,0,height-.24,half,thickness*.42)],cut=.071)
        m.box("walkable_wall_cap",p["trim"],(0,0,height-.21),(2*half,thickness*.97,.14),bevel=.031)
        for i in (-.77,-.27,.27,.77):
            m.box("wall_crenel",p["base"],(i*half,0,height-.028),(half*.29,thickness*.61,.31),bevel=.028)
        if f in {"laban","shiz"}:
            for x in (-half*.57,half*.57):
                m.box("colored_wall_relief",p["accent"],(x,.29,height*.54),(half*.37,.071,.20),team=True)
    else:
        for h in (.42,.93):
            m.beam("palisade_rail",p["beam"],(-half,0,h),(half,0,h),.165,n=5)
        for i in range(11):
            x = (i / 10 * 2 - 1)*half*.962
            m.beam("palisade_stake",p["beam"] if i%2 else "wood",(x,.105,.04),(x+.046,.072,height-.05+(i%3)*.094),
                   .196,.065,n=5)


def harbor(a):
    m,p,f = a.m,a.p,a.faction
    a.plinth(-1.02,0,0,2.00,3.21,.16)
    a.room(-1.02,0,.15,1.82,2.18,1.18,.54,style="shed" if f=="gadianton" else None)
    for i in range(11):
        x = -.06+i*.278
        m.box("wharf_board","wood" if i%3 else "cedar",(x,0,.17),(.268,2.14,.165))
    for x,y in ((.17,-1.11),(.17,1.11),(1.60,-1.11),(1.60,1.11),(2.65,-1.11),(2.65,1.11)):
        m.beam("mooring_pile",p["beam"],(x,y,-.11),(x,y,.49),.109,n=5)
        m.beam("mooring_cap",p["trim"],(x,y,.42),(x,y,.51),.121,n=5)
    m.beam("shipyard_hoist",p["beam"],(.31,-.85,.17),(.28,-.85,1.91),.101,n=5)
    m.beam("shipyard_jib",p["beam"],(.27,-.86,1.80),(1.65,-.85,1.87),.091,n=5)
    m.beam("hoist_brace",p["beam"],(.29,-.85,1.21),(1.02,-.85,1.84),.058,n=5)
    m.beam("hoist_line","leather",(1.60,-.85,1.85),(1.60,-.85,.77),.020,n=4)
    basket(m,"shore_basket",(-.03,1.32,.18),.24,.33)
    a.flag(-1.13,.69,1.61,.75)


def lodge(a, camp=False):
    m,p = a.m,a.p
    a.plinth(-.1,0,0,3.88 if camp else 3.44,3.58,.115,False)
    a.room(-.53,0,.13,2.83,3.18,1.17,.76 if camp else 0,style="tent",body="darkclay")
    if camp:
        a.roof(.78,.02,1.125,1.82,2.81,.55,"tent","moss")
    else:
        a.roof(-.556,0,1.30,2.83*1.12,3.18*1.12,.91,"ridge","forest")
        a.roof(.78,.02,1.125,1.62,2.65,.55,"shed","moss")
    for y in (-1.03,1.03):
        a.column(1.41,y,.11,1.03,.110,False,"bark")
        m.gem("camouflage_rock","rock",(-.9,y*1.40,.39),(.91,.46,.52),n=7,skew=-.42)
    if camp:
        for i,(x,y,z) in enumerate(((-1.22,-.92,1.74),(-1.61,-.53,1.92),(-.88,.72,1.78),(-1.74,.69,1.86),(-.34,-1.19,1.63))):
            m.gem("roof_bracken","forest" if i%2 else "moss",(x,y,z),(.59,.49,.33),n=6,skew=.22)
    for side in (-1,1):
        x,y = (1.40,side*.93)
        m.box("hidden_society_torch","wood",(x,y,.66),(.12,.12,.78),bevel=.022)
        m.gem("torch_glow","amber",(x,y,.99),(.110,.100,.224),n=5,skew=.48)
    if camp:
        for y in (-1.02,.79):
            m.box("camp_supply_box","cedar",(1.54,y,.26),(.49,.52,.40),bevel=.038)
    else:
        a.banners(1.66,.02,1.13,1.18,.48)


def winery(a):
    m,p = a.m,a.p
    a.plinth(0,0,0,3.88,3.84,.18)
    a.room(-.59,.41,.18,2.81,2.51,1.24,.52,style="hip")
    a.roof(.57,-.68,1.27,1.90,1.45,.34,"shed","plum")
    for y, height in ((-1.30,1.10),(-.04,1.28)):
        a.column(1.40,y,.18,height,.141)
    a.stairs(1.25,-.68,0,1.13,.20,.68,3)
    for i,(x,y) in enumerate(((.78,-.83),(.72,-.21),(-.24,-1.21),(-.79,-1.35),(1.54,-.75))):
        height = (.61,.72,.43,.47,.49)[i]
        jar(m,"wine_amphora",(x,y,.18),.19 + .035*(i%3),height,"wine" if i%2 else "clay",trim="gold")
    a.banners(1.50,-.68,1.25,1.19,.49)
    for y in (.44,1.43):
        x = 1.31
        basket(m,"vintner_vat",(x,y,.19),.44,.65,"cedar")
        m.loft("macerated_grapes","grape",[(x,y,.80,.37,.37),(x,y,.84,.37,.37)],n=7)


def structure(definition, faction=None, gate=False):
    id = definition["id"]
    f = faction or ("laban" if id=="winery" else "gadianton" if id=="secret_combination_lodge" else
                    "coriantumr" if id.startswith("cement") else "nephites")
    height_scale = .95 if id == "fields_of_grain" else 1.0
    horizontal_scale = height_scale
    if id == "temple_of_the_land":
        horizontal_scale = {"nephites": .92, "shiz": .88}.get(f, 1.0)
    elif id == "seat_of_the_people" and f == "gadianton":
        horizontal_scale = 1.08
    mesh = Mesh(id,"building",lambda v: (v[0]*horizontal_scale, v[1]*horizontal_scale, v[2]*height_scale))
    design = Architecture(mesh,f)
    builders = {"seat_of_the_people":citadel,"temple_of_the_land":shrine,"tents_and_habitations":housing,
                "fields_of_grain":crops,"gathering_of_flocks":flock,"hill_of_ore":quarry,
                "house_of_curious_workmanship":workshop,"place_of_trade":market,"storehouse_of_provisions":storage,
                "encampment_of_war":barracks,"place_of_many_waters":harbor,"secret_combination_lodge":lodge,
                "winery":winery}
    if id in {"works_of_defense","cement_wall"}:
        fortification(design,id=="cement_wall",gate)
    elif id in {"tower_of_watchmen","cement_tower"}:
        tower(design,id=="cement_tower")
    else:
        builders[id](design)
    return mesh


RESOURCE_NAMES = {"tree":"Timber Tree", "ore":"Ore Deposit", "berries":"Wild Berries",
                  "crate":"Dropped Cargo", "wild_game":"Wild Game"}


def resource(id):
    if id == "wild_game":
        return animal(id,"resource",scale=.95)
    m = Mesh(id,"resource")
    if id == "tree":
        trunk = [(.04,-.09,0),(0,0,.56),(.08,.014,1.37),(.01,.05,2.49),(-.11,.04,2.96)]
        path(m,"cedar_trunk","bark",trunk,.22,n=7,widths=[1.5,1.11,.82,.55,.16])
        for i,(x,y) in enumerate(sides(5,.16)):
            m.beam("surface_root","bark",(x*.61,y*.63,.035),(x*.083,y*.064,.48),.096,.17,n=5)
        tips = [(-.81,-.48,2.44),(.83,-.45,2.38),(.65,.67,2.56),(-.49,.58,2.75),(.14,.09,3.17)]
        for i,(x,y,z) in enumerate(tips):
            path(m,"canopy_branch","cedar",[(.02,.05,1.28+i*.17),(x*.46,y*.51,2.08+i*.07),(x,y,z)],.099,
                 n=5,widths=[1.25,.80,.23])
        rng = random.Random(904)
        for i,(x,y,z) in enumerate(tips):
            for j in range(3):
                color = ("forest","moss","olive","sage")[(i+j)%4]
                dx,dy = rng.uniform(-.37,.39),rng.uniform(-.44,.43)
                m.gem("foliage_cluster",color,(x+dx,y+dy,z+.25+j*.109),
                      (.61+rng.random()*.17,.48+rng.random()*.23,.48+rng.random()*.26),n=7,skew=rng.uniform(-.44,.44))
    elif id == "ore":
        rocks = ((.0,.0,.57,1.03,.86,.69),(-.61,.50,.33,.62,.47,.41),(.66,.11,.31,.58,.45,.39),
                 (-.52,-.51,.30,.63,.45,.34),(.22,-.67,.24,.48,.46,.29),(.64,.62,.22,.44,.34,.25))
        for i,(x,y,z,rx,ry,rz) in enumerate(rocks):
            m.gem("stone_outcrop",("rock","stone","earth")[i%3],(x,y,z),(rx,ry,rz),n=7,skew=.26-i*.11)
        for i,(x,y,z) in enumerate(((.34,-.51,.85),(.59,-.35,.79),(.40,-.66,.65),(.88,-.18,.43),
                                     (-.46,.47,.62),(.42,.54,.73),(.14,-.79,.40))):
            m.gem("native_metal",("gold","bronze","copper")[i%3],(x,y,z),(.16,.13,.15),n=5,skew=.68)
    elif id == "berries":
        for i,(x,y,z) in enumerate(((-.32,-.21,.35),(.25,-.29,.38),(.32,.31,.48),(-.24,.33,.41),(0,.10,.68))):
            m.beam("berry_cane","bark",(.01,.02,.02),(x,y,z),.069,.037,n=5)
            m.gem("leaf_cluster", ("forest","olive","moss")[i%3],(x,y,z),(.46,.39,.34),n=7,skew=.38)
            for j,(a,b) in enumerate(sides(4,.32)):
                m.gem("ripe_berry","red" if j%2 else "berry", (x+a*.31,y+b*.29,z+.07+(j%2)*.07),
                      (.11,.09,.12),n=5,skew=.14)
    elif id == "crate":
        m.box("goods_crate","wood",(0,0,.302),(.90,.72,.604),bevel=.048)
        for y in (-.241,0,.241):
            m.box("lid_slat","cedar" if y else "wood",(0,y,.60),(.845,.219,.065),bevel=.012)
        for x in (-.282,.282):
            m.box("leather_lid_binding","leather",(x,0,.643),(.092,.756,.042))
            for y in (-.366,.366):
                m.box("leather_side_binding","leather",(x,y,.32),(.092,.032,.636))
        m.beam("fastening_crossbrace","cedar",(.451,-.31,.095),(.451,.31,.53),.061,n=4)
        m.box("bronze_hasps","bronze",(.472,0,.525),(.04,.134,.155),bevel=.01)
    return m


def unit_model(definition, faction=None):
    id, kind = definition["id"], definition["kind"]
    if kind in {"beast","sheep"}:
        return animal(id,kind)
    if kind == "chariot":
        return chariot(id)
    if kind == "ship":
        return vessel(id,faction)
    if id == "laman_and_lemuel":
        mesh = person("laman",kind,offset=(.105,-.48,0),scale=.92,prefix="laman_")
        mesh.id = id
        mesh.insert(person("lemuel",kind,offset=(-.10,.47,0),scale=.92,prefix="lemuel_"),"lemuel_")
        return mesh
    return person(id,kind,faction)


def expected_assets():
    catalog = json.loads((DATA / "catalog.json").read_text(encoding="utf-8"))
    names = json.loads((DATA / "names.json").read_text(encoding="utf-8"))["names"]
    visuals = json.loads((DATA / "visuals.json").read_text(encoding="utf-8"))["sprites"]
    result = {}
    definitions = {d["id"]:d for tag in ("units","buildings") for d in catalog[tag]}
    buildings = {d["id"] for d in catalog["buildings"]}
    visual_variants = collections.defaultdict(set)
    faction_variants = collections.defaultdict(set)
    for key in visuals:
        id, sep, faction = key.partition("@")
        if sep:
            visual_variants[id].add(faction)
    for entry in catalog["factions"]:
        for id in entry["buildings"] + entry["units"]:
            faction_variants[id].add(entry["id"])
    for id,d in definitions.items():
        building = id in buildings
        category = "buildings" if building else "units"
        mode = structure if building else unit_model
        base_name = names.get(f"definition.{id}",d["name"])
        result[id] = (f"{category}/{id}.json",mode,d,None,False,base_name)
        factions = set(visual_variants[id])
        if building or d.get("kind") in {"worker","job","trader","infantry","ranged","priest","ship"}:
            factions.update(faction_variants[id])
        for f in sorted(factions):
            title = names.get(f"faction.{f}.definition.{id}",base_name)
            result[f"{id}:{f}"] = (f"{category}/{id}@{f}.json",mode,d,f,False,title)
        if id in {"works_of_defense","cement_wall"}:
            result[f"{id}:gate"] = (f"{category}/{id}@gate.json",mode,d,None,True,base_name+" Gate")
            for f in sorted(factions):
                result[f"{id}:gate:{f}"] = (f"{category}/{id}@gate@{f}.json",mode,d,f,True,base_name+" Gate")
    for id, display in RESOURCE_NAMES.items():
        result[id] = (f"resources/{id}.json",resource,{"id":id},None,False,display)
    return result


def write_model(file, document):
    # One line per part keeps individual objects easy to diff without a large format expansion.
    header = {k:v for k,v in document.items() if k != "parts"}
    pretty = json.dumps(header,indent=2,ensure_ascii=False).rstrip()[:-2]
    parts = ",\n".join("    "+json.dumps(part,separators=(",",":"),ensure_ascii=False) for part in document["parts"])
    file.parent.mkdir(parents=True,exist_ok=True)
    file.write_text(pretty + ',\n  "parts": [\n' + parts + '\n  ]\n}\n',encoding="utf-8")


def generate(entries):
    models, variants = {}, {}
    for key, (filename, loader, definition, faction, gate, display) in sorted(entries.items()):
        if loader == structure:
            mesh = loader(definition,faction,gate)
        elif loader == unit_model:
            mesh = loader(definition,faction)
        else:
            mesh = loader(key)
        write_model(MODELS / filename, mesh.document(display,faction))
        (variants if ":" in key else models)[key] = filename
    (MODELS / "models.json").write_text(json.dumps({"version":1,"models":models,"variants":variants},indent=2)+"\n",encoding="utf-8")



def read_model(manifest, key):
    return json.loads((MODELS / manifest[key]).read_text(encoding="utf-8"))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def finite_vector(value, size):
    return len(value)==size and all(type(n) in {int,float} and math.isfinite(n) for n in value)


def validate_building_layout(buildings, catalog):
    definitions = {entry["id"]: entry for entry in catalog["buildings"]}

    def model_for(id, faction):
        return buildings.get(f"{id}:{faction}", buildings[id])

    def area(model):
        low, high = model["bounds"]
        return (high[0] - low[0]) * (high[1] - low[1])

    for faction in catalog["factions"]:
        id = faction["id"]
        seat = model_for("seat_of_the_people", id)
        home = model_for("tents_and_habitations", id)
        require(area(home) <= .40 * area(seat), f"{id}: homes compete with the Seat's footprint")
        exceptions = {"fields_of_grain", "gathering_of_flocks", "seat_of_the_people"}
        if id == "laban":
            landmark = model_for("temple_of_the_land", id)
            require(area(landmark) >= 1.25 * area(seat) and landmark["height"] >= 1.25 * seat["height"],
                    "laban: the Great and Spacious Building must dominate the Crib")
            exceptions.add("temple_of_the_land")
        for building in sorted(set(faction["buildings"]) - exceptions):
            require(area(seat) >= 1.05 * area(model_for(building, id)),
                    f"{id}: the Seat needs a broader footprint than {building}")

    assembly = model_for("seat_of_the_people", "gadianton")
    radius = definitions["seat_of_the_people"]["radius"]
    require(all(math.hypot(x, y) <= radius for part in assembly["parts"] for x, y, _ in part["vertices"]),
            "gadianton: the Assembly extends beyond its placement circle")

    winery = model_for("winery", "laban")
    armory = model_for("house_of_curious_workmanship", "laban")
    require(winery["height"] <= .75 * armory["height"], "laban: the Winery should read lower than the Armory")
    vats = [part["vertices"] for part in winery["parts"] if re.fullmatch(r"vintner_vat(?:_\d+)?", part["name"])]
    foundation = next(part["vertices"] for part in winery["parts"] if part["name"] == "dressed_foundation")
    require(len(vats) == 2, "laban: the Winery needs two visible vats")
    for vat in vats:
        for axis in range(2):
            minimum, maximum = min(v[axis] for v in vat), max(v[axis] for v in vat)
            require(maximum - minimum >= .8, "laban: the Winery vats are too narrow to read at far zoom")
            require(min(v[axis] for v in foundation) <= minimum <= maximum <= max(v[axis] for v in foundation),
                    "laban: a Winery vat extends past the foundation")

    roof = max(z for part in armory["parts"] if part["name"].startswith("hipped_roof")
               for _, _, z in part["vertices"])
    collar = next(part["vertices"] for part in armory["parts"] if part["name"] == "smoke_collar")
    require(max(z for _, _, z in collar) - roof >= .8, "laban: the Armory's chimney is lost in the roofline")


def validate(entries):
    document = json.loads((MODELS / "models.json").read_text(encoding="utf-8"))
    manifest = document["models"] | document["variants"]
    require(document["version"]==1 and document["models"].keys().isdisjoint(document["variants"]), "Invalid manifest groups")
    require(manifest == {k:v[0] for k,v in entries.items()}, "Manifest does not match the current catalog and visuals")
    require(len(manifest)==len(set(manifest.values())), "Duplicate paths in models.json")
    counts = collections.defaultdict(list)
    buildings = {}
    for key, filename in manifest.items():
        path = MODELS / filename
        require(path.is_file() and path.resolve().is_relative_to(MODELS.resolve()), f"{key}: missing or unsafe asset path")
        model = read_model(manifest,key)
        require(model["version"]==1 and model["id"]==key.split(":")[0] and model["name"],f"{key}: invalid header")
        rig, parts = model["rig"],model["parts"]
        require(rig["root"]=={"pivot":[0,0,0]} and parts, f"{key}: missing root pivot or parts")
        for bone, entry in rig.items():
            require(re.fullmatch("[a-z0-9_]+",bone) and finite_vector(entry["pivot"],3), f"{key}: invalid bone {bone}")
            chain = set()
            parent = bone
            while parent != "root":
                require(parent in rig and parent not in chain,f"{key}: invalid parent chain for {bone}")
                chain.add(parent)
                parent = rig[parent].get("parent", "")
        animation = model.get("animation",{})
        style = animation.get("style")
        sockets = animation.get("sockets",{})
        require(style in {None,"ranged","spear","sword","staff","worker","concubine","beast","chariot","ship"},
                f"{key}: unknown animation style")
        for socket, entry in sockets.items():
            require(finite_vector(entry["position"],3) and entry["bone"] in rig,f"{key}: invalid socket {socket}")
        for bone in rig:
            for suffix in ("forearm_l","forearm_r","shin_l","shin_r"):
                if bone.endswith(suffix):
                    joint, side = suffix.split("_")
                    parent_kind = "arm" if joint == "forearm" else "leg"
                    parent = bone.removesuffix(suffix) + f"{parent_kind}_{side}"
                    require(rig[bone]["parent"] == parent,f"{key}: unexpected parent for {bone}")
            for segment,parent_kind in (("front","arm"),("mid","leg_mid"),("back","leg")):
                for side in ("l","r"):
                    suffix = f"shin_{segment}_{side}"
                    if bone.endswith(suffix):
                        require(rig[bone]["parent"]==bone.removesuffix(suffix)+f"{parent_kind}_{side}",
                                f"{key}: animal knee must follow its upper leg")
        if style=="ship":
            require("hull" in rig,f"{key}: missing hull rig")
            if model["id"]=="hagoths_vessel":
                require(all(rig.get(b,{}).get("parent")=="hull" for b in ("sail","oar_l","oar_r")),
                        f"{key}: sail and oars must be independently articulated on hull")
        if animation.get("equipment") in {"bow","longbow"}:
            require(rig["weapon"]["parent"]=="forearm_l" and sockets["draw_hand"]["bone"]=="forearm_r" and
                    all(sockets[name]["bone"]=="weapon" for name in ("projectile_release","bowstring_lower","bowstring_upper")),
                    f"{key}: invalid bow grip or draw-hand rig")
        lo, hi = [math.inf]*3, [-math.inf]*3
        tris = 0
        named = set()
        for part in parts:
            name, vertices, indices, color = (part[n] for n in ("name","vertices","triangles","color"))
            label = f"{key}/{name}"
            require(re.fullmatch("[a-z0-9_]+",name) and name not in named and part["bone"] in rig,
                    f"{label}: invalid part name or bone")
            if name.startswith("bow_stave") and animation.get("equipment") in {"bow","longbow"}:
                require(part["bone"]=="weapon",f"{label}: bow must follow the left grip")
            if re.search(r"(?:^|_)(lower_leg|hoof)(?:_[0-9]+)?$",name):
                require(re.search(r"shin_(front|mid|back)_[lr]$",part["bone"]),
                        f"{label}: animal lower leg must follow a knee joint")
            for articulated_part,bone_suffix in (("wrapped_calf","shin"),("sandaled_foot","shin"),
                                                ("ankle_binding","shin"),("vambrace","forearm"),
                                                ("forearm","forearm"),("hand","forearm")):
                if re.search(f"(?:^|_){articulated_part}(?:_[0-9]+)?$",name):
                    require(part["bone"].endswith((f"{bone_suffix}_l",f"{bone_suffix}_r")),
                            f"{label}: part does not follow its lower limb")
            named.add(name)
            require(finite_vector(color,4) and all(0<=c<=1 for c in color),f"{label}: invalid RGBA")
            require(len(vertices)>=3 and indices and len(indices)%3==0, f"{label}: missing or invalid triangles")
            for v in vertices:
                require(finite_vector(v,3),f"{label}: invalid vertex")
                for n in range(3):
                    lo[n],hi[n] = min(lo[n],v[n]),max(hi[n],v[n])
            require(all(type(i) is int and 0<=i<len(vertices) for i in indices),f"{label}: triangle index out of range")
            faces = iter(indices)
            for a,b,c in zip(faces,faces,faces):
                normal = cross(sub(vertices[b],vertices[a]),sub(vertices[c],vertices[a]))
                require(dot(normal,normal)>1e-13,f"{label}: degenerate triangle")
            tris += len(indices)//3
        require(model["bounds"]==[lo,hi] and abs(model["height"]-(hi[2]-lo[2]))<.0001,
                f"{key}: incorrect picking bounds")
        require(model["footprint"]==[round(2*max(abs(lo[i]),abs(hi[i])),4) for i in range(2)] and all(hi[i]>lo[i] for i in range(3)),
                f"{key}: incorrect footprint")
        require(lo[2] >= -.18 and hi[2] <= 6 and max(map(abs,lo+hi)) < 12, f"{key}: geometry exceeds expected dimensions")
        limit = 3400 if model["kind"]=="chariot" else 2400 if key=="laman_and_lemuel" else 2000 if model["kind"]=="building" else 1650
        require(tris<=limit,f"{key}: {tris} triangles exceeds limit {limit}")
        counts[model["kind"]].append(tris)
        if model["kind"] == "building":
            buildings[key] = model
    catalog = json.loads((DATA / "catalog.json").read_text(encoding="utf-8"))
    validate_building_layout(buildings, catalog)
    print(f"Validated {len(document['models'])} bases and {len(document['variants'])} variants ({sum(map(sum,counts.values())):,} triangles).")
    for kind, values in sorted(counts.items()):
        print(f"  {kind:>9}: {len(values):>3} assets, {min(values):,}-{max(values):,} triangles")
    return manifest


def export_obj(model, output):
    output = output.with_suffix(".obj")
    output.parent.mkdir(parents=True,exist_ok=True)
    material_file = output.with_suffix(".mtl")
    vertex_start = 1
    materials = []
    lines = [f"# {model['name']} — X forward, Y right, Z up, meters",f"mtllib {material_file.name}","s off"]
    for part in model["parts"]:
        material = "material_"+part["name"]
        materials.extend(["newmtl " + material, "Kd " + " ".join(map(str,part["color"][:3])),f"d {part['color'][3]}",""])
        lines.extend(["", "o " + part["name"],"g " + part["bone"],"usemtl " + material])
        lines.extend("v " + " ".join(map(str,v)) for v in part["vertices"])
        faces = iter(part["triangles"])
        lines.extend(f"f {a+vertex_start} {b+vertex_start} {c+vertex_start}" for a,b,c in zip(faces,faces,faces))
        vertex_start += len(part["vertices"])
    output.write_text("\n".join(lines)+"\n",encoding="utf-8")
    material_file.write_text("\n".join(materials),encoding="utf-8")
    print(f"Wrote {output} and {material_file.name}")


PREVIEWS = {
    "Structures": ["seat_of_the_people:"+f for f in FACTIONS] + ["temple_of_the_land:"+f for f in FACTIONS if f!="gadianton"]
        + ["secret_combination_lodge", "fields_of_grain", "fields_of_grain:laban", "gathering_of_flocks:coriantumr", "hill_of_ore",
           "house_of_curious_workmanship:shiz", "place_of_trade:laban", "storehouse_of_provisions", "encampment_of_war:lamanites",
           "place_of_many_waters", "tower_of_watchmen:gadianton", "works_of_defense:gate:laban", "cement_tower", "winery",
           "tents_and_habitations:nephites", "tents_and_habitations:laban", "cement_wall:gate:shiz", "works_of_defense:lamanites",
           "hill_of_ore:coriantumr"],
    "Characters": ["nephi","captain_moroni","mormon","alma","teancum","stripling_warrior",
                   "aaron","shiz","coriantumr","laban","king_noah","laman_and_lemuel",
                   "gadianton_robber","jaredite_shield_bearer","jaredite_archer","jaredite_warrior","cimeter_bearer","one_of_the_fifty",
                   "industrious_laborer:laban","industrious_laborer:gadianton","trader_in_precious_things","worker_in_ore",
                   "oath_keeper","drunken_concubine","priestcrafter","ammonite_covenant_keeper","lawyer","tiller_of_the_earth",
                   "keeper_of_flocks","bowman"],
    "World": ["tree","ore","berries","crate","wild_game","sheep",
              "curelom","cumelom","elephant","kings_chariot","hagoths_vessel","jaredite_barge"],
}


def render_tile(model, size, camera=None, world_scale=None, background=(42,40,41), player_color=None):
    from PIL import Image, ImageDraw, ImageFilter

    width,height = size
    camera = camera or unit((.85,-1,1.12 if model["kind"]=="building" else .67))
    right = unit(cross((0,0,1),camera))
    up = cross(camera,right)
    light = unit((.1,-.75,.88))
    geometry = [dot(v,right) for p in model["parts"] for v in p["vertices"]]
    heights = [dot(v,up) for p in model["parts"] for v in p["vertices"]]
    min_x,max_x,min_y,max_y = min(geometry),max(geometry),min(heights),max(heights)
    scale = world_scale or min(width*.87/(max_x-min_x),height*.86/(max_y-min_y))
    screen_x = width*.5 if world_scale else width*.5 - (min_x+max_x)*scale/2
    screen_y = height*.81 if world_scale else height*.5 + (min_y+max_y)*scale/2
    depth = [-math.inf]*(width*height)
    tile = Image.new("RGB",size,background)
    shadows = Image.new("L",size)
    shadow = ImageDraw.Draw(shadows)
    projected = []
    for part in model["parts"]:
        verts = part["vertices"]
        pigment = part["color"]
        if part.get("team") and player_color:
            pigment = tuple(a*.32+b*.68 for a,b in zip(pigment,player_color))
        points = [(screen_x+dot(v,right)*scale,screen_y-dot(v,up)*scale,dot(v,camera)) for v in verts]
        ground = [(x+.33*z,y+.30*z,0) for x,y,z in verts]
        ground = [(screen_x+dot(v,right)*scale,screen_y-dot(v,up)*scale) for v in ground]
        it = iter(part["triangles"])
        for a,b,c in zip(it,it,it):
            n = unit(cross(sub(verts[b],verts[a]),sub(verts[c],verts[a])))
            if dot(n,camera) < -1e-6:
                # Most surfaces are closed. Cloth meshes and pennants use a two-sided material.
                if "cloth" not in part["name"] and "mantle" not in part["name"]:
                    continue
                n = mul(n,-1)
            dot_light = max(0,dot(n,light))
            warm = (.60+dot_light*.47,.62+dot_light*.40,.70+dot_light*.27)
            color = tuple(min(255,int(c*f*255+8.5)) for c,f in zip(pigment,warm))
            projected.append((points[a],points[b],points[c],color))
            shadow.polygon([ground[a],ground[b],ground[c]],fill=160)
    tile.paste((14,15,17),(0,0,width,height),shadows.filter(ImageFilter.GaussianBlur(radius=3)))
    pixels = tile.load()
    for a,b,c,color in projected:
        x0,y0,z0 = a
        x1,y1,z1 = b
        x2,y2,z2 = c
        denominator = (y1-y2)*(x0-x2)+(x2-x1)*(y0-y2)
        if abs(denominator) < 1e-5:
            continue
        end_y = min(height-1,math.ceil(max(y0,y1,y2)))
        end_x = min(width-1,math.ceil(max(x0,x1,x2)))
        for y in range(max(0,math.floor(min(y0,y1,y2))),end_y+1):
            for x in range(max(0,math.floor(min(x0,x1,x2))),end_x+1):
                c0 = ((y1-y2)*(x+.5-x2)+(x2-x1)*(y+.5-y2))/denominator
                c1 = ((y2-y0)*(x+.5-x2)+(x0-x2)*(y+.5-y2))/denominator
                c2 = 1-c0-c1
                if min(c0,c1,c2) < 0:
                    continue
                z = z0*c0+z1*c1+z2*c2
                index = y*width+x
                if z > depth[index]:
                    depth[index] = z
                    pixels[x,y] = color
    return tile


def contact_sheets(manifest, output_dir):
    from PIL import Image, ImageDraw, ImageFont

    output_dir.mkdir(parents=True,exist_ok=True)
    fonts = Path("C:/Windows/Fonts")
    body_font = fonts / "segoeui.ttf"
    bold_font = fonts / "segoeuib.ttf"
    font = lambda path, size: ImageFont.truetype(str(path),size) if path.is_file() else ImageFont.load_default()
    for name, keys in PREVIEWS.items():
        cols, tile_w, tile_h, gutter, header = 6, 222, 215, 12, 94
        rows = (len(keys)+cols-1)//cols
        page = Image.new("RGB", (cols*(tile_w+gutter)+gutter,header+rows*(tile_h+gutter)+gutter),(30,29,31))
        draw = ImageDraw.Draw(page)
        draw.text((24,15),"Chronicles of the Promised Land",font=font(bold_font,30),fill=(238,215,166))
        draw.text((26,58),f"v0.9  /  {name}",font=font(body_font,16),fill=(211,186,152))
        for i,key in enumerate(keys):
            model = read_model(manifest,key)
            x,y = gutter+(i%cols)*(tile_w+gutter),header+(i//cols)*(tile_h+gutter)
            tile = render_tile(model,(tile_w*2,(tile_h-46)*2)).resize((tile_w,tile_h-46),Image.Resampling.LANCZOS)
            page.paste(tile,(x,y))
            draw.rectangle((x,y+tile_h-46,x+tile_w-1,y+tile_h-1),fill=(54,45,43))
            label = model["name"]
            size = 13 if len(label)>29 else 15
            draw.text((x+8,y+tile_h-43),label[:36],font=font(bold_font,size),fill=(241,219,179))
            faction = model.get("faction")
            tris = sum(len(p["triangles"])//3 for p in model["parts"])
            subtitle = (faction.title()+"  /  " if faction else "")+f"{tris:,} triangles"
            draw.text((x+9,y+tile_h-21),subtitle,font=font(body_font,12),fill=(192,171,151))
        filename = output_dir / f"Models-v0.9-{name.lower()}.png"
        page.save(filename)
        print(f"Wrote {filename}",flush=True)
    gameplay_scale_sheet(manifest,output_dir,font,body_font,bold_font)


def gameplay_scale_sheet(manifest, output_dir, font, body_font, bold_font):
    from PIL import Image, ImageDraw

    keys = [("industrious_laborer:nephites","Laborer"),("industrious_laborer:gadianton","Civilian"),
            ("industrious_laborer:laban","Laban Worker"),("tiller_of_the_earth","Farmer"),
            ("worker_in_ore","Miner"),("trader_in_precious_things","Trader"),
            ("keeper_of_flocks","Shepherd"),("man_of_the_sword","Swordsman"),
            ("stripling_warrior","Spearman"),("gadianton_robber","Robber"),
            ("bowman","Bowman"),("jaredite_archer","Longbow"),("slinger_of_stones","Slinger"),
            ("drunken_concubine","Concubine"),("one_of_the_fifty","Palace Guard")]
    calls = [(1,1,"Default  /  front"),(1,-1,"Default  /  rear"),(.48,1,"Farthest zoom"),(2.4,1,"Nearest zoom")]
    cell_width,cell_height,gutter,left,top = 78,102,5,143,86
    page = Image.new("RGB",(left+len(keys)*(cell_width+gutter)+gutter,top+len(calls)*(cell_height+gutter)+gutter),(30,29,31))
    draw = ImageDraw.Draw(page)
    draw.text((24,14),"Chronicles of the Promised Land",font=font(bold_font,28),fill=(238,215,166))
    draw.text((26,51),"v0.9  /  Unit silhouettes at gameplay scale  /  1600 × 900 reference viewport",
              font=font(body_font,16),fill=(211,186,152))
    player_color = (.27,.51,.90)
    models = [read_model(manifest,key) for key,_ in keys]
    for i,(zoom,facing,label) in enumerate(calls):
        row_y = top+i*(cell_height+gutter)
        draw.text((14,row_y+24),label,font=font(bold_font,13),fill=(236,211,167))
        draw.text((14,row_y+47),f"{12.727922*zoom:.1f} px / meter",font=font(body_font,12),fill=(195,174,149))
        camera = unit((facing,facing,math.sqrt(2/3)))
        for j,((_,name),model) in enumerate(zip(keys,models)):
            x = left+j*(cell_width+gutter)
            mesh_size = (cell_width,cell_height-18)
            tile = render_tile(model,tuple(2*n for n in mesh_size),camera=camera,
                               world_scale=2*12.727922*zoom,background=(99,112,72),player_color=player_color)
            page.paste(tile.resize(mesh_size,Image.Resampling.LANCZOS),(x,row_y))
            draw.rectangle((x,row_y+mesh_size[1],x+cell_width-1,row_y+cell_height-1),fill=(54,45,43))
            draw.text((x+4,row_y+mesh_size[1]+1),name,font=font(body_font,11),fill=(240,220,177))
    filename = output_dir / "Models-v0.9-gameplay-scale.png"
    page.save(filename)
    print(f"Wrote {filename}",flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--validate",action="store_true",help="Check the existing library without rewriting it")
    action.add_argument("--export",metavar="KEY",help="Write one asset as Wavefront OBJ and MTL")
    action.add_argument("--preview",action="store_true",help="Render the documentation contact sheets (requires Pillow)")
    parser.add_argument("--out",type=Path,help="OBJ filename or contact-sheet directory")
    args = parser.parse_args()
    entries = expected_assets()
    if not (args.validate or args.export or args.preview):
        generate(entries)
    manifest = validate(entries)
    if args.export:
        if args.export not in manifest:
            parser.error(f"Unknown model key: {args.export}")
        export_obj(read_model(manifest,args.export),args.out or Path(args.export.replace(":","@")).with_suffix(".obj"))
    if args.preview:
        contact_sheets(manifest,args.out or REVISION / "Docs")


if __name__ == "__main__":
    main()
