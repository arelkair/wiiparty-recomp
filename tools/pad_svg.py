#!/usr/bin/env python3
"""Prepare controller SVGs from AL2009man's Gamepad Asset Pack (MIT) for the launcher.

The launcher draws these SVGs itself and lights up the pressed buttons, so every part that
has to react gets a stable id ("@south", "@dpad_up", "@stick_l"...). Inkscape metadata,
hidden layers and everything outside the controller group are dropped to keep the files small.

    python tools/pad_svg.py <pack-checkout> launcher/res/pads

<pack-checkout> is a clone of https://github.com/AL2009man/Gamepad-Asset-Pack.
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

SVG = "http://www.w3.org/2000/svg"
INKSCAPE = "http://www.inkscape.org/namespaces/inkscape"
LABEL = "{%s}label" % INKSCAPE
SHAPES = {"path", "ellipse", "circle", "rect", "line", "polygon", "polyline"}

ET.register_namespace("", SVG)

PACK = "Controller Asset Pack/"
PADS = {
    "switchpro": {
        "source": PACK + "Nintendo Switch Controller Images/Switch Pro Controller/Default Theme/Theme SVG/Switch Pro Controller VSCView.svg",
        "root": "Nintendo Switch Pro Controller",
        "parts": {
            "south": "B Button", "east": "A Button", "west": "Y Button", "north": "X Button",
            "l": "L Bumper", "r": "R Bumper", "zl": "ZL Trigger", "zr": "ZR Trigger",
            "dpad": "D-PAD", "stick_l": "Left Joystick", "stick_r": "Right Joystick",
            "back": "Minus", "start": "Plus", "guide": "Home", "misc1": "Capture",
        },
    },
    "xbox": {
        "source": PACK + "Xbox Wireless Controller Images/Default Theme/Theme SVG/Xbox Series X Color/Xbox Series X Controller VSCView White.svg",
        "root": "Xbox Series X Controller - Robot White",
        "parts": {
            "south": "A Button", "east": "B Button", "west": "X Button", "north": "Y Button",
            "zl": "Left Trigger", "zr": "Right Trigger", "dpad": "Xbox Series Controller D-PAD",
            "stick_l": "Left Stick", "stick_r": "Right Stick",
            "back": "View Button", "start": "Menu Button", "guide": "Xbox Guide Button", "misc1": "Share Button",
        },
    },
    "dualsense": {
        "source": PACK + "DualSense Controller Image/Default/Theme SVG/DualSense VSCView SVG.svg",
        "root": "DualSense Controller",
        "parts": {
            "south": "Crosss", "east": "Circle", "west": "Square", "north": "Triangle",
            "l": "L1", "r": "R1", "zl": "L2 Triggers", "zr": "R2 Trigger",
            "dpad_up": "D-PAD Up", "dpad_down": "D-PAD Down", "dpad_left": "D-PAD Left", "dpad_right": "D-PAD Right",
            "stick_l": "Left Stick", "stick_r": "Right Stick",
            "back": "Create Button", "start": "Option Button", "guide": "PS Button", "misc1": "Mute",
        },
    },
}


def tag(element):
    return element.tag.split("}")[-1]


def label(element):
    return (element.get(LABEL) or "").strip()


def find(root, wanted):
    for element in root.iter():
        if label(element) == wanted:
            return element
    raise SystemExit("label not found: " + wanted)


def hidden(element):
    return "display:none" in (element.get("style") or "").replace(" ", "")


def parents(root):
    return {child: parent for parent in root.iter() for child in parent}


def prune(tree_root, keep):
    """Remove everything that is neither the kept group, an ancestor of it, nor <defs>."""
    up = parents(tree_root)
    node = keep
    while up.get(node) is not None:
        parent = up[node]
        for child in list(parent):
            if child is not node and tag(child) != "defs":
                parent.remove(child)
        node = parent


def convert(pack, name, spec, out_dir):
    tree = ET.parse(pack / spec["source"])
    svg = tree.getroot()
    root = find(svg, spec["root"])
    prune(svg, root)
    for element in list(svg.iter()):
        for child in list(element):
            if hidden(child) or (child.tag.startswith("{") and not child.tag.startswith("{%s}" % SVG)):
                element.remove(child)
    # Tag the animated parts.
    for part, wanted in spec["parts"].items():
        group = find(root, wanted)
        shapes = [e for e in group.iter() if tag(e) in SHAPES]
        if not shapes:
            raise SystemExit("no shapes under " + wanted)
        colour = [e for e in shapes if label(e) == "Color"]
        if tag(group) in SHAPES:
            colour = [group]
        elif not colour:
            colour = [shapes[0]]
        for index, shape in enumerate(shapes):
            shape.set("id", "@" + part if shape in colour else "@%s_%d" % (part, index))
    # Strip editor attributes.
    for element in svg.iter():
        for key in list(element.attrib):
            if key.startswith("{") and not key.startswith("{http://www.w3.org/1999/xlink}"):
                del element.attrib[key]
    out = out_dir / (name + ".svg")
    tree.write(out, encoding="utf-8", xml_declaration=True)
    text = out.read_text(encoding="utf-8")
    text = re.sub(r">\s+<", "><", text)
    out.write_text(text, encoding="utf-8")
    print(out, len(text))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    pack = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, spec in PADS.items():
        convert(pack, name, spec, out_dir)


if __name__ == "__main__":
    main()
