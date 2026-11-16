"""Build the six-slide IntelliRoom carousel in a clean mint/editorial style."""
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageDraw
from reportlab.lib.colors import HexColor
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs" / "assets"
OUT = ROOT / "docs" / "IntelliRoom-LinkedIn-Carousel.pdf"
W, H = 576, 720  # Reference carousel uses portrait 4:5 pages.
FONT_DIR = Path(r"C:\Windows\Fonts")
pdfmetrics.registerFont(TTFont("UI", str(FONT_DIR / "segoeui.ttf")))
pdfmetrics.registerFont(TTFont("UIBold", str(FONT_DIR / "segoeuib.ttf")))
pdfmetrics.registerFont(TTFont("UILight", str(FONT_DIR / "segoeuil.ttf")))

INK = HexColor("#17212A")
MUTED = HexColor("#53616B")
AMBER = HexColor("#D17A2B")
TEAL = AMBER  # Retained as a code alias; rendered accent is amber, never green.
CORAL = HexColor("#E25855")
BLUE = HexColor("#429BC3")
VIOLET = HexColor("#8264B6")
CARD = HexColor("#FFFFFD")
LINE = HexColor("#E5E0D9")
PALE_TEAL = HexColor("#FFF2E3")
PALE_AMBER = HexColor("#FFF5E8")
PALE_BLUE = HexColor("#EAF6F9")
PALE_VIOLET = HexColor("#F2EFF8")


def make_background():
    """Subtle top-warm to bottom-mint gradient, like the supplied reference."""
    width, height = 1152, 1440
    top, bottom = (252, 248, 240), (248, 237, 230)
    im = Image.new("RGB", (width, height))
    draw = ImageDraw.Draw(im)
    for y in range(height):
        t = y / max(1, height - 1)
        color = tuple(round(a * (1 - t) + b * t) for a, b in zip(top, bottom))
        draw.line((0, y, width, y), fill=color)
    return im


BACKGROUND = make_background()


def txt(x, y, value, size=9, color=INK, font="UI", align="left"):
    """Draw selectable vector text and fail early if a line would be clipped."""
    width = pdfmetrics.stringWidth(value, font, size)
    left = x - width / 2 if align == "center" else (x - width if align == "right" else x)
    if left < 14 or left + width > W - 14 or y < 8 or y + size > H - 8:
        raise ValueError(f"Text outside page bounds: {value!r}")
    c.setFont(font, size)
    c.setFillColor(color)
    if align == "center":
        c.drawCentredString(x, y, value)
    elif align == "right":
        c.drawRightString(x, y, value)
    else:
        c.drawString(x, y, value)


def fit_txt(x, y, value, max_width, size=26, minimum=18, color=INK, font="UIBold"):
    while size > minimum and pdfmetrics.stringWidth(value, font, size) > max_width:
        size -= 0.5
    txt(x, y, value, size, color, font)
    return size


def wrap(value, max_width, size=8.5, font="UI"):
    result, line = [], ""
    for word in value.split():
        candidate = word if not line else f"{line} {word}"
        if pdfmetrics.stringWidth(candidate, font, size) <= max_width:
            line = candidate
        else:
            if line:
                result.append(line)
            line = word
    if line:
        result.append(line)
    if any(pdfmetrics.stringWidth(line, font, size) > max_width for line in result):
        raise ValueError("A word is wider than the target text column")
    return result


def para(x, y, value, width, size=8.5, leading=12, color=MUTED, font="UI", max_lines=None):
    lines = wrap(value, width, size, font)
    if max_lines and len(lines) > max_lines:
        raise ValueError(f"Paragraph overflow ({len(lines)} lines): {value[:60]}")
    for i, line in enumerate(lines):
        txt(x, y - i * leading, line, size, color, font)
    return y - len(lines) * leading


def rounded(x, y, w, h, fill=CARD, stroke=LINE, radius=10, shadow=True):
    if min(x, y, w, h) < 0 or x + w > W or y + h > H:
        raise ValueError("Card outside page bounds")
    if shadow:
        c.setFillColor(HexColor("#E5D8CC"))
        c.roundRect(x + 1, y - 2, w, h, radius, fill=1, stroke=0)
    c.setFillColor(fill)
    c.setStrokeColor(stroke)
    c.setLineWidth(0.65)
    c.roundRect(x, y, w, h, radius, fill=1, stroke=1)


def fit_image(path, x, y, w, h, pad=0):
    """Fit the entire source image into its frame, preserving aspect ratio."""
    im = Image.open(path).convert("RGB")
    max_w, max_h = w - 2 * pad, h - 2 * pad
    scale = min(max_w / im.width, max_h / im.height)
    display_w, display_h = im.width * scale, im.height * scale
    encoded = BytesIO()
    if Path(path).suffix.lower() == ".png":
        # Keep the product screenshots lossless so their text and rules stay crisp.
        im.thumbnail((1500, 1000), Image.Resampling.LANCZOS)
        im.save(encoded, format="PNG", optimize=True)
    else:
        im.thumbnail((1800, 1300), Image.Resampling.LANCZOS)
        im.save(encoded, format="JPEG", quality=91, optimize=True)
    encoded.seek(0)
    c.drawImage(ImageReader(encoded), x + (w - display_w) / 2,
                y + (h - display_h) / 2, display_w, display_h)


def page_base(section, number):
    c.drawImage(ImageReader(BACKGROUND), 0, 0, W, H)
    # Brand mark and compact running header mirror both supplied carousels.
    c.setFillColor(AMBER)
    c.roundRect(34, 676, 17, 17, 4, fill=1, stroke=0)
    c.setFillColor(INK)
    c.roundRect(38, 679, 3.8, 11, 1, fill=1, stroke=0)
    c.roundRect(43, 679, 4.2, 5, 1, fill=1, stroke=0)
    txt(57, 681, "IntelliRoom", 8.5, INK, "UIBold")
    txt(W - 34, 682, section.upper(), 6.2, MUTED, "UIBold", "right")
    c.setStrokeColor(HexColor("#E9E0D8"))
    c.setLineWidth(0.65)
    c.line(34, 667, W - 34, 667)
    c.line(34, 31, W - 34, 31)
    txt(34, 18, "CONNECTED SYSTEMS PROTOTYPE  ·  DEMONSTRATION BUILD", 5.6, MUTED, "UI")
    txt(W - 34, 18, f"{number:02d} / 06", 6, TEAL, "UIBold", "right")


def heading(kicker, title, subtitle, title_y=611, title_size=24):
    txt(36, 641, kicker.upper(), 6.8, TEAL, "UIBold")
    fit_txt(36, title_y, title, 500, title_size, 19, INK, "UIBold")
    para(37, title_y - 21, subtitle, 496, 8.4, 11.4, MUTED)


def pill(x, y, label, fill=PALE_TEAL, color=TEAL, size=6.2):
    width = pdfmetrics.stringWidth(label, "UIBold", size) + 15
    c.setFillColor(fill)
    c.roundRect(x, y, width, 17, 8.5, fill=1, stroke=0)
    txt(x + width / 2, y + 5.4, label, size, color, "UIBold", "center")
    return width


def bar_card(x, y, w, h, number, title, body, accent, fill=CARD):
    rounded(x, y, w, h, fill, LINE, 9)
    c.setFillColor(accent)
    c.roundRect(x, y + 8, 4, h - 16, 2, fill=1, stroke=0)
    txt(x + 15, y + h - 20, number, 6.1, accent, "UIBold")
    txt(x + 15, y + h - 39, title, 9.4, INK, "UIBold")
    return para(x + 15, y + h - 56, body, w - 29, 7.8, 10.5, MUTED)


def photo_panel(x, y, w, h, image, title, caption):
    rounded(x, y, w, h, CARD, LINE, 9)
    fit_image(image, x + 10, y + 37, w - 20, h - 48, pad=2)
    txt(x + 11, y + 22, title, 6.8, INK, "UIBold")
    para(x + 11, y + 10, caption, w - 22, 5.8, 7.1, MUTED)


c = canvas.Canvas(str(OUT), pagesize=(W, H), pageCompression=1)
c.setTitle("IntelliRoom — Project Carousel")
c.setAuthor("IntelliRoom")
c.setSubject("Project design, interface and validation status")


# 01 — Use the exact supplied thumbnail as the hero visual, without cropping.
page_base("Project overview", 1)
cover = ASSETS / "intelliroom-thumbnail.jpg"
rounded(34, 361, 508, 284, CARD, HexColor("#E7EAE3"), 10)
fit_image(cover, 38, 365, 500, 276)
txt(38, 338, "LOCAL CONTROL  /  VISIBLE STATUS  /  ENERGY CONTEXT", 6.6, TEAL, "UIBold")
fit_txt(37, 308, "A clearer view of everyday loads", 500, 22, 19, INK, "UIBold")
para(38, 284, "IntelliRoom brings relay commands, operating state and configurable automation into one ESP32-hosted interface.", 495, 9, 12.5, MUTED)
px = 38
for label, fill, color in [
    ("LOCAL WEB CONSOLE", PALE_TEAL, TEAL),
    ("MANUAL + REMOTE MODES", PALE_AMBER, AMBER),
    ("METER INTEGRATION PENDING", PALE_BLUE, BLUE),
]:
    px += pill(px, 231, label, fill, color) + 8
rounded(38, 104, 500, 91, HexColor("#FFFAF5"), HexColor("#EADFD1"), 8, False)
txt(52, 174, "READ THIS BEFORE INTERPRETING THE ARTWORK", 6.6, TEAL, "UIBold")
para(52, 154, "The image is the supplied concept thumbnail. Values and channel states drawn inside it are illustrative artwork, not live PZEM readings or verified relay feedback. The meter is not currently connected.", 468, 8, 11.2, INK)
c.showPage()


# 02 — Challenge and project response, styled as the reference's vertical cards.
page_base("System challenge", 2)
heading("01 / The design constraint", "Keep command, measurement and feedback distinct", "The interface should make the state of the prototype clear without overstating what its sensors or outputs have confirmed.", title_y=610, title_size=24)
bar_card(35, 430, 506, 111, "01  ·  CONTROL SOURCES", "Different controls have different rules", "The four physical buttons invoke saved combinations only in Manual mode. Authenticated web controls remain available in both Manual and Remote modes.", CORAL)
bar_card(35, 296, 506, 111, "02  ·  OUTPUT STATE", "A requested state is not contact feedback", "The ESP32 sends a relay mask to the Uno over a one-way serial link. The current UI reflects the last commanded state; the Uno does not acknowledge contact position.", AMBER)
bar_card(35, 162, 506, 111, "03  ·  ENERGY DATA", "Measurement is a separate integration step", "The PZEM-004T and CT path is not connected right now. Graphs and meter values shown in dashboard screenshots are demo data, not measurements from the hardware.", BLUE)
txt(38, 108, "The project keeps these boundaries visible in the README and interface notes.", 7.2, TEAL, "UIBold")
c.showPage()


# 03 — Architecture diagram with actual controller boundaries and pins.
page_base("Architecture", 3)
heading("02 / System architecture", "A clear, two-controller design", "The ESP32 handles the web application and policy. The Arduino Uno receives a compact command and drives relay inputs.", title_y=611, title_size=25)

node_y, node_w, node_h = 470, 111, 67
node_x = [35, 165, 295, 425]
nodes = [
    ("WEB UI", "Browser request", PALE_BLUE, BLUE),
    ("ESP32", "Mode + scheduling", PALE_AMBER, AMBER),
    ("ARDUINO UNO", "Output driver", PALE_AMBER, AMBER),
    ("RELAY MODULE", "Switched outputs", HexColor("#F8ECEB"), CORAL),
]
for x, (name, sub, fill, accent) in zip(node_x, nodes):
    rounded(x, node_y, node_w, node_h, fill, LINE, 8, False)
    c.setFillColor(accent)
    c.roundRect(x, node_y + node_h - 6, node_w, 6, 3, fill=1, stroke=0)
    txt(x + node_w / 2, node_y + 36, name, 7.4, INK, "UIBold", "center")
    txt(x + node_w / 2, node_y + 17, sub, 6.4, MUTED, "UI", "center")
for left in node_x[:-1]:
    c.setStrokeColor(TEAL if left < 250 else AMBER)
    c.setLineWidth(1.4)
    tip_x = left + node_w + 17
    c.line(left + node_w + 3, node_y + 34, tip_x, node_y + 34)
    c.setFillColor(TEAL if left < 250 else AMBER)
    p = c.beginPath()
    p.moveTo(tip_x, node_y + 34)
    p.lineTo(tip_x - 6, node_y + 37)
    p.lineTo(tip_x - 6, node_y + 31)
    p.close()
    c.drawPath(p, fill=1, stroke=0)
txt(296, 451, "GPIO17 TX → Uno D0/RX  ·  9600 baud  ·  one-way", 6.3, MUTED, "UIBold", "center")

bar_card(35, 294, 246, 119, "INPUTS  ·  ESP32", "Buttons + mode selector", "Buttons: GPIO32, 33, 25 and 26. The selector uses GPIO27: HIGH is Remote; LOW is Manual.", TEAL)
bar_card(295, 294, 246, 119, "LOCAL DISPLAY + METER", "LCD and PZEM interfaces", "LCD I²C: SDA 21, SCL 22. PZEM UART: RX 16, TX 14. The PZEM/CT hardware is not currently connected.", BLUE)
rounded(35, 140, 506, 113, HexColor("#FFFAF5"), HexColor("#EADFD1"), 9, False)
txt(51, 228, "UNO OUTPUT MAP", 6.7, TEAL, "UIBold")
para(51, 208, "The Uno sketch maps its D2–D9 outputs to the relay module inputs. Low-voltage signal grounds share a reference. This graphic describes logic connections only; it is not an AC wiring diagram.", 474, 8.1, 11.4, INK)
txt(51, 161, "Keep mains wiring, protection and enclosure work under qualified electrical review.", 6.9, CORAL, "UIBold")
c.showPage()


# 04 — Real dashboard screenshots; all values are disclosed as mock/demo data.
page_base("The interface", 4)
heading("03 / Application", "A status-first interface with visible controls", "Home is informational. Individual relay actions live on Controls, alongside the larger power-draw chart.", title_y=611, title_size=24)

for x, label, note, asset, accent in [
    (35, "HOME", "Read-only status overview", ASSETS / "ui-home.png", BLUE),
    (295, "CONTROLS", "Individual relay actions + chart", ASSETS / "ui-controls.png", AMBER),
]:
    rounded(x, 333, 246, 204, CARD, LINE, 9)
    c.setFillColor(accent)
    c.roundRect(x, 333, 4, 204, 2, fill=1, stroke=0)
    txt(x + 15, 515, label, 6.8, accent, "UIBold")
    txt(x + 15, 496, note, 8.1, INK, "UIBold")
    rounded(x + 11, 352, 224, 129, HexColor("#F8FAF9"), LINE, 4, False)
    fit_image(asset, x + 14, 355, 218, 123)

rounded(35, 153, 506, 143, CARD, LINE, 9)
txt(51, 274, "WHAT THE TECHNICIAN SEES", 6.7, TEAL, "UIBold")
bar_card(47, 188, 228, 72, "HOME", "Status only", "Link, mode, commanded relay state and available energy data.", BLUE, PALE_BLUE)
bar_card(293, 188, 234, 72, "CONTROLS", "Actions + power draw", "Per-channel commands with a whole-cell state highlight.", AMBER, PALE_AMBER)
pill(51, 164, "SCREENSHOTS CONTAIN MOCK DATA", PALE_AMBER, AMBER, 6)
txt(40, 117, "PZEM readings should remain unavailable until the meter is connected and verified.", 7.2, MUTED, "UI")
c.showPage()


# 05 — Setup and automation, using the supplied configuration screen.
page_base("Configuration", 5)
heading("04 / Technician setup", "Configure groups, schedules and timers", "Technician settings are saved on the ESP32 and can be changed from the responsive web console.", title_y=611, title_size=24)

for x, label, image, accent in [
    (35, "BUTTON + GROUP MAPPING", ASSETS / "ui-button-mapping.png", AMBER),
    (295, "SCHEDULES + TIMERS", ASSETS / "ui-schedules.png", BLUE),
]:
    rounded(x, 381, 246, 169, CARD, LINE, 9)
    c.setFillColor(accent)
    c.roundRect(x, 381, 4, 169, 2, fill=1, stroke=0)
    txt(x + 14, 529, label, 6.7, accent, "UIBold")
    rounded(x + 10, 397, 224, 112, HexColor("#F7F9F8"), LINE, 4, False)
    fit_image(image, x + 13, 400, 218, 106)

bar_card(35, 198, 158, 150, "01 / NAME", "Channels + groups", "Rename channels, select a custom relay combination and assign that combination to a physical button.", TEAL, CARD)
bar_card(209, 198, 158, 150, "02 / SCHEDULE", "Start + end actions", "Choose weekdays, a start and end time, relays, and the ON or OFF action at each boundary.", AMBER, CARD)
bar_card(383, 198, 158, 150, "03 / TIMER", "Choose a duration", "Set a selected channel to turn off after a duration in seconds, minutes or hours.", BLUE, CARD)
rounded(35, 111, 506, 58, HexColor("#FFFAF5"), HexColor("#EADFD1"), 8, False)
para(49, 146, "Physical buttons operate only in Manual mode. Web control remains available in both Manual and Remote modes. Clock-based schedules need Wi‑Fi time sync.", 476, 7.7, 10.2, INK)
c.showPage()


# 06 — Actual hardware context, current verification boundaries, and safe next steps.
page_base("Prototype status", 6)
heading("05 / Prototype + next steps", "Bench assembly first. Verification next.", "The photographs show the demonstration hardware. Relay behavior, push-button behavior and the disconnected meter path must be verified on the actual build.", title_y=611, title_size=24)

photo_panel(35, 409, 246, 153, ASSETS / "prototype-controller.jpg", "CONTROLLER ASSEMBLY", "ESP32 · Uno · relay module · LCD")
photo_panel(295, 409, 246, 153, ASSETS / "prototype-demo-box.jpg", "SEPARATE LOAD DEMONSTRATOR", "Socket junction box · 16 A MCB")

bar_card(35, 218, 158, 158, "ASSEMBLED", "Demo hardware", "Photographs show a bench enclosure and separate socket junction box. This is not a household switch-box installation.", BLUE, CARD)
bar_card(209, 218, 158, 158, "VERIFY", "Relay + buttons", "Check Uno relay outputs and confirm each physical button works only in Manual mode before connecting loads.", AMBER, CARD)
bar_card(383, 218, 158, 158, "PENDING", "PZEM + CT", "Meter and CT are not connected. Verify terminal markings and readings before showing real energy data.", BLUE, CARD)

rounded(35, 94, 506, 93, HexColor("#FFF3EB"), HexColor("#F0D9CC"), 9, False)
txt(51, 164, "SAFETY + FUTURE WORK", 6.7, CORAL, "UIBold")
para(51, 144, "Next: verify low-voltage relay and button operation, connect and calibrate the PZEM path, then add relay acknowledgement if confirmed contact state is needed. The 16 A MCB alone does not establish safety; have all AC wiring and enclosure work reviewed by a qualified electrician before energizing.", 474, 7.65, 10.1, INK)
c.showPage()

c.save()
print(f"Created {OUT}")
