"""Prepare the CC0 comedy/instrument recordings, independently of the licensed gun bank.

Usage: python tools/build_comedy_bank.py build/comedy-sources
Requires numpy, scipy and soundfile (asset preparation only; no game/build dependency).
Input provenance and SHA-256 hashes are recorded alongside the generated PCM bank.
"""
from pathlib import Path
import sys, json, hashlib, struct
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfilt, resample_poly
from math import gcd

RATE = 44100

def build(root):
    dest = Path(__file__).resolve().parents[1] / 'res/audio'
    sounds, sources = {}, {}
    def add(key, relative, start, duration, author, page):
        path = root / relative
        data, rate = sf.read(path, always_2d=True)
        x = data.mean(axis=1)[round(start*rate):round((start+duration)*rate)]
        x -= x.mean()
        # Remove room rumble and brittle top end before pitch-shifted playback.
        x = sosfilt(butter(2, 60, 'highpass', fs=rate, output='sos'), x)
        x = sosfilt(butter(3, 7000, 'lowpass', fs=rate, output='sos'), x)
        if rate != RATE:
            g = gcd(rate, RATE)
            x = resample_poly(x, RATE//g, rate//g)
        # Keep the attack; remove only the leading recording silence.
        envelope = np.convolve(x*x, np.ones(441)/441, 'same')
        active = np.flatnonzero(envelope > envelope.max() * .001)
        if not len(active): raise ValueError('Silent source: '+key)
        x = x[max(0, int(active[0])-132):]
        fade_in = min(88,len(x)); fade_out = min(2205,len(x))
        x[:fade_in] *= np.linspace(0,1,fade_in)
        x[-fade_out:] *= np.linspace(1,0,fade_out)
        x *= .74 / max(np.abs(x).max(),1e-8)
        assert 0 < len(x) <= RATE*5 and np.isfinite(x).all()
        sounds[key] = np.rint(x*32767).astype('<i2')
        sources[key] = dict(source=relative, source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                            author=author, page=page, license='CC0-1.0',
                            license_url='https://creativecommons.org/publicdomain/zero/1.0/',
                            start=start, source_duration=duration, frames=len(x))
    oga = 'https://opengameart.org/content/'
    for i, take in enumerate([1,2,3,5]):
        add('toy_'+str(i),f'squeak-toy-sounds/squeak toy/squeak toy - squeak{take}.wav',0,1,'bart',oga+'squeak-toy-sounds')
    for i,name in enumerate(['moo-notification','moo-death']):
        add('cow_'+str(i),f'sci-fi-aliens-and-cows-pack/qubodup-sci-fi-cow-alien-soundpack/{name}.wav',0,2,'qubodup',oga+'sci-fi-aliens-and-cows-pack')
    add('cat','kitten-mew/kitten_mew-1.wav',0,1.137,'AntumDeluge',oga+'kitten-mew')
    for i,start in enumerate([.15,2.53]):
        add('laugh_'+str(i),'group-giggling/group_giggling.ogg',start,1.72,'Nocturnal_Vanguard / AuraVoice',oga+'group-giggling')
    add('laugh_2','tiny-creatures-sounds/laugh.ogg',0,1.73,'fvcalderan',oga+'tiny-creatures-sounds')
    for i,(start,duration) in enumerate([(.14,1.1),(2.22,.75),(4.53,1.12)]):
        add('fart_'+str(i),'gastric-distress/gastricdistress_bylfa_0.wav',start,duration,'LFA',oga+'gastric-distress')
    for i,take in enumerate([1,2,3]):
        add('spring_'+str(i),f'spring-sounds/door springs/door spring - Region #{take}.wav',0,1.25,'bart',oga+'spring-sounds')
    add('bell','metal-polelow-bell-hit/Hitting_The_Metal_BellPole.ogg',0,2.31,'Hydroque',oga+'metal-polelow-bell-hit')
    instruments=json.loads((root/'vsco/manifest.json').read_text(encoding='utf-8'))
    for name,meta in instruments.items():
        add(name,'vsco/'+name+'.wav',0,3.3 if name in ['flute','trombone','trumpet','mute'] else 1.8,
            'Versilian Studios / Sam Gossner & Simon Dalzell; sample cutting Elan Hickler',
            'https://github.com/sgossner/VSCO-2-CE')
        sources[name].update(meta)
    bank=bytearray(struct.pack('<4sII',b'KSB1',RATE,len(sounds)))
    for name,pcm in sounds.items():
        key=name.encode('ascii')
        bank += struct.pack('<II',len(key),len(pcm))+key+pcm.tobytes()
    (dest/'comedy-sfx.bank').write_bytes(bank)
    (dest/'comedy-sources.json').write_text(json.dumps(sources,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    # Existing folder packages already carry this source manifest and credits.
    combined=json.loads((dest/'sources.json').read_text(encoding='utf-8'))
    combined['comedy_recordings']=sources
    (dest/'sources.json').write_text(json.dumps(combined,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    print(f'Prepared {len(sounds)} CC0 comedy sources: {len(bank):,} bytes')

if __name__ == '__main__': build(Path(sys.argv[1]))
