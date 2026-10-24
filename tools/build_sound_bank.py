"""Prepare the user-supplied recordings as one game-specific embedded sound bank.

Requires Python + numpy. Original recordings are never copied into the repository.
Usage: python tools/build_sound_bank.py C:/Users/name/Downloads
The source sounds remain subject to their original licenses; see res/audio/CREDITS.txt.
"""
from pathlib import Path
from zipfile import ZipFile
import hashlib
import json
import struct
import sys
import numpy as np

RATE = 44100
ROOT = Path(__file__).resolve().parents[1]


def decode_wav(blob):
    if blob[:4] != b"RIFF" or blob[8:12] != b"WAVE":
        raise ValueError("Expected RIFF/WAVE")
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(blob):
        tag, size = struct.unpack_from("<4sI", blob, pos)
        if pos + 8 + size > len(blob):
            raise ValueError("Truncated WAV chunk")
        chunk = blob[pos + 8:pos + 8 + size]
        if tag == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", chunk)
        elif tag == b"data":
            data = chunk
        pos += 8 + size + size % 2
    if fmt is None or data is None:
        raise ValueError("Missing WAV format/data")
    encoding, channels, rate, _, align, bits = fmt
    if encoding != 1 or bits not in (16, 24) or channels not in (1, 2):
        raise ValueError(f"Unsupported input format: {fmt}")
    if len(data) % align:
        raise ValueError("Partial PCM frame")
    if bits == 24:
        raw = np.frombuffer(data, np.uint8).reshape(-1, 3).astype(np.int32)
        pcm = raw[:, 0] | raw[:, 1] << 8 | raw[:, 2] << 16
        pcm = (pcm ^ 0x800000) - 0x800000
        pcm = pcm.astype(np.float64) / 8388608
    else:
        pcm = np.frombuffer(data, "<i2").astype(np.float64) / 32768
    return pcm.reshape(-1, channels).mean(axis=1), rate


def prepare(blob, duration, peak=.8, start=None):
    x, rate = decode_wav(blob)
    if start is None:
        onset = np.flatnonzero(np.abs(x) > np.max(np.abs(x)) * .075)
        start = max(0, (int(onset[0]) / rate if len(onset) else 0) - .003)
    x = x[int(start * rate):int((start + duration) * rate)]
    if len(x) < 2:
        raise ValueError("Empty sound after trimming")
    x -= np.mean(x)
    # Anti-alias before the 96 kHz -> 44.1 kHz conversion; keep transients intact.
    cutoff = min(.45, RATE * .45 / rate)
    t = np.arange(-80, 81, dtype=np.float64)
    kernel = 2 * cutoff * np.sinc(2 * cutoff * t) * np.kaiser(len(t), 8)
    kernel /= kernel.sum()
    x = np.convolve(x, kernel, "same")
    count = int(round(len(x) * RATE / rate))
    x = np.interp(np.arange(count) * rate / RATE, np.arange(len(x)), x)
    x *= peak / max(float(np.max(np.abs(x))), 1e-9)
    fade_in, fade_out = min(66, len(x)), min(int(.055 * RATE), len(x) // 3)
    x[:fade_in] *= np.linspace(0, 1, fade_in)
    x[-fade_out:] *= np.linspace(1, 0, fade_out)
    return x


def build(downloads):
    dest = ROOT / "res/audio"
    dest.mkdir(parents=True, exist_ok=True)
    sounds, sources = {}, {}
    def add(name, filename, seconds, peak=.8, archive=None, start=None):
        blob = archive.read(filename) if archive else (downloads / filename).read_bytes()
        sounds[name] = prepare(blob, seconds, peak, start)
        sources[name] = {"source": filename, "source_sha256": hashlib.sha256(blob).hexdigest(),
                         "duration": round(len(sounds[name]) / RATE, 4)}

    add("smg_shot", "gma_gun_submachine_auto_shot_00_last_with_tail_01_581.wav", .72, .86)
    add("smg_burst", "gma_gun_submachine_auto_shot_00_automatic_preview_01_580.wav", .065, .65)
    add("arcade_punch", "zapsplat_multimedia_videogame_machine_gun_synthetic_designed_105191.wav", .09, .55)
    add("empty", "skyclad_sound_TOYMisc_Gun+Dry+fire+_SCS_CT_821.wav", .24, .65)
    for i, num in enumerate((572, 573, 574)):
        add(f"load_{i}", f"gma_gun_submachine_auto_magazine_load_0{i+2}_{num}.wav", .36, .7)
    for i, num in enumerate((577, 578, 579)):
        add(f"unload_{i}", f"gma_gun_submachine_auto_magazine_unload_0{i+1}_{num}.wav", .46, .65)
    for i, num in enumerate((575, 576)):
        add(f"switch_{i}", f"gma_gun_submachine_auto_magazine_safety_switch_0{i+1}_{num}.wav", .11, .65)
    with ZipFile(downloads / "zapsplat_pack_bullet_impacts_wav.zip") as archive:
        categories = {
            "metal": ("bullet_hit_metal_", .6),
            "wood": ("gun_bullet_whizz_hit_wood_", .38),
            "glass": ("gun_bullet_hit_glass_smash_", .65),
            "wall": ("bullet_hit_ground_dirt_", .3),
        }
        for material, (match, duration) in categories.items():
            files = sorted(n for n in archive.namelist() if not n.startswith("__MACOSX/") and match in n and n.endswith(".wav"))[:3]
            if not files:
                raise ValueError(f"Missing {material} recordings")
            for i, filename in enumerate(files):
                # For whizz + impact recordings, start on the impact's strongest transient.
                blob = archive.read(filename)
                x, rate = decode_wav(blob)
                start = max(0, int(np.argmax(np.abs(x))) / rate - .006) if material == "wood" else None
                add(f"{material}_{i}", filename, duration, .68, archive, start)
        (dest / "ZapSplat-EULA.pdf").write_bytes(archive.read("zapsplat_pack_bullet_impacts_wav/ZapSplat-EULA-Standard-License.pdf"))
    # Custom, bounded PCM bank embedded in the application (not loose downloadable WAVs).
    bank = bytearray(struct.pack("<4sII", b"KSB1", RATE, len(sounds)))
    for name, samples in sounds.items():
        encoded = name.encode("ascii")
        pcm = np.rint(samples * 32767).astype("<i2").tobytes()
        bank += struct.pack("<II", len(encoded), len(samples)) + encoded + pcm
    (dest / "game-sfx.bank").write_bytes(bank)
    comedy = dest / "comedy-sources.json"
    if comedy.exists():
        sources["comedy_recordings"] = json.loads(comedy.read_text(encoding="utf-8"))
    (dest / "sources.json").write_text(json.dumps(sources, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(sounds)} game sounds, {len(bank):,} bytes. Originals remain in Downloads.")


if __name__ == "__main__":
    build(Path(sys.argv[1]))
