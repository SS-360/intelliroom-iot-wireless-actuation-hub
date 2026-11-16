from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas


OUT = Path(__file__).with_name("IntelliRoom-Sticker-Sheet-A4.pdf")
PAGE_W, PAGE_H = A4
INK = colors.black
PAPER = colors.white
MID = colors.black
LINE = colors.black
AMBER = colors.white
WHITE = colors.white
TEAL = colors.white


def y_from_top(top_mm: float, height_mm: float = 0) -> float:
    return PAGE_H - (top_mm + height_mm) * mm


def rounded_label(c, x_mm, top_mm, w_mm, h_mm, fill=PAPER, stroke=INK, radius=0.8):
    x, y = x_mm * mm, y_from_top(top_mm, h_mm)
    w, h = w_mm * mm, h_mm * mm
    c.setFillColor(fill)
    c.setStrokeColor(stroke)
    c.setLineWidth(0.75)
    c.roundRect(x, y, w, h, radius * mm, fill=1, stroke=1)
    c.saveState()
    c.setDash(1.2 * mm, 1.2 * mm)
    c.setStrokeColor(LINE)
    c.setLineWidth(0.35)
    c.roundRect(x - 1.1 * mm, y - 1.1 * mm, w + 2.2 * mm, h + 2.2 * mm, (radius + 1) * mm, fill=0, stroke=1)
    c.restoreState()
    return x, y, w, h


def triangle_warning(c, x, y, size):
    c.saveState()
    c.setFillColor(AMBER)
    c.setStrokeColor(INK)
    c.setLineWidth(1.2)
    p = c.beginPath()
    p.moveTo(x + size / 2, y + size)
    p.lineTo(x + size, y)
    p.lineTo(x, y)
    p.close()
    c.drawPath(p, fill=1, stroke=1)
    c.setFillColor(INK)
    bolt = c.beginPath()
    bolt.moveTo(x + size * 0.57, y + size * 0.80)
    bolt.lineTo(x + size * 0.35, y + size * 0.47)
    bolt.lineTo(x + size * 0.51, y + size * 0.47)
    bolt.lineTo(x + size * 0.42, y + size * 0.19)
    bolt.lineTo(x + size * 0.67, y + size * 0.55)
    bolt.lineTo(x + size * 0.51, y + size * 0.55)
    bolt.close()
    c.drawPath(bolt, fill=1, stroke=0)
    c.restoreState()


def component_label(c, x, top, w, h, title, subtitle, code, accent=TEAL):
    px, py, pw, ph = rounded_label(c, x, top, w, h, radius=0.8)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 12.2)
    c.drawCentredString(px + pw / 2, py + ph / 2 - 4.1, title)


def small_label(c, x, top, w, h, title, note=""):
    px, py, pw, ph = rounded_label(c, x, top, w, h, fill=PAPER, stroke=INK, radius=0.8)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 8.4 if note else 10)
    c.drawCentredString(px + pw / 2, py + ph * (0.60 if note else 0.37), title)
    if note:
        c.setFillColor(MID)
        c.setFont("Helvetica-Bold", 6.5)
        c.drawCentredString(px + pw / 2, py + ph * 0.23, note)
    if title == "PE  /  EARTH":
        c.setStrokeColor(INK)
        c.setLineWidth(0.75)
        sx = px + pw - 6.2 * mm
        c.line(sx, py + ph * 0.78, sx, py + ph * 0.48)
        c.line(sx - 2.5 * mm, py + ph * 0.72, sx + 2.5 * mm, py + ph * 0.72)
        c.line(sx - 1.8 * mm, py + ph * 0.61, sx + 1.8 * mm, py + ph * 0.61)
        c.line(sx - 1.0 * mm, py + ph * 0.50, sx + 1.0 * mm, py + ph * 0.50)


def make_pdf():
    c = canvas.Canvas(str(OUT), pagesize=A4, pageCompression=1)
    c.setTitle("IntelliRoom demo labels and safety stickers")
    c.setAuthor("IntelliRoom project")

    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 15)
    c.drawString(8 * mm, y_from_top(9), "INTELLIROOM")
    c.setFillColor(MID)
    c.setFont("Helvetica", 7.5)
    c.drawRightString(202 * mm, y_from_top(9), "DEMO CONTROL SYSTEM  /  A4 LABEL SHEET")
    c.setStrokeColor(INK)
    c.setLineWidth(0.8)
    c.line(8 * mm, y_from_top(13), 202 * mm, y_from_top(13))

    x, y, w, h = rounded_label(c, 9, 17, 192, 14, fill=WHITE, stroke=INK, radius=0.8)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 16)
    c.drawCentredString(x + w / 2, y + 4.8 * mm, "INTELLIROOM  /  DEMO CONTROL SYSTEM")

    # High visibility safety labels.
    x, y, w, h = rounded_label(c, 9, 37, 94, 33, fill=AMBER, stroke=INK, radius=2.5)
    triangle_warning(c, x + 4 * mm, y + 6 * mm, 20 * mm)
    c.setFillColor(INK)
    c.setFont("Helvetica-Bold", 14)
    c.drawString(x + 28 * mm, y + h - 10 * mm, "DANGER  /  230 V AC")
    c.setFont("Helvetica-Bold", 9.3)
    c.drawString(x + 28 * mm, y + h - 17 * mm, "ISOLATE SUPPLY BEFORE OPENING")
    c.setFont("Helvetica-Bold", 8)
    c.drawString(x + 28 * mm, y + 6 * mm, "Mains voltage inside enclosure")

    x, y, w, h = rounded_label(c, 107, 37, 94, 33, fill=INK, stroke=INK, radius=2.5)
    c.setFillColor(WHITE)
    c.setStrokeColor(WHITE)
    c.setFont("Helvetica-Bold", 11.2)
    c.drawString(x + 5 * mm, y + h - 10 * mm, "CAUTION  /  DEMO ONLY")
    c.setFont("Helvetica-Bold", 8.7)
    c.drawString(x + 5 * mm, y + h - 18 * mm, "8-SOCKET BOX  ·  16 A MCB")
    c.setFont("Helvetica-Bold", 7.5)
    c.drawString(x + 5 * mm, y + 6 * mm, "NOT FOR HOUSEHOLD INSTALLATION")

    c.setFillColor(MID)
    c.setFont("Helvetica-Bold", 7)
    c.drawString(9 * mm, y_from_top(76), "COMPONENT IDENTIFICATION")
    labels = [
        ("ESP32 CONTROLLER", "Wi-Fi · dashboard · button inputs", "ESP", TEAL),
        ("ARDUINO UNO", "8-channel relay driver", "UNO", TEAL),
        ("20 x 4 I2C LCD", "Controller status display", "LCD", TEAL),
        ("8-CHANNEL RELAY MODULE", "Low-voltage control side", "RLY", TEAL),
        ("PZEM-004T ENERGY METER", "AC measurement module", "PZ", TEAL),
        ("BUCK CONVERTER", "DC step-down supply", "DC-DC", TEAL),
        ("5 V DC CONTROL SUPPLY", "External regulated low-voltage rail", "5V", TEAL),
        ("8-SOCKET JUNCTION BOX", "Plug-in demo load bank", "DEMO", TEAL),
        ("16 A MCB", "Demo load circuit protection", "MCB", AMBER),
        ("CURRENT TRANSFORMER", "CT coil · model dependent", "CT", AMBER),
    ]
    for idx, (title, subtitle, code, accent) in enumerate(labels):
        col, row = idx % 2, idx // 2
        component_label(c, 9 + col * 97, 80 + row * 18.5, 94, 16.5, title, subtitle, code, accent)

    c.setFillColor(MID)
    c.setFont("Helvetica-Bold", 7)
    c.drawString(9 * mm, y_from_top(177), "RELAY CHANNEL NUMBERS")
    for idx in range(8):
        col, row = idx % 4, idx // 4
        x, y, w, h = rounded_label(c, 9 + col * 48.5, 181 + row * 20, 45, 17, fill=PAPER, stroke=INK, radius=1.8)
        c.setFillColor(INK)
        c.setFont("Helvetica-Bold", 13)
        c.drawCentredString(x + w / 2, y + 4.7 * mm, str(idx + 1))

    c.setFillColor(MID)
    c.setFont("Helvetica-Bold", 7)
    c.drawString(9 * mm, y_from_top(224), "POWER, SIGNAL AND TERMINAL MARKERS")
    markers = [
        ("5 V DC", "LOW VOLTAGE"), ("GND", "0 V RETURN"), ("L  /  LIVE", "230 V AC"),
        ("N  /  NEUTRAL", "230 V AC"), ("PE  /  EARTH", "PROTECTIVE EARTH"),
        ("UART TX", "SERIAL OUT"), ("UART RX", "SERIAL IN"), ("CT INPUT", "CURRENT COIL"),
    ]
    for idx, (title, note) in enumerate(markers):
        col, row = idx % 4, idx // 4
        small_label(c, 9 + col * 48.5, 228 + row * 18, 45, 15, title, note)

    c.setFillColor(MID)
    c.setFont("Helvetica", 6.5)
    c.drawString(9 * mm, y_from_top(278), "Print A4 at 100% / Actual size. Dashed outlines are cut guides. Verify terminal labels before fitting.")
    c.showPage()
    c.save()
    return OUT


if __name__ == "__main__":
    print(make_pdf())
