#!/usr/bin/env python3
"""Read every control position, size, label and tooltip from the approved mockup DOM with Playwright and
write docs/mockup/layout.json. tools/gen_layout.py turns that into src/editor/Layout.h, so the native
editor and the mockup cannot drift apart. Run from the repo root."""
import asyncio, json, os, sys
from playwright.async_api import async_playwright
root = os.getcwd()
src = open('docs/mockup/FetlinePounce.html').read()
ff = ''.join('@font-face{font-family:"%s";src:url("file://%s/assets/fonts/%s");font-weight:%s;font-style:%s}' % (fam, root, f, w, st) for fam, f, w, st in [
    ('Barlow Condensed', 'BarlowCondensed-Medium.ttf', 500, 'normal'), ('Barlow Condensed', 'BarlowCondensed-SemiBold.ttf', 600, 'normal'),
    ('Barlow Condensed', 'BarlowCondensed-Bold.ttf', 700, 'normal'), ('Barlow Condensed', 'BarlowCondensed-ExtraBoldItalic.ttf', 800, 'italic'),
    ('Barlow Condensed', 'BarlowCondensed-ExtraBoldItalic.ttf', 700, 'italic'),
    ('JetBrains Mono', 'JetBrainsMono-Regular.ttf', 400, 'normal'), ('JetBrains Mono', 'JetBrainsMono-Regular.ttf', 600, 'normal')])
# the wrapper drops the Google Fonts link (offline) and loads the bundled fonts instead
src = '\n'.join(l for l in src.split('\n') if 'fonts.googleapis' not in l and 'fonts.gstatic' not in l)
wrapped = '/tmp/fp_wrapped.html'
open(wrapped, 'w').write('<!doctype html><html><head><meta charset="utf-8"><style>' + ff + '</style></head><body>' + src + '</body></html>')
JS = r"""
() => {
  const st = document.querySelector('#stage').getBoundingClientRect();

  const R = e => { const r = e.getBoundingClientRect(); return [Math.round((r.left-st.left)*100)/100, Math.round((r.top-st.top)*100)/100, Math.round(r.width*100)/100, Math.round(r.height*100)/100]; };
  const q = s => document.querySelector(s), qa = s => [...document.querySelectorAll(s)];
  qa('.key').forEach(k => k.classList.remove('on'));   // the pressed look moves a key by 1 px; measure the resting position
  const o = {};
  for (const [k, s] of Object.entries({cheekL:'#cheekL', cheekR:'#cheekR', main:'.main', hdr:'.hdr', body:'.body', top:'#top', ctl:'#ctl', disp:'.disp', rig:'.rig',
      meters:'.meters', foot:'.foot', logo:'.brand svg', n1:'.brand .n1', n2:'.brand .n2', s87:'.brand .s87', pPrev:'#pPrev', pName:'#pName', pNext:'#pNext', pSave:'#pSave',
      bBypass:'#bBypass', bGuide:'#bGuide', bSettings:'#bSettings', bAbout:'#bAbout', vu:'.vu', cvVU:'#cvVU', cvCurve:'#cvCurve', cvHist:'#cvHist', cvTime:'#cvTime',
      cvIn:'#cvIn', cvOut:'#cvOut', pkIn:'#pkIn', pkOut:'#pkOut', winsz:'#winsz', footText:'.foot span:first-child', kHP:'#kHP', mixKnob:'.mixknob .knob', mixVal:'#mixVal'})) o[k] = R(q(s));
  o.tips = {};
  for (const [k, s] of Object.entries({brand:'.brand', pPrev:'#pPrev', pName:'#pName', pNext:'#pNext', pSave:'#pSave', bBypass:'#bBypass', bGuide:'#bGuide', bSettings:'#bSettings', bAbout:'#bAbout',
      vu:'.vu', cvCurve:'#cvCurve', cvHist:'#cvHist', cvTime:'#cvTime', cvIn:'#cvIn', cvOut:'#cvOut', pkIn:'#pkIn', pkOut:'#pkOut', kHP:'#kHP', cheek:'#cheekL', mixKnob:'.mixknob .knob'})) o.tips[k] = q(s).dataset.tip || '';
  o.kcells = qa('#top .kcell').map(c => ({id:c.dataset.k, lab:c.querySelector('.lab').textContent, labR:R(c.querySelector('.lab')), labTip:c.querySelector('.lab').dataset.tip, knob:R(c.querySelector('.knob')),
      knobTip:c.querySelector('.knob').dataset.tip, ends:[...c.querySelectorAll('.ends span')].map(e => ({t:e.textContent, r:R(e)})), val:R(c.querySelector('.kval'))}));
  o.grps = qa('#ctl .grp').map(g => ({cap:g.querySelector('.cap').textContent, capR:R(g.querySelector('.cap')), keys:[...g.querySelectorAll('.key')].map(k => ({t:k.textContent, r:R(k), tip:k.dataset.tip || '', aria:k.getAttribute('aria-label') || ''}))}));
  o.mixCap = {t:q('.mixgrp .cap').textContent, r:R(q('.mixgrp .cap'))};
  o.dcaps = qa('.dcell .cap').map(e => ({t:e.textContent, r:R(e)}));
  o.mcaps = qa('.meters .cap span').map(e => ({t:e.textContent, r:R(e)}));
  o.mcapAll = R(q('.meters .cap'));
  o.ornament = R(q('#top'));
  return o;
}
"""
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={'width': 1400, 'height': 900})
        await pg.goto('file://' + wrapped); await pg.wait_for_timeout(1500)
        d = await pg.evaluate(JS)
        json.dump(d, open('docs/mockup/layout.json', 'w'), indent=1)
        await pg.locator('#stage').screenshot(path='docs/mockup/mockup_reference.png')
        print('ok', len(d['kcells']), 'knob cells,', sum(len(g['keys']) for g in d['grps']), 'keys')
        await b.close()
asyncio.run(main())
