"""Create the detailed, print-ready IntelliRoom A3 poster."""
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A3, landscape
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.utils import ImageReader
from PIL import Image
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
OUT = DOCS / "IntelliRoom-A3-Project-Poster.pdf"
PREVIEW = DOCS / "IntelliRoom-A3-Project-Poster-Preview.png"
ASSET_DIR = DOCS / "poster_assets"
ASSET_DIR.mkdir(parents=True, exist_ok=True)
SOURCE = {
    "crest": ASSET_DIR / "college-crest.png",
    "mvp_a": ASSET_DIR / "mvp_a.jpg",
    "mvp_b": ASSET_DIR / "mvp_b.jpg",
}
ASSET = {}
for key, src in SOURCE.items():
    if not src.exists():
        raise FileNotFoundError(f"Required packaged poster asset is missing: {src}")
    ASSET[key] = src

FONTS = Path(r"C:\Windows\Fonts")
pdfmetrics.registerFont(TTFont("Segoe", str(FONTS / "segoeui.ttf")))
pdfmetrics.registerFont(TTFont("SegoeBold", str(FONTS / "segoeuib.ttf")))
pdfmetrics.registerFont(TTFont("SegoeLight", str(FONTS / "segoeuil.ttf")))

W, H = landscape(A3)
NAVY = HexColor("#11253B")
NAVY2 = HexColor("#1C3854")
INK = HexColor("#1C2C3B")
MUTED = HexColor("#53697C")
PAPER = HexColor("#F3F6F7")
WHITE = white
TEAL = HexColor("#0B9F9A")
TEAL_PALE = HexColor("#E3F3F1")
GOLD = HexColor("#E9A72E")
GOLD_PALE = HexColor("#FFF2D5")
LINE = HexColor("#D4DFE5")
BLUE = HexColor("#6086C1")

c = canvas.Canvas(str(OUT), pagesize=(W, H), pageCompression=1)
c.setTitle("IntelliRoom - Detailed A3 Project Poster")
c.setAuthor("Department of Computer Science and Engineering, AAA College of Engineering and Technology")
c.setSubject("Detailed system design, prototype, control logic and validation status")

def txt(x, y, value, size=8, color=INK, font="Segoe", align="left"):
    c.setFont(font, size); c.setFillColor(color)
    if align == "center": c.drawCentredString(x, y, value)
    elif align == "right": c.drawRightString(x, y, value)
    else: c.drawString(x, y, value)

def wrap(value, width, font, size):
    out, line = [], ""
    for word in value.split():
        nxt = word if not line else line + " " + word
        if pdfmetrics.stringWidth(nxt, font, size) <= width: line = nxt
        else:
            if line: out.append(line)
            line = word
    if line: out.append(line)
    return out

def para(x, y, value, width, size=7.6, leading=10, color=INK, font="Segoe", max_lines=None):
    lines = wrap(value, width, font, size)
    if max_lines and len(lines) > max_lines:
        lines = lines[:max_lines]
        while lines[-1] and pdfmetrics.stringWidth(lines[-1] + "...", font, size) > width:
            lines[-1] = lines[-1][:-1]
        lines[-1] += "..."
    for i, line in enumerate(lines): txt(x, y-i*leading, line, size, color, font)
    return y-len(lines)*leading

def rounded(x, y, w, h, fill, stroke=None, r=7, sw=.7):
    c.setFillColor(fill); c.setStrokeColor(stroke or fill); c.setLineWidth(sw)
    c.roundRect(x, y, w, h, r, fill=1, stroke=1 if stroke else 0)

def card(x, y, w, h, title, label, accent=TEAL):
    rounded(x, y, w, h, WHITE, LINE, 8)
    c.setFillColor(accent); c.roundRect(x, y+h-4, w, 4, 2, fill=1, stroke=0)
    txt(x+12, y+h-15, label.upper(), 5.8, accent, "SegoeBold")
    txt(x+12, y+h-29, title, 10.2, NAVY, "SegoeBold")
    return y+h-42

def dot_bullet(x, y, heading, body, width, accent=TEAL, size=7.25, leading=9.1):
    c.setFillColor(accent); c.circle(x+2, y+2, 1.8, fill=1, stroke=0)
    txt(x+9, y, heading, size, NAVY, "SegoeBold")
    lines = wrap(body, width-10, "Segoe", size)
    for i, line in enumerate(lines): txt(x+9, y-(i+1)*leading, line, size, MUTED)
    return y-(len(lines)+1)*leading-3

def fit_image(path, x, y, w, h):
    im = Image.open(path).convert("RGB")
    iw, ih = im.size; scale = min(w/iw, h/ih)
    dw, dh = iw*scale, ih*scale
    c.drawImage(ImageReader(im), x+(w-dw)/2, y+(h-dh)/2, dw, dh, mask="auto")

def cover_image(path, x, y, w, h):
    im = Image.open(path).convert("RGB")
    iw, ih = im.size
    if iw/ih > w/h:
        nw = int(ih*w/h); left = (iw-nw)//2; im = im.crop((left, 0, left+nw, ih))
    else:
        nh = int(iw*h/w); top = (ih-nh)//2; im = im.crop((0, top, iw, top+nh))
    c.saveState(); p = c.beginPath(); p.roundRect(x, y, w, h, 5); c.clipPath(p, stroke=0)
    c.drawImage(ImageReader(im), x, y, w, h, mask="auto")
    c.restoreState(); c.setStrokeColor(LINE); c.setLineWidth(.6); c.roundRect(x, y, w, h, 5, fill=0, stroke=1)

# Ground and institutional masthead.
c.setFillColor(PAPER); c.rect(0, 0, W, H, fill=1, stroke=0)
M = 24
fit_image(ASSET["crest"], M, H-78, 57, 57)
cx = W/2
txt(cx, H-34, "AAA COLLEGE OF ENGINEERING AND TECHNOLOGY", 16, NAVY, "SegoeBold", "center")
txt(cx, H-49, "(AN AUTONOMOUS INSTITUTION)", 8, TEAL, "SegoeBold", "center")
txt(cx, H-63, "Approved by AICTE, New Delhi & Anna University, Chennai  |  Amathur, Sivakasi - 626 005", 7, MUTED, "Segoe", "center")
c.setStrokeColor(LINE); c.line(M, H-87, W-M, H-87)
rounded(M, H-110, W-2*M, 16, NAVY, r=5)
txt(W/2, H-105, "DEPARTMENT OF COMPUTER SCIENCE AND ENGINEERING", 7.6, WHITE, "SegoeBold", "center")

# Project title and cohort.
hy = H-194
rounded(M, hy, W-2*M, 76, NAVY, r=12)
c.setFillColor(TEAL); c.roundRect(M, hy, 7, 76, 4, fill=1, stroke=0)
txt(M+21, hy+49, "INTELLIROOM", 24, WHITE, "SegoeBold")
txt(M+23, hy+29, "IoT Wireless Actuation Hub for Smart Energy Monitoring and Control", 11.2, HexColor("#D4E9EE"), "Segoe")
txt(M+23, hy+12, "EIGHT-CHANNEL EMBEDDED CONTROL  /  RESPONSIVE WEB CONSOLE  /  DEMONSTRATION MVP", 6.1, HexColor("#B8C9D7"), "SegoeBold")
rounded(W-181, hy+13, 143, 50, NAVY2, HexColor("#365671"), 8)
txt(W-109.5, hy+43, "8 RELAY CHANNELS", 7, HexColor("#FFD278"), "SegoeBold", "center")
txt(W-109.5, hy+25, "ESP32 + UNO", 10.5, WHITE, "SegoeBold", "center")

ty = hy-36
rounded(M, ty, W-2*M, 27, WHITE, LINE, 6)
txt(M+11, ty+17, "PROJECT MEMBERS", 5.6, TEAL, "SegoeBold")
txt(M+11, ty+6, "Suraj Kumar  |  Santhosh.R  |  Sam Sankar.P  |  Sivakumar.M  |  Rithick.C  |  Vittal.R", 7.0, NAVY, "SegoeBold")
c.setStrokeColor(LINE); c.line(W-254, ty+4, W-254, ty+23)
txt(W-243, ty+17, "PROJECT GUIDE", 5.6, TEAL, "SegoeBold")
txt(W-243, ty+6, "Mr. S. Parameswaran  |  AP/CSE", 7, NAVY, "SegoeBold")

# Three compact but content-heavy columns.
gap = 10
colw = (W-2*M-2*gap)/3
x1, x2, x3 = M, M+colw+gap, M+2*(colw+gap)
body_top = ty-8

# Column 1: project intent, architecture and exact low-voltage pin mapping.
h1, h2, h3 = 157, 197, 151
y1 = body_top-h1
cy = card(x1, y1, colw, h1, "Project context & objectives", "01 / Problem and purpose", TEAL)
cy = para(x1+12, cy, "Conventional switching gives users little remote visibility or flexible control. IntelliRoom explores a compact, locally hosted IoT controller that combines relay switching, configurable control groups and an energy-metering interface in one demonstrator.", colw-24, 7.3, 9.2)
cy -= 4
cy = dot_bullet(x1+12, cy, "Design objective", "Build an eight-channel prototype with browser-based control and local physical inputs.", colw-24, TEAL, 7.0, 8.7)
cy = dot_bullet(x1+12, cy, "Observe and automate", "Present commanded channel states, meter data when available, schedules, timers and recent activity.", colw-24, TEAL, 7.0, 8.7)
cy = dot_bullet(x1+12, cy, "Technician flexibility", "Allow channel names, relay combinations and button mappings to be customised and saved.", colw-24, TEAL, 7.0, 8.7)

y2 = y1-9-h2
cy = card(x1, y2, colw, h2, "System architecture & signal flow", "02 / Two-controller design", GOLD)
# Architecture boxes and directional arrows.
boxes = [("WEB CLIENT", "phone / PC"), ("ESP32", "policy + Wi-Fi"), ("UNO", "relay driver"), ("8-CH RELAY", "channels 1-8")]
bx0=x1+11; by=cy-30; bw=(colw-22-18)/4; bg=6
for i,(a,b) in enumerate(boxes):
    bx=bx0+i*(bw+bg)
    fill=HexColor("#E8F3F5") if i<2 else HexColor("#F1F3F4")
    rounded(bx,by,bw,27,fill,LINE,4)
    txt(bx+bw/2,by+16,a,5.3,NAVY,"SegoeBold","center")
    txt(bx+bw/2,by+6,b,5.0,MUTED,"Segoe","center")
    if i<3:
        c.setStrokeColor(TEAL if i<2 else GOLD); c.setLineWidth(1.1)
        c.line(bx+bw+1,by+13,bx+bw+bg-1,by+13)
        c.line(bx+bw+bg-4,by+15,bx+bw+bg-1,by+13); c.line(bx+bw+bg-4,by+11,bx+bw+bg-1,by+13)
cy=by-14
cy=dot_bullet(x1+12,cy,"Command path","The ESP32 assembles the eight-bit relay mask and sends an ASCII command at 9600 baud over a one-way serial link. The Uno maps bits 0-7 to D2-D9 and applies active-low relay outputs.",colw-24,TEAL,6.8,8.4)
cy=dot_bullet(x1+12,cy,"Meter path","The PZEM-004T v3 communicates with ESP32 UART2. Voltage, current, power, cumulative energy, frequency and power factor are polled every 2 seconds when the meter is available.",colw-24,GOLD,6.8,8.4)
cy=dot_bullet(x1+12,cy,"Display and inputs","A 20×4 I²C LCD reports status; buttons, mode switch and meter are read by the ESP32. Relay state shown by the app is commanded state: the Uno does not send feedback acknowledgements.",colw-24,TEAL,6.8,8.4)

y3=y2-9-h3
cy=card(x1,y3,colw,h3,"Controller pins & interconnects","03 / Firmware pin map",TEAL)
pinrows=[
    ("ESP32 GPIO17 TX  →  Uno D0/RX", "9600-baud command link; common GND."),
    ("Uno D2-D9  →  Relay IN1-IN8", "Channel 1 maps to D2; channel 8 to D9."),
    ("ESP32 GPIO21 / GPIO22", "LCD SDA / SCL; configured I²C address 0x27."),
    ("ESP32 GPIO16 RX / GPIO14 TX", "PZEM TX / RX respectively; UART at 9600 baud."),
    ("ESP32 GPIO32, 33, 25, 26", "Four active-low push buttons; each button closes to GND."),
    ("ESP32 GPIO27", "Manual / Remote mode input; LOW selects Manual."),
    ("5 V supply and ground", "Use regulated 5 V for specified modules; share logic ground; keep mains wiring segregated."),
]
for i,(head,body) in enumerate(pinrows):
    yy=cy-i*15.0
    txt(x1+12,yy,head,6.4,NAVY,"SegoeBold")
    txt(x1+12,yy-8,body,5.9,MUTED)

# Column 2: human-readable behavior, dashboard and implementation method.
h1b,h2b,h3b=182,198,145
y1b=body_top-h1b
cy=card(x2,y1b,colw,h1b,"Operating logic & control rules","04 / Modes, commands and safeguards",GOLD)
cy=dot_bullet(x2+12,cy,"REMOTE mode (GPIO27 HIGH)","Web controls work. Schedules and automation can run when time is synchronised. Physical relay buttons are ignored.",colw-24,GOLD,7.0,8.8)
cy=dot_bullet(x2+12,cy,"MANUAL mode (GPIO27 LOW)","Four local buttons toggle their saved relay combinations. Web control remains available in both modes.",colw-24,TEAL,7.0,8.8)
cy=dot_bullet(x2+12,cy,"Configurable combinations","Each button is mapped to selected channels; a press turns its selection ON when all are OFF, otherwise OFF. Names and masks are stored in ESP32 Preferences.",colw-24,GOLD,7.0,8.8)
cy=dot_bullet(x2+12,cy,"Time-based control","Four weekly schedule slots have start/end times, weekdays, channels and separate ON/OFF actions. Per-channel timers support up to 24 hours. NTP time is needed for schedules and timers.",colw-24,TEAL,7.0,8.8)
cy=dot_bullet(x2+12,cy,"Command precedence","Human commands replace earlier human state. Automation yields to explicit human control; the all-off lock and safety interlock block automated or ON commands as applicable.",colw-24,GOLD,7.0,8.8)

y2b=y1b-9-h2b
cy=card(x2,y2b,colw,h2b,"Web console, firmware & data","05 / User-facing capabilities",TEAL)
feature_rows=[
    ("Responsive dashboard", "Five areas: read-only Home, relay Controls with power graph, Setup, Activity and About."),
    ("Individual + grouped control", "Eight named channels; custom groups and button combinations are selected as channel sets."),
    ("Schedules and timers", "Technician configures weekdays, start/end time, relay selection, action and per-channel duration."),
    ("Energy interface", "PZEM fields: V, A, W, kWh, Hz and power factor. Dashboard chart has no valid readings until the meter is connected and verified."),
    ("LCD status pages", "20×4 display rotates through live summary, relay mask, electrical readings, energy, and Wi-Fi/IP/clock; boot includes button diagnostics."),
    ("Persistence and activity", "Names, group/button maps and schedules use ESP32 non-volatile Preferences. A 24-entry recent activity ring is held in RAM."),
    ("Access and network", "Role-based technician/teacher login and an ESP32-hosted HTTP dashboard on the local Wi-Fi network; credentials must be set in config.h."),
]
for i,(head,body) in enumerate(feature_rows):
    yy=cy-i*22.1
    txt(x2+12,yy,head,6.8,NAVY,"SegoeBold")
    para(x2+12,yy-8,body,colw-24,6.45,7.7,MUTED,max_lines=2)

y3b=y2b-9-h3b
cy=card(x2,y3b,colw,h3b,"Methodology & build sequence","06 / Development workflow",GOLD)
workflow=[
    "Define eight-channel scope, manual/remote behavior and demo enclosure requirements.",
    "Allocate ESP32 coordination and UI; keep the Uno focused on deterministic relay outputs.",
    "Develop UART mask protocol, dashboard API, saved settings, schedules and LCD rotation.",
    "Assemble the demonstrator with controller, relay board, 8-socket box and protection device.",
    "Bring up one stage at a time: logic pins → serial link → relay LEDs → meter readings → enclosed load checks.",
]
for i,item in enumerate(workflow):
    yy=cy-i*17.5
    c.setFillColor(TEAL if i%2==0 else GOLD); c.circle(x2+15,yy+2,5,fill=1,stroke=0)
    txt(x2+15,yy, str(i+1),5.1,WHITE,"SegoeBold","center")
    para(x2+25,yy+1,item,colw-38,6.6,8.0,INK,max_lines=2)

# Column 3: supplied prototype imagery, validated facts, limitations, next steps.
h1c,h2c,h3c=210,170,145
y1c=body_top-h1c
cy=card(x3,y1c,colw,h1c,"Minimum viable prototype","07 / Assembled demonstration hardware",TEAL)
igap=7; iw=(colw-30-igap)/2; iy=cy-91
cover_image(ASSET["mvp_a"],x3+12,iy,iw,83)
cover_image(ASSET["mvp_b"],x3+12+iw+igap,iy,iw,83)
txt(x3+12,iy-9,"Controller panel and eight-socket demonstration box",5.8,MUTED)
para(x3+12,iy-22,"The photographs show the prototype assembly: ESP32 Dev Module, Arduino UNO, 8-channel relay module, 20×4 LCD, push buttons, PZEM interface, buck converter, 8-socket junction box and 16 A MCB. This is a demo rig; it is not installed in a household switch box.",colw-24,6.8,8.7,INK,max_lines=5)

y2c=y1c-9-h2c
cy=card(x3,y2c,colw,h2c,"Results & validation status","08 / What the prototype demonstrates",GOLD)
cy=dot_bullet(x3+12,cy,"System implementation","The ESP32 and Uno roles are separated; eight channels have a defined mask-to-pin mapping, and the web/LCD control and configuration paths are implemented.",colw-24,TEAL,6.85,8.4)
cy=dot_bullet(x3+12,cy,"Honest state reporting","The UI reflects the last command sent. Because the Uno relay controller has no return channel, this is not independent confirmation that a relay physically changed state.",colw-24,GOLD,6.85,8.4)
cy=dot_bullet(x3+12,cy,"Pending validation","At this demo stage, reliable end-to-end relay/button operation, PZEM readings and loaded mains operation still require controlled verification. No measured energy saving is claimed.",colw-24,TEAL,6.85,8.4)

y3c=y2c-9-h3c
cy=card(x3,y3c,colw,h3c,"Benefits, next steps & safety","09 / Impact and responsible deployment",GOLD)
txt(x3+12,cy,"POTENTIAL BENEFITS",6.0,TEAL,"SegoeBold")
para(x3+12,cy-9,"Centralised status, configurable multi-relay actions, time-based operation and energy visibility can make a small-space control setup easier to manage and study.",colw-24,6.65,8.2,INK,max_lines=3)
txt(x3+12,cy-39,"FUTURE WORK",6.0,TEAL,"SegoeBold")
para(x3+12,cy-48,"Validate meter calibration; add relay feedback; test outage recovery and security; record repeatable load trials; then design an enclosed, standards-compliant installation.",colw-24,6.65,8.2,INK,max_lines=3)
txt(x3+12,y3c+12,"SAFETY: Mains wiring, MCB selection and enclosure require qualified-person review before energising loads.",5.9,HexColor("#9A4B2C"),"SegoeBold")

# Footer: important system limitation stated at readable size.
fy=22
c.setFillColor(NAVY); c.rect(0,0,W,fy+9,fill=1,stroke=0)
txt(M,fy+5,"INTELLIROOM  |  IV YEAR CSE PROJECT  |  AAA COLLEGE OF ENGINEERING AND TECHNOLOGY",6.2,WHITE,"SegoeBold")
txt(W-M,fy+5,"Demo prototype only  •  No claim of verified household installation or measured energy savings",6.0,HexColor("#F7DDA8"),"Segoe","right")

c.showPage(); c.save()
subprocess.run(["pdftoppm","-f","1","-singlefile","-png","-r","120",str(OUT),str(PREVIEW.with_suffix(""))],check=True,capture_output=True)
print(f"Created {OUT}")
print(f"Created {PREVIEW}")
