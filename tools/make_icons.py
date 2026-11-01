#!/usr/bin/env python3
"""Render the cat head logo on a dark rounded square and write assets/icon/* (PNG sizes, .ico, .icns).
Needs Playwright (Chromium) and Pillow. Run from the repo root."""
import asyncio, os
from playwright.async_api import async_playwright
from PIL import Image
SVG = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="640" height="640"><path d="M8 6 L24 22 H40 L56 6 L60 36 Q60 58 32 60 Q4 58 4 36 Z" fill="#FF7BB1"/><path d="M12 14 L21 23 H14 Z M52 14 L43 23 H50 Z" fill="#FBBDD0"/><ellipse cx="22" cy="36" rx="4" ry="5" fill="#16171a"/><ellipse cx="42" cy="36" rx="4" ry="5" fill="#16171a"/><path d="M29 44 H35 L32 48 Z" fill="#16171a"/><path d="M32 48 Q28 53 24 50 M32 48 Q36 53 40 50" stroke="#16171a" stroke-width="2" fill="none"/><path d="M2 40 H14 M2 46 H14 M62 40 H50 M62 46 H50" stroke="#FBBDD0" stroke-width="1.6"/></svg>'
HTML = '<body style="margin:0;background:transparent"><div style="width:1024px;height:1024px;border-radius:200px;background:linear-gradient(#1e2024,#0b0b0d);display:flex;align-items:center;justify-content:center;box-shadow:inset 0 0 0 8px #3d2210">' + SVG.replace('width="640" height="640"', 'width="720" height="720"') + '</div></body>'
async def main():
    os.makedirs('assets/icon', exist_ok=True)
    async with async_playwright() as p:
        b = await p.chromium.launch(); pg = await b.new_page(viewport={'width': 1024, 'height': 1024})
        await pg.set_content(HTML); await pg.screenshot(path='assets/icon/icon-1024.png', omit_background=True); await b.close()
    im = Image.open('assets/icon/icon-1024.png').convert('RGBA')
    for s in (512, 256, 128, 64, 32):
        im.resize((s, s), Image.LANCZOS).save('assets/icon/icon-%d.png' % s)
    im.save('assets/icon/icon.ico', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    im.save('assets/icon/icon.icns')
asyncio.run(main())
