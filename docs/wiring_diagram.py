"""
Generate a simple vector-style PDF wiring diagram showing Arduino connections
for four HBS86H motor drivers and their associated limit switches.
This script avoids external dependencies by writing the PDF primitives directly.
"""
from pathlib import Path

STEP_PINS = [22, 24, 26, 28]
DIR_PINS = [23, 25, 27, 29]
EN_PINS = [30, 31, 32, 33]
LIMIT_PINS = [34, 36, 38, 40]

MEDIA_BOX = (0, 0, 792, 612)  # landscape letter (x1, y1, x2, y2)


def escape_text(text: str) -> str:
    return text.replace('\\', r'\\').replace('(', r'\\(').replace(')', r'\\)')


def rect(x, y, w, h):
    return f"{x} {y} {w} {h} re S\n"


def line(x1, y1, x2, y2):
    return f"{x1} {y1} m {x2} {y2} l S\n"


def text(x, y, message, size=12):
    return f"BT /F1 {size} Tf {x} {y} Td ({escape_text(message)}) Tj ET\n"


def build_contents():
    contents = []
    contents.append("0.5 w\n0 0 0 RG 0 0 0 rg\n")

    # Arduino block
    contents.append(rect(40, 260, 180, 160))
    contents.append(text(130, 400, "Arduino Mega 2560", 14))
    contents.append(text(60, 360, "Step: 22, 24, 26, 28"))
    contents.append(text(60, 340, "Dir: 23, 25, 27, 29"))
    contents.append(text(60, 320, "Enable: 30 - 33"))
    contents.append(text(60, 300, "Limit inputs: 34, 36, 38, 40 (NC)"))
    contents.append(text(60, 280, "5V -> limit COM, GND -> driver - pins", 10))

    # Driver boxes and limit switches
    start_x = 260
    spacing = 130
    driver_w, driver_h = 110, 90
    limit_h = 60

    for idx in range(4):
        x = start_x + spacing * idx
        driver_y = 330
        limit_y = 200
        contents.append(rect(x, driver_y, driver_w, driver_h))
        contents.append(text(x + 10, driver_y + 70, f"HBS86H Driver A{idx+1}", 12))
        contents.append(text(x + 10, driver_y + 50, f"PUL+ <- STEP {STEP_PINS[idx]}", 10))
        contents.append(text(x + 10, driver_y + 35, f"DIR+ <- DIR {DIR_PINS[idx]}", 10))
        contents.append(text(x + 10, driver_y + 20, f"ENA+ <- EN {EN_PINS[idx]}", 10))
        contents.append(text(x + 10, driver_y + 5, "PUL-/DIR-/ENA- -> GND", 10))

        # lines from Arduino block to driver block
        contents.append(line(220, 380 - idx * 5, x, driver_y + driver_h - 10))

        # limit switch box
        contents.append(rect(x, limit_y, driver_w, limit_h))
        contents.append(text(x + 10, limit_y + 40, f"Limit Switch A{idx+1}", 11))
        contents.append(text(x + 10, limit_y + 25, f"NC -> pin {LIMIT_PINS[idx]}", 10))
        contents.append(text(x + 10, limit_y + 10, "COM -> GND, NO unused", 10))

        # dashed line (simulate) from limit to Arduino
        contents.append("[3] 0 d\n")
        contents.append(line(x + driver_w / 2, limit_y + limit_h, 130, 310 + idx * 8))
        contents.append("[] 0 d\n")

    contents.append(text(396, 120, "Each driver output connects to one NEMA34 actuator motor.\nFollow HBS86H manual for motor and supply wiring.", 10))
    return "".join(contents)


def build_pdf(content: str) -> bytes:
    content_bytes = content.encode('utf-8')
    length = len(content_bytes)
    parts = []
    parts.append(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    parts.append(b"1 0 obj<< /Type /Catalog /Pages 2 0 R >>endobj\n")
    parts.append(b"2 0 obj<< /Type /Pages /Kids [3 0 R] /Count 1 >>endobj\n")
    media = f"[{MEDIA_BOX[0]} {MEDIA_BOX[1]} {MEDIA_BOX[2]} {MEDIA_BOX[3]}]"
    parts.append(f"3 0 obj<< /Type /Page /Parent 2 0 R /MediaBox {media} /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>endobj\n".encode('utf-8'))
    parts.append(f"4 0 obj<< /Length {length} >>stream\n".encode('utf-8'))
    parts.append(content_bytes)
    parts.append(b"endstream endobj\n")
    parts.append(b"5 0 obj<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>endobj\n")

    # xref
    offset_list = []
    current = 0
    full = b"".join(parts)
    # Need offsets for xref, compute iteratively
    parts_with_offsets = []
    for p in parts:
        parts_with_offsets.append((current, p))
        current += len(p)
    xref_start = current
    xref_lines = [b"xref\n", b"0 6\n", b"0000000000 65535 f \n"]
    for offset, _ in parts_with_offsets:
        xref_lines.append(f"{offset:010d} 00000 n \n".encode('utf-8'))
    trailer = f"trailer<< /Size 6 /Root 1 0 R >>\nstartxref\n{xref_start}\n%%EOF".encode('utf-8')
    return b"".join([p for _, p in parts_with_offsets] + xref_lines + [trailer])


def main():
    content = build_contents()
    pdf_bytes = build_pdf(content)
    out_path = Path(__file__).with_name('wiring-diagram.pdf')
    out_path.write_bytes(pdf_bytes)
    print(f"Wiring diagram saved to {out_path}")


if __name__ == "__main__":
    main()
