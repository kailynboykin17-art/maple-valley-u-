#!/usr/bin/env python3
"""Convert PNGs to raw 3DS RGBA8 tiled textures (.tex) — no tex3ds needed.
Usage: python3 tools/png2tex.py   (converts art/portraits/*.png and art/logo.png into romfs/gfx)"""
import glob, os
from PIL import Image
def morton(x,y):
    r=0
    for i in range(3): r|=((x>>i)&1)<<(2*i)|((y>>i)&1)<<(2*i+1)
    return r
def conv(src,dst,W,H):
    im=Image.open(src).convert('RGBA'); w,h=im.size; px=im.load()
    out=bytearray(W*H*4)
    for y in range(h):
        for x in range(w):
            r,g,b,a=px[x,y]
            tile=(y//8)*(W//8)+(x//8); i=(tile*64+morton(x%8,y%8))*4
            out[i:i+4]=bytes((a,b,g,r))
    open(dst,'wb').write(out)
os.makedirs('romfs/gfx/char',exist_ok=True)
for f in glob.glob('art/portraits/*.png'):
    conv(f,'romfs/gfx/char/'+os.path.basename(f)[:-4]+'.tex',256,256)
conv('art/logo.png','romfs/gfx/logo.tex',64,64)
conv('art/title.png','romfs/gfx/title.tex',512,256)
