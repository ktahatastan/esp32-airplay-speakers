#!/usr/bin/env python3
"""Minimal SVG schematic primitives for the Harman Kardom documentation sheet.

The point of this module is that geometry is computed, never hand-placed. A pin
knows its own coordinate, a wire is routed between two known coordinates, and a
label is anchored to the thing it names. That is what keeps the drawing free of
the overlapping text and clipped symbols that a hand-written SVG accumulates.

Coordinates are SVG user units on a 10 unit grid, origin top-left.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from xml.sax.saxutils import escape

GRID = 10
PIN_PITCH = 32
HEADER_H = 62
FOOTER_H = 18
STUB = 22


def snap(value: float) -> float:
    return round(value / GRID) * GRID


@dataclass
class Pin:
    name: str
    x: float
    y: float
    side: str  # "L" or "R"

    @property
    def xy(self) -> tuple[float, float]:
        """Outer end of the pin stub — the point a wire attaches to."""
        return (self.x - STUB, self.y) if self.side == "L" else (self.x + STUB, self.y)


@dataclass
class Block:
    ref: str
    title: str
    subtitle: str
    x: float
    y: float
    width: float
    left: list[str] = field(default_factory=list)
    right: list[str] = field(default_factory=list)
    note: str = ""
    pins: dict[str, Pin] = field(default_factory=dict, init=False)

    @property
    def height(self) -> float:
        rows = max(len(self.left), len(self.right), 1)
        return HEADER_H + rows * PIN_PITCH + FOOTER_H

    def build(self) -> None:
        # Pins are keyed by name, so a name reused on both sides would silently
        # overwrite the first one: its stub would never be drawn and every wire
        # asking for it would attach to the wrong side of the block.
        names = self.left + self.right
        duplicates = sorted({name for name in names if names.count(name) > 1})
        if duplicates:
            raise ValueError(f"{self.ref}: pin names must be unique, repeated {duplicates}")
        top = self.y + HEADER_H
        for index, name in enumerate(self.left):
            self.pins[name] = Pin(name, self.x, top + index * PIN_PITCH + PIN_PITCH / 2, "L")
        for index, name in enumerate(self.right):
            self.pins[name] = Pin(name, self.x + self.width, top + index * PIN_PITCH + PIN_PITCH / 2, "R")

    def pin(self, name: str) -> tuple[float, float]:
        return self.pins[name].xy


class Sheet:
    """Accumulates SVG fragments in explicit draw order."""

    def __init__(self, width: int, height: int, title: str, subtitle: str) -> None:
        self.width = width
        self.height = height
        self.title = title
        self.subtitle = subtitle
        self.background: list[str] = []
        self.wires: list[str] = []
        self.symbols: list[str] = []
        self.overlay: list[str] = []
        self.blocks: dict[str, Block] = {}

    # ---------------------------------------------------------------- blocks
    def block(self, ref: str, title: str, subtitle: str, x: float, y: float,
              width: float, left: list[str] | None = None, right: list[str] | None = None,
              note: str = "") -> Block:
        item = Block(ref, title, subtitle, snap(x), snap(y), width, left or [], right or [], note)
        item.build()
        self.blocks[ref] = item

        h = item.height
        self.symbols.append(
            f'<rect class="blk" x="{item.x}" y="{item.y}" width="{width}" height="{h}" rx="6"/>'
            f'<line class="blk-div" x1="{item.x}" y1="{item.y + HEADER_H - 10}" '
            f'x2="{item.x + width}" y2="{item.y + HEADER_H - 10}"/>'
        )
        self.symbols.append(
            f'<text class="blk-ref" x="{item.x}" y="{item.y - 9}">{escape(ref)}</text>'
            f'<text class="blk-title" x="{item.x + width / 2}" y="{item.y + 26}">{escape(title)}</text>'
            f'<text class="blk-sub" x="{item.x + width / 2}" y="{item.y + 43}">{escape(subtitle)}</text>'
        )
        for pin in item.pins.values():
            ox, oy = pin.xy
            self.symbols.append(f'<line class="pin" x1="{pin.x}" y1="{pin.y}" x2="{ox}" y2="{oy}"/>')
            if pin.side == "L":
                self.symbols.append(
                    f'<text class="pin-name" text-anchor="start" x="{pin.x + 10}" y="{pin.y + 4}">{escape(pin.name)}</text>')
            else:
                self.symbols.append(
                    f'<text class="pin-name" text-anchor="end" x="{pin.x - 10}" y="{pin.y + 4}">{escape(pin.name)}</text>')
        if note:
            self.symbols.append(
                f'<text class="blk-note" x="{item.x + width / 2}" y="{item.y + h + 18}">{escape(note)}</text>')
        return item

    # ----------------------------------------------------------------- wires
    def wire(self, points: list[tuple[float, float]], kind: str = "sig") -> None:
        path = " ".join(f"{x},{y}" for x, y in points)
        self.wires.append(f'<polyline class="w {kind}" points="{path}"/>')

    def wire_hvh(self, a: tuple[float, float], b: tuple[float, float], xmid: float, kind: str = "sig") -> None:
        """Horizontal, vertical, horizontal — the standard orthogonal route."""
        self.wire([a, (xmid, a[1]), (xmid, b[1]), b], kind)

    def wire_vhv(self, a: tuple[float, float], b: tuple[float, float], ymid: float, kind: str = "sig") -> None:
        self.wire([a, (a[0], ymid), (b[0], ymid), b], kind)

    def junction(self, x: float, y: float) -> None:
        self.overlay.append(f'<circle class="junction" cx="{x}" cy="{y}" r="4.5"/>')

    def netlabel(self, x: float, y: float, text: str, anchor: str = "middle", dy: float = -8) -> None:
        self.overlay.append(
            f'<text class="net" text-anchor="{anchor}" x="{x}" y="{y + dy}">{escape(text)}</text>')

    # --------------------------------------------------------------- symbols
    def power_port(self, x: float, y: float, name: str, direction: str = "up") -> None:
        """Global rail symbol: a wire never has to cross the sheet to reach a rail."""
        if direction == "up":
            self.symbols.append(
                f'<line class="w rail" x1="{x}" y1="{y}" x2="{x}" y2="{y - 16}"/>'
                f'<line class="rail-bar" x1="{x - 16}" y1="{y - 16}" x2="{x + 16}" y2="{y - 16}"/>'
                f'<text class="rail-name" text-anchor="middle" x="{x}" y="{y - 24}">{escape(name)}</text>')
        else:
            self.symbols.append(
                f'<line class="w rail" x1="{x}" y1="{y}" x2="{x}" y2="{y + 16}"/>'
                f'<line class="rail-bar" x1="{x - 16}" y1="{y + 16}" x2="{x + 16}" y2="{y + 16}"/>'
                f'<text class="rail-name" text-anchor="middle" x="{x}" y="{y + 32}">{escape(name)}</text>')

    def gnd(self, x: float, y: float, name: str = "") -> None:
        self.symbols.append(
            f'<line class="w gnd" x1="{x}" y1="{y}" x2="{x}" y2="{y + 14}"/>'
            f'<line class="gnd-bar" x1="{x - 15}" y1="{y + 14}" x2="{x + 15}" y2="{y + 14}"/>'
            f'<line class="gnd-bar" x1="{x - 9}" y1="{y + 20}" x2="{x + 9}" y2="{y + 20}"/>'
            f'<line class="gnd-bar" x1="{x - 4}" y1="{y + 26}" x2="{x + 4}" y2="{y + 26}"/>')
        if name:
            self.symbols.append(
                f'<text class="gnd-name" text-anchor="middle" x="{x}" y="{y + 42}">{escape(name)}</text>')

    def fuse(self, x: float, y: float, ref: str, value: str) -> tuple[tuple[float, float], tuple[float, float]]:
        """Horizontal fuse. Returns the left and right terminals."""
        self.symbols.append(
            f'<rect class="sym" x="{x}" y="{y - 10}" width="56" height="20" rx="3"/>'
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x + 56}" y2="{y}"/>'
            f'<text class="sym-ref" text-anchor="middle" x="{x + 28}" y="{y - 18}">{escape(ref)}</text>'
            f'<text class="sym-val" text-anchor="middle" x="{x + 28}" y="{y + 30}">{escape(value)}</text>')
        return (x, y), (x + 56, y)

    def switch(self, x: float, y: float, ref: str, value: str) -> tuple[tuple[float, float], tuple[float, float]]:
        """Horizontal SPST, drawn open. Returns the left and right terminals."""
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x + 14}" y2="{y}"/>'
            f'<circle class="sym-dot" cx="{x + 16}" cy="{y}" r="3.5"/>'
            f'<line class="sym-line" x1="{x + 18}" y1="{y - 2}" x2="{x + 48}" y2="{y - 20}"/>'
            f'<circle class="sym-dot" cx="{x + 50}" cy="{y}" r="3.5"/>'
            f'<line class="sym-line" x1="{x + 52}" y1="{y}" x2="{x + 66}" y2="{y}"/>'
            f'<text class="sym-ref" text-anchor="middle" x="{x + 33}" y="{y - 30}">{escape(ref)}</text>'
            f'<text class="sym-val" text-anchor="middle" x="{x + 33}" y="{y + 28}">{escape(value)}</text>')
        return (x, y), (x + 66, y)

    def resistor_v(self, x: float, y: float, ref: str, value: str,
                   dnp: bool = False) -> tuple[tuple[float, float], tuple[float, float]]:
        """Vertical IEC resistor. Returns the top and bottom terminals."""
        cls = "sym dnp" if dnp else "sym"
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x}" y2="{y + 12}"/>'
            f'<rect class="{cls}" x="{x - 11}" y="{y + 12}" width="22" height="42" rx="2"/>'
            f'<line class="sym-line" x1="{x}" y1="{y + 54}" x2="{x}" y2="{y + 66}"/>'
            f'<text class="sym-ref" text-anchor="start" x="{x + 18}" y="{y + 28}">{escape(ref)}</text>'
            f'<text class="sym-val" text-anchor="start" x="{x + 18}" y="{y + 44}">{escape(value)}</text>')
        return (x, y), (x, y + 66)

    def cap_v(self, x: float, y: float, ref: str, value: str,
              polarized: bool = False) -> tuple[tuple[float, float], tuple[float, float]]:
        """Vertical capacitor. Returns the top and bottom terminals."""
        self.symbols.append(f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x}" y2="{y + 26}"/>')
        self.symbols.append(f'<line class="sym-plate" x1="{x - 18}" y1="{y + 26}" x2="{x + 18}" y2="{y + 26}"/>')
        if polarized:
            self.symbols.append(
                f'<path class="sym-plate" d="M {x - 18} {y + 42} q 18 -12 36 0" fill="none"/>'
                f'<text class="sym-pol" x="{x - 34}" y="{y + 22}">+</text>')
        else:
            self.symbols.append(f'<line class="sym-plate" x1="{x - 18}" y1="{y + 38}" x2="{x + 18}" y2="{y + 38}"/>')
        bottom = y + 42 if polarized else y + 38
        self.symbols.append(f'<line class="sym-line" x1="{x}" y1="{bottom}" x2="{x}" y2="{y + 64}"/>')
        self.symbols.append(
            f'<text class="sym-ref" text-anchor="start" x="{x + 26}" y="{y + 28}">{escape(ref)}</text>'
            f'<text class="sym-val" text-anchor="start" x="{x + 26}" y="{y + 44}">{escape(value)}</text>')
        return (x, y), (x, y + 64)

    def cap_h(self, x: float, y: float, ref: str, value: str) -> tuple[tuple[float, float], tuple[float, float]]:
        """Horizontal non-polarised capacitor. Returns the left and right terminals."""
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x + 26}" y2="{y}"/>'
            f'<line class="sym-plate" x1="{x + 26}" y1="{y - 18}" x2="{x + 26}" y2="{y + 18}"/>'
            f'<line class="sym-plate" x1="{x + 38}" y1="{y - 18}" x2="{x + 38}" y2="{y + 18}"/>'
            f'<line class="sym-line" x1="{x + 38}" y1="{y}" x2="{x + 64}" y2="{y}"/>'
            f'<text class="sym-ref" text-anchor="middle" x="{x + 32}" y="{y - 26}">{escape(ref)}</text>'
            f'<text class="sym-val" text-anchor="middle" x="{x + 32}" y="{y + 36}">{escape(value)}</text>')
        return (x, y), (x + 64, y)

    def led_v(self, x: float, y: float, ref: str, value: str, colour: str,
              dnp: bool = False) -> tuple[tuple[float, float], tuple[float, float]]:
        """Vertical LED, anode on top. Returns the anode and cathode terminals."""
        cls = "sym dnp" if dnp else "sym"
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x}" y2="{y + 14}"/>'
            f'<path class="{cls}" style="fill:{colour}" d="M {x - 14} {y + 14} L {x + 14} {y + 14} L {x} {y + 38} Z"/>'
            f'<line class="sym-plate" x1="{x - 14}" y1="{y + 38}" x2="{x + 14}" y2="{y + 38}"/>'
            f'<line class="sym-line" x1="{x}" y1="{y + 38}" x2="{x}" y2="{y + 52}"/>'
            f'<path class="led-ray" d="M {x + 16} {y + 16} l 12 -10 M {x + 16} {y + 26} l 12 -10"/>'
            f'<text class="sym-ref" text-anchor="start" x="{x + 32}" y="{y + 22}">{escape(ref)}</text>'
            f'<text class="sym-val" text-anchor="start" x="{x + 32}" y="{y + 38}">{escape(value)}</text>')
        return (x, y), (x, y + 52)

    def cell_h(self, x: float, y: float, label: str) -> tuple[tuple[float, float], tuple[float, float]]:
        """One battery cell, positive terminal on the right."""
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x + 22}" y2="{y}"/>'
            f'<line class="sym-plate-long" x1="{x + 22}" y1="{y - 20}" x2="{x + 22}" y2="{y + 20}"/>'
            f'<line class="sym-plate" x1="{x + 34}" y1="{y - 11}" x2="{x + 34}" y2="{y + 11}"/>'
            f'<line class="sym-line" x1="{x + 34}" y1="{y}" x2="{x + 56}" y2="{y}"/>'
            f'<text class="cell-name" text-anchor="middle" x="{x + 28}" y="{y - 28}">{escape(label)}</text>')
        return (x, y), (x + 56, y)

    def cell_v(self, x: float, y: float, label: str) -> tuple[tuple[float, float], tuple[float, float]]:
        """One battery cell drawn vertically, positive on top.

        Returns the positive and negative terminals so a series string can be
        wired without guessing the symbol height.
        """
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x}" y2="{y + 18}"/>'
            f'<line class="sym-plate-long" x1="{x - 20}" y1="{y + 18}" x2="{x + 20}" y2="{y + 18}"/>'
            f'<line class="sym-plate" x1="{x - 11}" y1="{y + 30}" x2="{x + 11}" y2="{y + 30}"/>'
            f'<line class="sym-line" x1="{x}" y1="{y + 30}" x2="{x}" y2="{y + 48}"/>'
            f'<text class="sym-pol" x="{x - 34}" y="{y + 16}">+</text>'
            f'<text class="cell-name" text-anchor="start" x="{x + 30}" y="{y + 28}">{escape(label)}</text>')
        return (x, y), (x, y + 48)

    def speaker(self, x: float, y: float, ref: str, label: str, detail: str) -> tuple[tuple[float, float], tuple[float, float]]:
        """Driver symbol. Returns the plus and minus terminals on the left."""
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y - 16}" x2="{x + 24}" y2="{y - 16}"/>'
            f'<line class="sym-line" x1="{x}" y1="{y + 16}" x2="{x + 24}" y2="{y + 16}"/>'
            f'<rect class="sym" x="{x + 24}" y="{y - 32}" width="24" height="64" rx="2"/>'
            f'<path class="sym" d="M {x + 48} {y - 32} L {x + 84} {y - 56} L {x + 84} {y + 56} L {x + 48} {y + 32} Z"/>'
            f'<text class="sym-pol" x="{x + 4}" y="{y - 22}">+</text>'
            f'<text class="sym-pol" x="{x + 4}" y="{y + 32}">−</text>'
            f'<text class="drv-ref" text-anchor="middle" x="{x + 42}" y="{y + 78}">{escape(ref)}</text>'
            f'<text class="drv-name" text-anchor="middle" x="{x + 42}" y="{y + 98}">{escape(label)}</text>'
            f'<text class="drv-detail" text-anchor="middle" x="{x + 42}" y="{y + 118}">{escape(detail)}</text>')
        return (x, y - 16), (x, y + 16)

    def net_flag(self, x: float, y: float, name: str, direction: str = "R") -> None:
        """Off-block net label. Two flags with the same name are the same net."""
        w = 16 + len(name) * 8.4
        if direction == "R":
            body = f"M {x} {y} l 12 -13 h {w} v 26 h -{w} Z"
            tx, anchor = x + 20, "start"
        elif direction == "L":
            body = f"M {x} {y} l -12 -13 h -{w} v 26 h {w} Z"
            tx, anchor = x - 20, "end"
        elif direction == "D":
            body = f"M {x} {y} l -13 -12 v -{w} h 26 v {w} Z"
            tx, anchor = x, "middle"
        else:
            body = f"M {x} {y} l -13 12 v {w} h 26 v -{w} Z"
            tx, anchor = x, "middle"
        self.symbols.append(f'<path class="flag" d="{body}"/>')
        if direction in ("R", "L"):
            self.symbols.append(
                f'<text class="flag-name" text-anchor="{anchor}" x="{tx}" y="{y + 5}">{escape(name)}</text>')
        else:
            offset = -22 - (w - 20) / 2 if direction == "D" else 22 + (w - 20) / 2
            self.symbols.append(
                f'<text class="flag-name" text-anchor="middle" x="{tx}" y="{y + offset}" '
                f'transform="rotate(-90 {tx} {y + offset})">{escape(name)}</text>')

    def pushbutton_v(self, x: float, y: float, ref: str, value: str) -> tuple[tuple[float, float], tuple[float, float]]:
        """Vertical normally-open momentary switch. Returns the top and bottom terminals."""
        self.symbols.append(
            f'<line class="sym-line" x1="{x}" y1="{y}" x2="{x}" y2="{y + 14}"/>'
            f'<circle class="sym-dot" cx="{x}" cy="{y + 17}" r="3.5"/>'
            f'<circle class="sym-dot" cx="{x}" cy="{y + 45}" r="3.5"/>'
            f'<line class="sym-line" x1="{x - 2}" y1="{y + 20}" x2="{x + 20}" y2="{y + 20}"/>'
            f'<line class="sym-line" x1="{x - 2}" y1="{y + 42}" x2="{x + 20}" y2="{y + 42}"/>'
            f'<line class="sym-line" x1="{x + 20}" y1="{y + 20}" x2="{x + 20}" y2="{y + 42}"/>'
            f'<line class="sym-line" x1="{x - 16}" y1="{y + 31}" x2="{x + 16}" y2="{y + 31}"/>'
            f'<line class="sym-line" x1="{x}" y1="{y + 45}" x2="{x}" y2="{y + 62}"/>'
            f'<text class="sym-ref" text-anchor="start" x="{x + 30}" y="{y + 26}">{escape(ref)}</text>'
            f'<text class="sym-val" text-anchor="start" x="{x + 30}" y="{y + 42}">{escape(value)}</text>')
        return (x, y), (x, y + 62)

    def testpoint(self, x: float, y: float, number: int, anchor: tuple[float, float] | None = None) -> None:
        """Numbered probe point. Draws a leader when it is not on the net itself."""
        if anchor:
            self.overlay.append(
                f'<line class="tp-leader" x1="{anchor[0]}" y1="{anchor[1]}" x2="{x}" y2="{y}"/>'
                f'<circle class="junction" cx="{anchor[0]}" cy="{anchor[1]}" r="3.5"/>')
        self.overlay.append(
            f'<circle class="tp" cx="{x}" cy="{y}" r="13"/>'
            f'<text class="tp-text" x="{x}" y="{y + 4}">{number}</text>')

    # ----------------------------------------------------------------- panes
    #: Vertical offset from a panel top edge to its first body baseline.
    PANEL_BODY_TOP = 62
    #: Baseline pitch inside a panel.
    PANEL_LINE = 24

    def panel(self, x: float, y: float, w: float, h: float, title: str, kind: str = "note") -> None:
        self.overlay.append(
            f'<rect class="panel {kind}" x="{x}" y="{y}" width="{w}" height="{h}" rx="6"/>')
        if title:
            self.overlay.append(
                f'<text class="panel-title {kind}-t" x="{x + 16}" y="{y + 28}">{escape(title)}</text>'
                f'<line class="tb-line" x1="{x + 16}" y1="{y + 42}" x2="{x + w - 16}" y2="{y + 42}"/>')

    def panel_body(self, x: float, y: float, lines, cls: str = "panel-text") -> float:
        """Write body lines at the panel pitch. Returns the next free baseline."""
        baseline = y + self.PANEL_BODY_TOP
        for line in lines:
            text, line_cls = line if isinstance(line, tuple) else (line, cls)
            if text:
                self.panel_line(x + 20, baseline, text, line_cls)
            baseline += self.PANEL_LINE
        return baseline

    def panel_line(self, x: float, y: float, text: str, cls: str = "panel-text") -> None:
        self.overlay.append(f'<text class="{cls}" x="{x}" y="{y}">{escape(text)}</text>')

    def zone(self, x: float, y: float, w: float, h: float, letter: str, title: str) -> None:
        self.background.append(
            f'<rect class="zone" x="{x}" y="{y}" width="{w}" height="{h}" rx="8"/>'
            f'<rect class="zone-tab" x="{x}" y="{y}" width="{34 + len(title) * 9.2}" height="28" rx="6"/>'
            f'<text class="zone-letter" x="{x + 13}" y="{y + 20}">{escape(letter)}</text>'
            f'<text class="zone-title" x="{x + 32}" y="{y + 20}">{escape(title)}</text>')

    def render(self, css: str) -> str:
        parts = [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.width}" height="{self.height}" '
            f'viewBox="0 0 {self.width} {self.height}" role="img" aria-labelledby="t d">',
            f'<title id="t">{escape(self.title)}</title>',
            f'<desc id="d">{escape(self.subtitle)}</desc>',
            f"<style>{css}</style>",
            f'<rect class="page" x="0" y="0" width="{self.width}" height="{self.height}"/>',
        ]
        parts += self.background + self.wires + self.symbols + self.overlay
        parts.append("</svg>")
        return "\n".join(parts) + "\n"
