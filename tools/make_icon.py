# -*- coding: utf-8 -*-
"""Erzeugt das Spielsymbol von "Kek Me If You Can": eine Dose ("can") mit verschmitzten Augen – ein Prop, der sich versteckt.

Ausgabe:
  res/icon.ico          Windows-Symbol (16–256 px, transparent), wird in die EXE eingebettet
  res/icon_1024.png     quadratisch mit Hintergrund – für Discord (Rich Presence Asset "logo") und Stores

Aufruf:  python tools/make_icon.py [vorschau.png]
Benötigt nur Pillow.
"""
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'Fonts')

SS = 2            # Supersampling
N = 1024 * SS     # Arbeitsgröße

AMBER = (255, 182, 39)
ORANGE = (255, 122, 26)
NAVY = (22, 27, 40)
INK = (12, 14, 20)
SILVER = (196, 203, 214)


def s(v):
    return int(round(v * SS))


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def clamp(v):
    return max(0, min(255, int(round(v))))


def cylinder_strip(width, base_top, base_bottom, height, spec=0.85, ambient=0.38):
    """Zylinder-Schattierung: Licht von links vorn, harte Glanzlinie links, weiches Gegenlicht rechts."""
    lx, lz = -0.55, 0.83
    strip = Image.new('RGB', (width, height))
    px = strip.load()
    for x in range(width):
        u = (x + 0.5) / width * 2 - 1
        nz = math.sqrt(max(0.0, 1 - u * u))
        diffuse = max(0.0, u * lx + nz * lz)
        edge = (1 - u * u) ** 0.12
        hi = math.exp(-((u + 0.42) / 0.075) ** 2) * spec + math.exp(-((u + 0.42) / 0.22) ** 2) * spec * 0.25
        rim = math.exp(-((u - 0.68) / 0.11) ** 2) * 0.22
        for y in range(height):
            t = y / max(1, height - 1)
            base = mix(base_top, base_bottom, t)
            k = (ambient + 0.78 * diffuse) * edge
            px[x, y] = tuple(clamp(c * k + 255 * (hi + rim)) for c in base)
    return strip


def ellipse_box(cx, cy, rx, ry):
    return [cx - rx, cy - ry, cx + rx, cy + ry]


def font(names, size, variation=None):
    for n in names:
        p = os.path.join(FONTS, n)
        if os.path.exists(p):
            f = ImageFont.truetype(p, size)
            if variation:
                try:
                    f.set_variation_by_name(variation)
                except Exception:
                    pass
            return f
    return ImageFont.load_default()


def build_can():
    """Liefert (Dosen-Ebene RGBA, Silhouette L) in Arbeitsgröße, aufrecht."""
    cx = N // 2
    r = s(212)          # Radius Rumpf
    rn = s(178)         # Radius Deckel (Schulter verjüngt)
    e = s(58)           # Ellipsen-Halbhöhe (Blick leicht von oben)
    y_lid = s(196)
    y_top = s(262)
    y_bot = s(812)
    y_foot = s(846)
    rf = s(190)

    can = Image.new('RGBA', (N, N), (0, 0, 0, 0))

    def paint(mask, fill_img):
        can.paste(fill_img, (0, 0), mask)

    def full_strip(radius, top, bottom, y0, y1, spec=0.85, ambient=0.38):
        img = Image.new('RGB', (N, N))
        st = cylinder_strip(2 * radius, top, bottom, 1, spec, ambient).resize((2 * radius, max(1, y1 - y0)))
        if top != bottom:
            st = cylinder_strip(2 * radius, top, bottom, max(2, (y1 - y0) // 8), spec, ambient).resize((2 * radius, max(1, y1 - y0)), Image.BILINEAR)
        img.paste(st, (cx - radius, y0))
        return img

    # Fuß (silbern, verjüngt)
    m = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(m)
    d.polygon([(cx - r, y_bot), (cx + r, y_bot), (cx + rf, y_foot), (cx - rf, y_foot)], fill=255)
    d.ellipse(ellipse_box(cx, y_foot, rf, int(e * 0.9)), fill=255)
    d.ellipse(ellipse_box(cx, y_bot, r, e), fill=255)
    paint(m, full_strip(r, (150, 156, 168), (112, 118, 130), y_bot - e, y_foot + e, 0.7, 0.42))

    # Schulter (silbern) – oberhalb der vorderen Rumpfkante
    m = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(m)
    d.polygon([(cx - rn, y_lid), (cx + rn, y_lid), (cx + r, y_top), (cx - r, y_top)], fill=255)
    d.ellipse(ellipse_box(cx, y_top, r, e), fill=255)
    d.ellipse(ellipse_box(cx, y_lid, rn, int(e * 0.86)), fill=255)
    paint(m, full_strip(r, (214, 220, 230), (168, 175, 188), y_lid - e, y_top + e, 0.9, 0.45))

    # Rumpf (Bernstein -> Orange), Oberkante folgt der vorderen Ellipse
    body = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(body)
    d.rectangle([cx - r, y_top, cx + r, y_bot], fill=255)
    d.ellipse(ellipse_box(cx, y_bot, r, e), fill=255)
    d.ellipse(ellipse_box(cx, y_top, r, e), fill=0)
    paint(body, full_strip(r, (255, 196, 64), ORANGE, y_top - e, y_bot + e))

    # Banderole (dunkel) mit Schriftzug, um den Zylinder gewickelt
    band_y0, band_y1 = s(548), s(716)
    band = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(band)
    # Ober- und Unterkante als Ellipsenbögen (vordere Hälfte)
    d.rectangle([cx - r, band_y0, cx + r, band_y1], fill=255)
    d.ellipse(ellipse_box(cx, band_y1, r, e), fill=255)
    d.ellipse(ellipse_box(cx, band_y0, r, e), fill=0)
    band = ImageChops.multiply(band, body)
    paint(band, full_strip(r, (40, 47, 66), NAVY, band_y0, band_y1 + e, 0.55, 0.5))
    # goldene Zierlinien an den Kanten
    for yy, w in ((band_y0, s(7)), (band_y1, s(7))):
        line = Image.new('L', (N, N), 0)
        dl = ImageDraw.Draw(line)
        dl.arc(ellipse_box(cx, yy, r, e), 0, 180, fill=255, width=w)
        line = ImageChops.multiply(line, body)
        paint(line, full_strip(r, (255, 214, 110), (255, 190, 70), yy - e, yy + e + s(10), 0.6, 0.55))

    # Schriftzug "KEK", flach gesetzt und dann zylindrisch verzerrt
    text_h = band_y1 - band_y0
    flat_w = int(math.pi * r)  # halber Umfang = sichtbare Vorderseite
    flat = Image.new('L', (flat_w, text_h + 2 * e), 0)
    f = font(['bahnschrift.ttf'], s(150), 'Bold')
    df = ImageDraw.Draw(flat)
    tb = df.textbbox((0, 0), 'KEK', font=f)
    tw, th = tb[2] - tb[0], tb[3] - tb[1]
    df.text(((flat_w - tw) / 2 - tb[0], (text_h - th) / 2 - tb[1] + e * 0.55), 'KEK', font=f, fill=255)
    warped = Image.new('L', (N, N), 0)
    for x in range(cx - r + 1, cx + r):
        u = (x - cx) / r
        theta = math.asin(max(-1.0, min(1.0, u)))  # -pi/2..pi/2
        sx = int((theta / math.pi + 0.5) * (flat_w - 1))
        col = flat.crop((sx, 0, sx + 1, flat.size[1]))
        # Vertikal folgt der Text der Ellipsenkrümmung
        dy = int(e * math.sqrt(max(0.0, 1 - u * u)))
        warped.paste(col, (x, band_y0 - int(e * 0.55) + dy))
    warped = ImageChops.multiply(warped, band)
    text_col = full_strip(r, (255, 200, 72), AMBER, band_y0, band_y1 + e, 0.5, 0.75)
    paint(warped, text_col)

    # Augen: halb geschlossen, Blick zur Seite – der Prop hat dich gesehen
    eye_y = s(400)
    for ex, rx in ((cx - s(92), s(64)), (cx + s(70), s(58))):
        ry = s(74)
        eye = Image.new('L', (N, N), 0)
        ImageDraw.Draw(eye).ellipse(ellipse_box(ex, eye_y, rx, ry), fill=255)
        white = Image.new('RGB', (N, N), (250, 250, 252))
        # leichte Schattierung unten
        shade = Image.new('L', (N, N), 0)
        ImageDraw.Draw(shade).ellipse(ellipse_box(ex + s(6), eye_y + s(20), rx, ry), fill=255)
        can.paste(Image.new('RGB', (N, N), INK), (0, 0), ImageDraw.Draw(eye) and eye.filter(ImageFilter.MaxFilter(3)))
        can.paste(white, (0, 0), eye)
        can.paste(Image.new('RGB', (N, N), (214, 220, 232)), (0, 0), ImageChops.subtract(eye, shade))
        # Pupille nach links oben
        pupil = Image.new('L', (N, N), 0)
        pr = int(rx * 0.5)
        ImageDraw.Draw(pupil).ellipse(ellipse_box(ex - int(rx * 0.38), eye_y + s(10), pr, int(pr * 1.08)), fill=255)
        pupil = ImageChops.multiply(pupil, eye)
        can.paste(Image.new('RGB', (N, N), INK), (0, 0), pupil)
        glint = Image.new('L', (N, N), 0)
        ImageDraw.Draw(glint).ellipse(ellipse_box(ex - int(rx * 0.52), eye_y - s(4), s(11), s(11)), fill=255)
        can.paste(Image.new('RGB', (N, N), (255, 255, 255)), (0, 0), glint)
        # Oberlid (schräg, verschmitzt) in Dosenfarbe mit dunkler Lidkante
        lid = Image.new('L', (N, N), 0)
        tilt = s(16) if ex < cx else -s(16)
        lid_y = eye_y - s(10)
        ImageDraw.Draw(lid).polygon([(ex - rx - s(10), lid_y - tilt - s(200)), (ex + rx + s(10), lid_y + tilt - s(200)),
                                     (ex + rx + s(10), lid_y + tilt), (ex - rx - s(10), lid_y - tilt)], fill=255)
        lid = ImageChops.multiply(lid, eye)
        can.paste(Image.new('RGB', (N, N), (232, 128, 22)), (0, 0), lid)
        edge = Image.new('L', (N, N), 0)
        ImageDraw.Draw(edge).line([(ex - rx - s(10), lid_y - tilt), (ex + rx + s(10), lid_y + tilt)], fill=255, width=s(12))
        edge = ImageChops.multiply(edge, eye.filter(ImageFilter.MaxFilter(7)))
        can.paste(Image.new('RGB', (N, N), INK), (0, 0), edge)
        rim = Image.new('L', (N, N), 0)
        ImageDraw.Draw(rim).ellipse(ellipse_box(ex, eye_y, rx, ry), outline=255, width=s(8))
        can.paste(Image.new('RGB', (N, N), INK), (0, 0), rim)

    # Grinsen unter den Augen, rechts hochgezogen
    smile = Image.new('L', (N, N), 0)
    ImageDraw.Draw(smile).arc(ellipse_box(cx + s(8), s(496), s(54), s(30)), 12, 158, fill=255, width=s(12))
    can.paste(Image.new('RGB', (N, N), INK), (0, 0), smile)

    # Deckel: Rand, vertiefte Fläche, Lasche
    lid_ry = int(e * 0.86)
    rimm = Image.new('L', (N, N), 0)
    ImageDraw.Draw(rimm).ellipse(ellipse_box(cx, y_lid, rn, lid_ry), fill=255)
    paint(rimm, full_strip(rn, (236, 240, 246), (200, 206, 216), y_lid - lid_ry, y_lid + lid_ry, 0.8, 0.55))
    inner = Image.new('L', (N, N), 0)
    ImageDraw.Draw(inner).ellipse(ellipse_box(cx, y_lid + s(5), rn - s(20), lid_ry - s(14)), fill=255)
    paint(inner, full_strip(rn, (170, 177, 190), (204, 210, 220), y_lid - lid_ry, y_lid + lid_ry, 0.35, 0.6))
    tab = Image.new('L', (N, N), 0)
    dt = ImageDraw.Draw(tab)
    dt.rounded_rectangle([cx - s(46), y_lid - s(26), cx + s(70), y_lid + s(16)], radius=s(20), fill=255)
    dt.rounded_rectangle([cx - s(18), y_lid - s(14), cx + s(44), y_lid + s(4)], radius=s(9), fill=0)
    paint(tab, full_strip(rn, (238, 242, 248), (190, 196, 208), y_lid - lid_ry, y_lid + lid_ry, 0.9, 0.6))
    rivet = Image.new('L', (N, N), 0)
    ImageDraw.Draw(rivet).ellipse(ellipse_box(cx - s(30), y_lid - s(5), s(10), s(7)), fill=255)
    can.paste(Image.new('RGB', (N, N), (140, 146, 158)), (0, 0), rivet)

    # Silhouette
    sil = can.getchannel('A')
    return can, sil


def outline_and_shadow(can, sil, angle, outline=12, shadow=True):
    can = can.rotate(angle, resample=Image.BICUBIC, center=(N // 2, s(520)))
    sil = sil.rotate(angle, resample=Image.BICUBIC, center=(N // 2, s(520)))
    out = Image.new('RGBA', (N, N), (0, 0, 0, 0))
    if shadow:  # Schatten am Boden
        sh = Image.new('L', (N, N), 0)
        ImageDraw.Draw(sh).ellipse(ellipse_box(N // 2 + s(26), s(872), s(250), s(44)), fill=150)
        sh = sh.filter(ImageFilter.GaussianBlur(s(18)))
        out.paste(Image.new('RGBA', (N, N), (0, 0, 0, 255)), (0, 0), sh)
    # dicke dunkle Kontur – für kleine Symbolgrößen noch dicker
    grown = sil.filter(ImageFilter.GaussianBlur(s(outline))).point(lambda v: 255 if v > 14 else 0).filter(ImageFilter.GaussianBlur(SS))
    out.paste(Image.new('RGBA', (N, N), INK + (255,)), (0, 0), grown)
    out.alpha_composite(can)
    return out


def square_crop(img, margin=0.03):
    """Quadratischer Ausschnitt um den sichtbaren Inhalt, damit die Dose die Symbolfläche füllt."""
    box = img.getchannel('A').point(lambda v: 255 if v > 10 else 0).getbbox()
    cx, cy = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
    half = max(box[2] - box[0], box[3] - box[1]) * (0.5 + margin)
    sq = Image.new('RGBA', (int(half * 2), int(half * 2)), (0, 0, 0, 0))
    sq.alpha_composite(img.crop((int(cx - half), int(cy - half), int(cx + half), int(cy + half))))
    return sq


def background(size):
    bg = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    # Kachel mit dunklem Verlauf
    grad = Image.new('RGB', (1, size))
    for y in range(size):
        grad.putpixel((0, y), mix((32, 38, 56), (10, 13, 20), y / (size - 1)))
    grad = grad.resize((size, size))
    mask = Image.new('L', (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=int(size * 0.22), fill=255)
    bg.paste(grad, (0, 0), mask)
    # Strahlenkranz + Glühen in Bernstein
    rays = Image.new('L', (size, size), 0)
    dr = ImageDraw.Draw(rays)
    c = (size / 2, size * 0.5)
    for i in range(18):
        a0 = i * 2 * math.pi / 18
        a1 = a0 + math.pi / 18
        rr = size
        dr.polygon([c, (c[0] + rr * math.cos(a0), c[1] + rr * math.sin(a0)), (c[0] + rr * math.cos(a1), c[1] + rr * math.sin(a1))], fill=34)
    glow = Image.new('L', (size, size), 0)
    ImageDraw.Draw(glow).ellipse(ellipse_box(size // 2, int(size * 0.5), int(size * 0.36), int(size * 0.36)), fill=200)
    glow = glow.filter(ImageFilter.GaussianBlur(size * 0.12))
    rays = ImageChops.multiply(rays, glow.point(lambda v: min(255, v * 2)))
    bg.paste(Image.new('RGB', (size, size), ORANGE), (0, 0), ImageChops.multiply(glow.point(lambda v: v // 2), mask))
    bg.paste(Image.new('RGB', (size, size), AMBER), (0, 0), ImageChops.multiply(rays, mask))
    return bg, mask


def main():
    can, sil = build_can()
    angle = -12
    big = outline_and_shadow(can, sil, angle).resize((1024, 1024), Image.LANCZOS)
    crop = square_crop(outline_and_shadow(can, sil, angle))
    crop_small = square_crop(outline_and_shadow(can, sil, angle, outline=34, shadow=False), 0.01)

    res = os.path.join(ROOT, 'res')
    os.makedirs(res, exist_ok=True)

    # Windows-Symbol: große Stufen mit Schatten, kleine mit dicker Kontur
    sizes = [256, 128, 64, 48, 40, 32, 24, 20, 16]
    frames = []
    for sz in sizes:
        im = (crop if sz > 48 else crop_small).resize((sz, sz), Image.LANCZOS)
        if sz <= 32:
            im = im.filter(ImageFilter.UnsharpMask(radius=0.6, percent=70, threshold=0))
        frames.append(im)
    frames[0].save(os.path.join(res, 'icon.ico'), format='ICO', sizes=[(f.width, f.height) for f in frames], append_images=frames[1:])

    # Quadratische Kachel mit Hintergrund (Discord, Stores)
    bg, mask = background(1024)
    tile = bg.copy()
    tile.alpha_composite(big.resize((900, 900), Image.LANCZOS), (62, 70))
    tile.putalpha(ImageChops.multiply(tile.getchannel('A'), mask))
    tile.save(os.path.join(res, 'icon_1024.png'))

    if len(sys.argv) > 1:
        sheet = Image.new('RGBA', (1024 + 300, 1024), (40, 44, 52, 255))
        sheet.alpha_composite(tile, (0, 0))
        x, y = 1040, 20
        for sz in (256, 64, 48, 32, 24, 16):
            im = (crop if sz > 48 else crop_small).resize((sz, sz), Image.LANCZOS)
            sheet.alpha_composite(im, (x, y))
            y += sz + 24
        light = Image.new('RGBA', (300, 120), (235, 237, 240, 255))
        for i, sz in enumerate((48, 32, 16)):
            light.alpha_composite((crop if sz > 48 else crop_small).resize((sz, sz), Image.LANCZOS), (10 + i * 90, 30))
        sheet.alpha_composite(light, (1024, 880))
        sheet.save(sys.argv[1])
    print('ok:', os.path.join(res, 'icon.ico'), os.path.join(res, 'icon_1024.png'))


if __name__ == '__main__':
    main()
