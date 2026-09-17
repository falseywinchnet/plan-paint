#!/usr/bin/env python3
"""Render the original Paint palette emblem from simple inspectable geometry."""
from pathlib import Path
from PIL import Image, ImageDraw
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
image = Image.new('RGBA', (1024, 1024))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((50, 58, 974, 982), radius=200, fill='#A9BFD7')
draw.rounded_rectangle((50, 42, 974, 966), radius=200, fill='#DCE8F5', outline='#7C99BA', width=8)
draw.rounded_rectangle((65, 57, 959, 951), radius=187, outline='#F9FCFF', width=7)
draw.ellipse((160, 223, 871, 819), fill='#E9C99C', outline='#9C714A', width=15)
draw.ellipse((202, 252, 830, 785), outline='#F8DFC0', width=10)
for box, color in [((273,294,404,425),'#4F81BD'), ((445,250,569,374),'#C0504D'), ((622,314,752,444),'#9BBB59'), ((679,489,793,603),'#8064A2'), ((518,623,639,744),'#F79646')]:
    draw.ellipse(box, fill=color, outline='#735D4A', width=5)
    x0,y0,x1,y1=box
    draw.arc((x0+9,y0+9,x1-9,y1-9),200,285,fill='#FFFFFF',width=6)
draw.ellipse((240,529,395,649),fill='#DCE8F5',outline='#9C714A',width=13)
draw.polygon([(683,137),(774,197),(443,674),(350,610)],fill='#6F472F')
draw.polygon([(690,151),(737,182),(410,647),(370,618)],fill='#C7884E')
draw.polygon([(438,488),(527,550),(455,653),(363,589)],fill='#85A5BE',outline='#4C687F',width=7)
draw.line([(433,509),(501,556)],fill='#EAF5FF',width=15)
draw.polygon([(363,586),(457,653),(340,796),(221,775),(277,684)],fill='#2F557C',outline='#244363',width=8)
for shift in range(0,72,14):
    draw.line([(366+shift,615+shift*.65),(290+shift,755+shift*.2)],fill='#6D94B8',width=5)
assets=ROOT/'assets'
image.save(assets/'app-icon.png')
image.save(assets/'app-icon.ico',sizes=[(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)])
if sys.platform=='darwin':
    iconset=ROOT/'dist/Rainstar.iconset'
    iconset.mkdir(parents=True,exist_ok=True)
    for side in (16,32,128,256,512):
        image.resize((side,side),Image.Resampling.LANCZOS).save(iconset/f'icon_{side}x{side}.png')
        image.resize((side*2,side*2),Image.Resampling.LANCZOS).save(iconset/f'icon_{side}x{side}@2x.png')
    subprocess.run(['iconutil','-c','icns',str(iconset),'-o',str(assets/'app-icon.icns')],check=True)
