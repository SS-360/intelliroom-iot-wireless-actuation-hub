"""Create a crisp, print-ready 4:5 IntelliRoom LinkedIn carousel PDF."""
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.utils import ImageReader
from PIL import Image
from io import BytesIO

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs" / "assets"
OUT = ROOT / "docs" / "IntelliRoom-LinkedIn-Carousel.pdf"
W, H = 576, 720  # 4:5 portrait, matching the supplied carousel reference.
FONT_DIR = Path(r"C:\Windows\Fonts")
pdfmetrics.registerFont(TTFont("UI", str(FONT_DIR / "segoeui.ttf")))
pdfmetrics.registerFont(TTFont("UIBold", str(FONT_DIR / "segoeuib.ttf")))
pdfmetrics.registerFont(TTFont("UILight", str(FONT_DIR / "segoeuil.ttf")))

PAPER = HexColor("#F7F2E9")
INK = HexColor("#20242B")
MUTED = HexColor("#68717A")
AMBER = HexColor("#C46A22")
AMBER_LIGHT = HexColor("#F4E0CB")
BLUE = HexColor("#456B87")
BLUE_LIGHT = HexColor("#E8EFF2")
CARD = HexColor("#FFFEFC")
LINE = HexColor("#DDD7CE")
SOFT = HexColor("#EEE9E1")
GREEN = HexColor("#547461")

c = canvas.Canvas(str(OUT), pagesize=(W, H), pageCompression=1)
c.setTitle("IntelliRoom | Project Carousel")
c.setAuthor("IntelliRoom Project Team, AAA College of Engineering and Technology")
c.setSubject("A visual overview of the IntelliRoom control and energy monitoring prototype")


def text(x, y, value, size=9, color=INK, font="UI", align="left"):
    c.setFont(font, size)
    c.setFillColor(color)
    if align == "center":
        c.drawCentredString(x, y, value)
    elif align == "right":
        c.drawRightString(x, y, value)
    else:
        c.drawString(x, y, value)


def wrap(value, max_width, font="UI", size=9):
    lines, line = [], ""
    for word in value.split():
        test = word if not line else f"{line} {word}"
        if pdfmetrics.stringWidth(test, font, size) <= max_width:
            line = test
        else:
            if line:
                lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


def para(x, y, value, width, size=9, leading=13, color=MUTED, font="UI", max_lines=None):
    lines = wrap(value, width, font, size)
    if max_lines and len(lines) > max_lines:
        lines = lines[:max_lines]
        last = lines[-1]
        while last and pdfmetrics.stringWidth(last + "…", font, size) > width:
            last = last[:-1]
        lines[-1] = last.rstrip() + "…"
    for i, line in enumerate(lines):
        text(x, y - i * leading, line, size, color, font)
    return y - len(lines) * leading


def rounded(x, y, w, h, fill=CARD, stroke=LINE, radius=10, shadow=True):
    if shadow:
        c.setFillColor(HexColor("#EAE4DB"))
        c.roundRect(x + 1, y - 2, w, h, radius, fill=1, stroke=0)
    c.setFillColor(fill)
    c.setStrokeColor(stroke)
    c.setLineWidth(0.7)
    c.roundRect(x, y, w, h, radius, fill=1, stroke=1)


def image_fit(path, x, y, w, h, cover=False, radius=7):
    im = Image.open(path).convert("RGB")
    iw, ih = im.size
    if cover:
        if iw / ih > w / h:
            nw = max(1, int(ih * w / h))
            left = (iw - nw) // 2
            im = im.crop((left, 0, left + nw, ih))
        else:
            nh = max(1, int(iw * h / w))
            top = (ih - nh) // 2
            im = im.crop((0, top, iw, top + nh))
    # Avoid embedding multi-megapixel camera originals when a carousel panel
    # only displays a fraction of that resolution. Keep screenshots lossless.
    scale = min(1.0, (w * 2.8) / im.width, (h * 2.8) / im.height)
    if scale < 1.0:
        im = im.resize((round(im.width * scale), round(im.height * scale)), Image.Resampling.LANCZOS)
    encoded = BytesIO()
    if Path(path).suffix.lower() == ".png":
        im.save(encoded, format="PNG", optimize=True)
    else:
        im.save(encoded, format="JPEG", quality=88, optimize=True)
    encoded.seek(0)
    image = ImageReader(encoded)
    iw, ih = im.size
    if cover:
        c.saveState()
        p = c.beginPath()
        p.roundRect(x, y, w, h, radius)
        c.clipPath(p, stroke=0)
        c.drawImage(image, x, y, w, h, mask="auto")
        c.restoreState()
    else:
        scale = min(w / iw, h / ih)
        dw, dh = iw * scale, ih * scale
        c.drawImage(image, x + (w - dw) / 2, y + (h - dh) / 2,
                    dw, dh, mask="auto")


def page_base(section, index, total=6):
    c.setFillColor(PAPER)
    c.rect(0, 0, W, H, fill=1, stroke=0)
    # Quiet amber registration marks give the page a tactile editorial frame.
    c.setStrokeColor(HexColor("#E8D8C6"))
    c.setLineWidth(0.6)
    c.line(30, H - 31, W - 30, H - 31)
    c.setFillColor(AMBER)
    c.circle(34, H - 31, 2.1, fill=1, stroke=0)
    text(43, H - 35, "INTELLIROOM", 7.2, INK, "UIBold")
    text(W - 31, H - 35, section.upper(), 6.5, MUTED, "UIBold", "right")
    c.setStrokeColor(LINE)
    c.line(30, 30, W - 30, 30)
    text(30, 17, "AAA COLLEGE OF ENGINEERING AND TECHNOLOGY  ·  CSE", 6.1, MUTED, "UI")
    text(W - 30, 17, f"{index:02d}  /  {total:02d}", 6.1, AMBER, "UIBold", "right")


def heading(kicker, title, subtitle=None, y=641, title_size=27):
    text(38, y, kicker.upper(), 7, AMBER, "UIBold")
    text(38, y - 37, title, title_size, INK, "UIBold")
    if subtitle:
        para(39, y - 56, subtitle, 495, 9.1, 13, MUTED)


def pill(x, y, label, fill=AMBER_LIGHT, color=AMBER, size=7.1):
    width = pdfmetrics.stringWidth(label, "UIBold", size) + 18
    c.setFillColor(fill)
    c.roundRect(x, y, width, 19, 9.5, fill=1, stroke=0)
    text(x + width / 2, y + 6, label, size, color, "UIBold", "center")
    return width


def bullet(x, y, title, body, width, color=AMBER, size=8.4, leading=12):
    c.setFillColor(color)
    c.circle(x + 2, y + 3, 2.1, fill=1, stroke=0)
    text(x + 11, y, title, size, INK, "UIBold")
    end_y = para(x + 11, y - 15, body, width - 12, 7.9, 11, MUTED)
    return end_y - 11


def arrow(x1, y1, x2, y2, color=AMBER, width=1.5):
    c.setStrokeColor(color)
    c.setFillColor(color)
    c.setLineWidth(width)
    c.line(x1, y1, x2, y2)
    import math
    angle = math.atan2(y2 - y1, x2 - x1)
    size = 5
    p = c.beginPath()
    p.moveTo(x2, y2)
    p.lineTo(x2 - size * math.cos(angle - 0.55), y2 - size * math.sin(angle - 0.55))
    p.lineTo(x2 - size * math.cos(angle + 0.55), y2 - size * math.sin(angle + 0.55))
    p.close()
    c.drawPath(p, fill=1, stroke=0)


def finish():
    c.showPage()


# 01 — Cover: real prototype photography, crisp vector typography.
page_base("Project feature", 1)
text(39, 617, "A CONNECTED SYSTEMS PROJECT", 8, AMBER, "UIBold")
text(37, 564, "Control & energy,", 32, INK, "UIBold")
text(37, 524, "made legible.", 32, AMBER, "UIBold")
para(40, 492, "Meet IntelliRoom: a student-built control prototype that brings relay commands, energy visibility and simple automation into one clear interface.", 470, 10.2, 15, MUTED)

rounded(38, 183, 500, 273, CARD, LINE, 13, True)
image_fit(ASSETS / "prototype-controller.jpg", 47, 192, 482, 255, cover=True, radius=9)
pill(49, 207, "WORKING DEMONSTRATION MVP", HexColor("#FFF8EF"), AMBER)

text(40, 155, "A practical build. An interface designed for clarity.", 12.5, INK, "UIBold")
labels = [("CONTROL", AMBER_LIGHT, AMBER), ("STATUS", BLUE_LIGHT, BLUE), ("AUTOMATION", SOFT, INK)]
px = 40
for label, fill, col in labels:
    px += pill(px, 112, label, fill, col) + 9
para(40, 82, "Arduino Uno handles the relay outputs. ESP32 coordinates the dashboard, inputs, display and meter interface.", 475, 8.6, 12, MUTED)
finish()


# 02 — Architecture and signal flow.
page_base("How it works", 2)
heading("01 / System design", "A simple path from intent to action", "Two controllers divide the work: network and application logic on the ESP32; relay output switching on the Uno.")

top = 470
nodes = [
    (39, "WEB CONSOLE", "Browser command", BLUE_LIGHT, BLUE),
    (169, "ESP32", "Mode + policy", AMBER_LIGHT, AMBER),
    (299, "ARDUINO UNO", "Relay driver", SOFT, INK),
    (429, "RELAY BANK", "Switched loads", HexColor("#F3E8DD"), AMBER),
]
for x, name, sub, fill, col in nodes:
    rounded(x, top, 108, 65, fill, LINE, 9, False)
    c.setFillColor(col)
    c.roundRect(x, top + 56, 108, 9, 4, fill=1, stroke=0)
    text(x + 54, top + 34, name, 8.2, INK, "UIBold", "center")
    text(x + 54, top + 16, sub, 7.2, MUTED, "UI", "center")
for x in (149, 279, 409):
    arrow(x, top + 32, x + 17, top + 32, AMBER if x < 279 else BLUE)
text(235, top - 15, "HTTP API", 6.4, MUTED, "UIBold", "center")
text(365, top - 15, "9600 baud · one-way mask", 6.4, MUTED, "UIBold", "center")

# Inputs and observability join at the ESP32.
rounded(43, 285, 222, 105, CARD, LINE, 10, False)
text(57, 370, "INPUTS TO THE ESP32", 7, AMBER, "UIBold")
bullet(58, 348, "Four physical buttons", "Each invokes its saved relay combination in Manual mode.", 193, AMBER, 8, 11)
bullet(58, 309, "Mode selector", "Remote disables physical button actions; web control remains available.", 193, BLUE, 8, 11)

rounded(311, 285, 222, 105, CARD, LINE, 10, False)
text(325, 370, "STATUS AND MEASUREMENT", 7, BLUE, "UIBold")
bullet(326, 348, "20 × 4 LCD", "Cycles through meaningful controller and meter summaries.", 193, BLUE, 8, 11)
bullet(326, 309, "PZEM-004T + CT", "Intended AC measurements use a separate ESP32 UART.", 193, AMBER, 8, 11)

rounded(43, 135, 490, 105, HexColor("#FFF8EF"), HexColor("#E8C9A9"), 10, False)
text(59, 214, "IMPORTANT STATE BOUNDARY", 7, AMBER, "UIBold")
para(59, 193, "The ESP32 sends desired relay states to the Uno. This firmware link does not return contact feedback, so the interface represents the last commanded state—not independent proof that a relay contact or load changed.", 452, 9.3, 14, INK)
text(59, 153, "This distinction is shown in the README and project documentation.", 7.4, MUTED, "UI")
finish()


# 03 — Product interface: real UI screenshots, explicitly marked demo data.
page_base("The interface", 3)
heading("02 / Product experience", "See the system at a glance", "A responsive technician console separates read-only status from relay controls and keeps the power chart prominent.")

cards = [
    (39, "HOME", "Status first; no relay controls on the overview.", ASSETS / "ui-home.png", BLUE),
    (299, "CONTROLS", "Individual channel actions with a live power chart.", ASSETS / "ui-controls.png", AMBER),
]
for x, label, cap, img, accent in cards:
    rounded(x, 339, 238, 204, CARD, LINE, 10)
    text(x + 14, 518, label, 7.3, accent, "UIBold")
    text(x + 14, 500, cap, 8.1, INK, "UIBold")
    rounded(x + 10, 355, 218, 131, HexColor("#F1F3F4"), LINE, 5, False)
    image_fit(img, x + 12, 357, 214, 127, cover=False)

pill(40, 302, "SCREENSHOTS USE MOCK DATA", HexColor("#FFF1DB"), AMBER, 6.7)
para(40, 280, "The locally hosted dashboard provides a Home overview, individual relay Controls, technician Setup, timestamped Activity and an About page with project details and connections.", 492, 9.2, 14, MUTED)

rounded(39, 126, 498, 103, BLUE_LIGHT, HexColor("#D2DEE3"), 10, False)
text(55, 202, "DESIGNED AROUND CLEAR STATES", 7, BLUE, "UIBold")
bullet(56, 182, "Home", "Read-only summary of relay commands, link state and energy readings when available.", 460, BLUE, 8.1, 11)
bullet(56, 146, "Controls", "Each channel is a clear, individually selectable cell; energized-commanded cells receive a whole-cell highlight.", 460, AMBER, 8.1, 11)
finish()


# 04 — Technician configuration and schedules.
page_base("Configuration", 4)
heading("03 / Customization", "Custom controls, built for your workflow", "Technicians can name each channel, build button combinations and schedule selected relays without editing firmware.")

for x, title, img, accent in [
    (39, "BUTTON COMBINATIONS", ASSETS / "ui-button-mapping.png", AMBER),
    (299, "SCHEDULES + TIMERS", ASSETS / "ui-schedules.png", BLUE),
]:
    rounded(x, 374, 238, 175, CARD, LINE, 10)
    text(x + 13, 528, title, 7, accent, "UIBold")
    rounded(x + 10, 386, 218, 126, HexColor("#F0F2F3"), LINE, 4, False)
    image_fit(img, x + 12, 388, 214, 122, cover=False)

rounded(39, 134, 498, 206, CARD, LINE, 10)
text(55, 315, "FOUR BUILDING BLOCKS", 7, AMBER, "UIBold")
bullet(56, 291, "Names that make sense", "Rename channels to match the connected load; labels are saved on the ESP32.", 455, AMBER, 8.3, 12)
bullet(56, 245, "One mapping workspace", "Name a combination, select its relays in a compact grid and assign it to a physical button.", 455, BLUE, 8.3, 12)
bullet(56, 199, "Schedules with explicit actions", "Choose weekdays, start and end time, relay selection, and ON or OFF action at each boundary.", 455, AMBER, 8.3, 12)
bullet(56, 153, "Per-channel timers", "Choose a duration in seconds, minutes or hours; the selected relay is commanded off at expiry.", 455, BLUE, 8.3, 12)
text(40, 107, "Physical button actions are enabled only in Manual mode. Authenticated web controls work in both modes.", 8, INK, "UIBold")
finish()


# 05 — Actual demonstrator and component roles.
page_base("The prototype", 5)
heading("04 / Built and demonstrated", "A working MVP on the bench", "The student demonstration combines controller hardware, a relay module and a separately wired multi-socket demo enclosure.")

rounded(39, 369, 238, 195, CARD, LINE, 10)
image_fit(ASSETS / "prototype-controller.jpg", 48, 405, 220, 149, cover=True, radius=6)
text(50, 387, "CONTROLLER + RELAY ASSEMBLY", 7, INK, "UIBold")
rounded(299, 369, 238, 195, CARD, LINE, 10)
image_fit(ASSETS / "prototype-demo-box.jpg", 308, 405, 220, 149, cover=True, radius=6)
text(310, 387, "SEPARATE SOCKET DEMONSTRATOR", 7, INK, "UIBold")

rounded(39, 197, 498, 145, BLUE_LIGHT, HexColor("#D4E0E4"), 10, False)
text(55, 317, "KEY HARDWARE ROLES", 7, BLUE, "UIBold")
col1 = [
    ("ESP32 Dev Module", "Hosts the local dashboard and coordinates system logic."),
    ("Arduino Uno", "Applies the relay output mask received over serial."),
    ("Relay module", "Switches the prototype's independently wired output channels."),
]
col2 = [
    ("PZEM-004T + CT", "Meter path for AC measurements; wiring and readings remain to be verified."),
    ("20 × 4 I²C LCD", "Local summary of controller, network, relay-command and meter status."),
    ("Buttons + selector", "Manual combinations and Remote/Manual mode selection."),
]
for i, (title, body) in enumerate(col1):
    bullet(56, 293 - 37 * i, title, body, 221, AMBER, 7.8, 10)
for i, (title, body) in enumerate(col2):
    bullet(303, 293 - 37 * i, title, body, 218, BLUE, 7.8, 10)

rounded(39, 99, 498, 76, HexColor("#FFF3E4"), HexColor("#E9CCAF"), 10, False)
text(55, 151, "DEMO SCOPE + SAFETY", 7, AMBER, "UIBold")
para(55, 132, "The prototype uses an eight-socket junction box and includes a 16 A MCB. It has not been installed in a household switch box. An MCB alone does not certify the assembly; any AC wiring must be reviewed by a qualified electrician before energizing.", 462, 8.2, 12, INK)
finish()


# 06 — Honest status, next validation steps, and contributors.
page_base("What comes next", 6)
heading("05 / Status + next steps", "Built to be tested—and improved", "IntelliRoom is an educational control prototype. The interface and command path are in place; measurement and electrical validation are explicit next steps.")

rounded(39, 354, 239, 187, CARD, LINE, 10)
text(55, 514, "IN PLACE", 7.2, GREEN, "UIBold")
y = 490
y = bullet(56, y, "Two-controller command path", "ESP32 dashboard and mode logic send relay masks to the Uno.", 204, GREEN, 8, 11)
y = bullet(56, y, "Responsive web console", "Status, controls, setup, schedules, activity and project details.", 204, GREEN, 8, 11)
y = bullet(56, y, "Technician customization", "Channel names, button mappings, schedule rules and timers.", 204, GREEN, 8, 11)

rounded(299, 354, 238, 187, CARD, LINE, 10)
text(315, 514, "NEXT VALIDATION", 7.2, AMBER, "UIBold")
y = 490
y = bullet(316, y, "Verify the PZEM path", "Confirm module terminals, CT placement, serial response and calibration.", 204, AMBER, 8, 11)
y = bullet(316, y, "Add command acknowledgement", "Return relay-driver state if confirmed status is required in the UI.", 204, BLUE, 8, 11)
y = bullet(316, y, "Review the AC assembly", "Check enclosure, clearances, load ratings and protection with a qualified person.", 204, AMBER, 8, 11)

rounded(39, 218, 498, 111, HexColor("#FFF8EF"), HexColor("#E9D6C2"), 10, False)
text(55, 305, "PROJECT TEAM", 7, AMBER, "UIBold")
para(55, 283, "Suraj Kumar  ·  Santhosh.R  ·  Sam Sankar.P  ·  Sivakumar.M  ·  Rithick.C  ·  Vittal.R", 460, 9.1, 14, INK, "UIBold")
text(55, 247, "Guide", 7, MUTED, "UIBold")
text(91, 247, "Mr. S. Parameswaran, AP/CSE", 8.4, INK, "UI")
text(55, 229, "Department of Computer Science and Engineering  ·  AAA College of Engineering and Technology", 7.3, MUTED, "UI")

text(40, 172, "Energy visibility starts with trustworthy measurement.", 15.5, INK, "UIBold")
para(41, 147, "The dashboard's sample graph and readings are mock data. PZEM readings should be treated as unavailable until the meter and CT are connected and verified on the prototype.", 485, 8.9, 13, MUTED)
text(40, 89, "INTELLIROOM  ·  CONTROL AND ENERGY, MADE LEGIBLE", 7, AMBER, "UIBold")
finish()

c.save()
print(f"Created {OUT}")
