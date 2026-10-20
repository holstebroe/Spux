#!/usr/bin/env python3
"""Acceptance gate (DESIGN_GUIDELINES 7): lines of layout and widget code, retained against immediate.

Counts non-blank lines that are not only a comment, after formatting both sides with spux's .clang-format
(the plugins write denser code than spux's style). What is counted:
  retained   the functions of the plugin's GuiWindow.cpp that lay out controls, hold their state, handle
             input, cache and compose frames (the list below); `if (menuOpen_)` blocks are left out for
             Tannhauser, whose menu is not ported, and the logo plate's drawing for Acidus.
  immediate  the `binding`, `painters` and `page` sections of tests/gate/<plugin>/GuiWindow.cpp.
Drawing (skins, panel renderer, the logo plate), window plumbing, file dialogs and the CLAP extension are
left out on both sides; Tannhauser's PanelLayout (the table both versions use) is reported on its own.

  tools/gate_lines.py /path/to/Acidus /path/to/Tannhauser
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

SPUX = Path(__file__).resolve().parent.parent


def sloc(lines):
    n, block = 0, False
    for raw in lines:
        s = raw.strip()
        if block:
            if "*/" in s:
                block = False
            continue
        if not s or s.startswith("//"):
            continue
        if s.startswith("/*"):
            block = "*/" not in s
            continue
        n += 1
    return n


def formatted(path):
    """The file as spux's .clang-format writes it, so both sides are counted in one style."""
    exe = shutil.which("clang-format")
    if not exe:
        print(f"note: no clang-format, {path.name} counted as written")
        return path.read_text().splitlines()
    out = subprocess.run([exe, f"--style=file:{SPUX / '.clang-format'}", f"--assume-filename={path.name}"],
                         input=path.read_text(), capture_output=True, text=True, check=True).stdout
    return out.splitlines()


def functions(path, names, drop_menu=False, cut=None):
    """The bodies of GuiWindow::<name> (and free functions <name>) from their signature to the closing brace."""
    text = formatted(path)
    out = {}
    for name in names:
        pat = re.compile(r"^[A-Za-z_].*\b(GuiWindow::)?" + re.escape(name) + r"\(")
        start = next((i for i, l in enumerate(text) if pat.match(l) and not l.rstrip().endswith(";")), None)
        if start is None:
            raise SystemExit(f"{path}: {name} not found")
        depth, end = 0, start
        for i in range(start, len(text)):
            depth += text[i].count("{") - text[i].count("}")
            if "{" in "".join(text[start:i + 1]) and depth == 0:
                end = i
                break
        body = text[start:end + 1]
        if drop_menu:
            body = drop_blocks(body, "if (menuOpen_")
        if cut and name in cut:
            first, last = cut[name]
            a = next(i for i, l in enumerate(body) if first in l)
            b = next(i for i, l in enumerate(body) if last in l)
            body = body[:a] + body[b + 1:]
        out[name] = sloc(body)
    return out


def drop_blocks(lines, opener):
    out, i = [], 0
    while i < len(lines):
        if lines[i].strip().startswith(opener) and "{" in lines[i]:
            depth = 0
            while i < len(lines):
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
                if depth == 0:
                    break
            continue
        out.append(lines[i])
        i += 1
    return out


def sections(path, keep):
    counts, current = {}, None
    for line in path.read_text().splitlines():
        m = re.match(r"^\s*// --- (\w+) ", line)
        if m:
            current = m.group(1)
            continue
        if current in keep:
            counts.setdefault(current, []).append(line)
    return {k: sloc(v) for k, v in counts.items()}


def report(name, retained, immediate, shared=None):
    r, im = sum(retained.values()), sum(immediate.values())
    print(f"{name}")
    print(f"  retained   {r:5d}  ({', '.join(f'{k} {v}' for k, v in retained.items())})")
    print(f"  immediate  {im:5d}  ({', '.join(f'{k} {v}' for k, v in immediate.items())})")
    page = immediate.get("page", 0) + immediate.get("painters", 0)
    print(f"  immediate without the binding (gone once the plugin uses spux_params): {page}")
    if shared:
        print(f"  shared by both: {shared}")
    print(f"  ratio immediate / retained: {im / r:.2f} (without binding {page / r:.2f})")


def main():
    acidus, tann = Path(sys.argv[1]), Path(sys.argv[2])
    a = functions(acidus / "src/gui/GuiWindow.cpp",
                  ["initControls", "updateKnobValuesFromPlugin", "renderFrame", "handleMouseDown", "handleMouseDrag",
                   "handleMouseUp", "openMenu", "closeMenu", "menuItemAt", "handleRightClick", "handleMouseMove"],
                  cut={"renderFrame": ("Graphics g(hiResBuffer_", "kTitleDynamic);")}) # the logo plate's drawing
    report("Acidus panel", a, sections(SPUX / "tests/gate/acidus/GuiWindow.cpp", {"binding", "painters", "page"}))
    t = functions(tann / "src/gui/GuiWindow.cpp",
                  ["sameState", "margins", "paramNorm", "stateFor", "addDirty", "copyRect", "composeControl", "downsample",
                   "updateReadout", "renderFrame", "controlIndexAt", "setParamFromGui", "isBlackKey", "keyAt", "beginEdit",
                   "handleMouseDown", "handleMouseDrag", "handleMouseUp", "handleMouseMove", "handleWheel", "showStatus"],
                  drop_menu=True)
    layout = sloc(formatted(tann / "src/gui/PanelLayout.cpp")) + sloc(formatted(tann / "src/gui/PanelLayout.hpp"))
    report("Tannhauser panel", t, sections(SPUX / "tests/gate/tannhauser/GuiWindow.cpp", {"binding", "painters", "page"}),
           f"PanelLayout.cpp/.hpp {layout} lines (the control table)")


if __name__ == "__main__":
    main()
