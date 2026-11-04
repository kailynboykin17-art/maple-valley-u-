# .mvs script format
```
# comment
*label                     jump target
@bg track_stadium_day      background
@show London smirk left    sprite + expression + slot
@music training_montage
London: Dialogue line.     speaker: text
Narration with no colon.
> Choice text -> label     consecutive lines = one choice menu
@add Diggy_Affection 2     change a variable
@if Diggy_Trust 5 label    jump if variable >= value
@jump label

```

## Additions
```
@title EPISODE 01 • FIRST IMPRESSIONS   header text
@show london          portrait (file in romfs/gfx/char/<name>.tex)
@hide
> [Sweet] text -> label   tone tag: Sweet / Bitchy / Good / Bad / Bold (adds Tone_<tag> +1)
@ifnot Var 3 label    jump if Var < 3
```
Portraits: put a PNG in art/portraits/ then run `python3 tools/png2tex.py`.
Saves: sdmc:/3ds/maple-valley-u.sav (autosave at every choice). B = save & title menu.
