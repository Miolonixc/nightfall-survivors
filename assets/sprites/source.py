"""Original Nightfall pixel art. Integer coordinates, no filtering, no external art.
All canvases are 48x48; ground pivot (24,40). The game displays pixels at 2x.
"""
from PIL import Image, ImageDraw, ImageFilter

P = {
    'ink': '#080e18', 'rim': '#91cbbb', 'deep': '#162c36', 'shade': '#204e54',
    'cloth': '#338b82', 'light': '#67c9ad', 'glint': '#bdedd2',
    'skin': '#c7b58e', 'skinlight': '#eddbaf', 'gold': '#c19a52', 'goldlight': '#f5d681',
    'bone': '#acb59a', 'bonelight': '#d6d5b2', 'rot': '#63785c', 'rotlight': '#94a776',
    'leather': '#493e42', 'leatherlight': '#79615a', 'metal': '#586376',
    'metallight': '#a0a0b0', 'purple': '#665472', 'purplelight': '#a386a7',
    'wing': '#493b60', 'winglight': '#8d6390', 'red': '#9e435d', 'redlight': '#db7790',
    'stone': '#34434b', 'stonelight': '#536266', 'stoneshade': '#202e38',
    'moss': '#3b5148', 'amber': '#d59b53', 'fire': '#ffd58b',
}

def canvas(): return Image.new('RGBA', (48,48))
def outline(im, hero=False):
    alpha=im.getchannel('A')
    outer=alpha.filter(ImageFilter.MaxFilter(3))
    base=Image.new('RGBA',im.size, P['rim'] if hero else P['ink'])
    base.putalpha(outer)
    base.alpha_composite(im)
    return base

def wizard(frame, team):
    im=canvas(); d=ImageDraw.Draw(im)
    cloth,light,shade=[('#338b82','#67c9ad','#204e54'),('#66508b','#b19bd9','#392e56'),
                       ('#a07638','#e4bd70','#57452d'),('#426e9c','#8ac5e0','#293d61')][team]
    foot=(frame%4 in (1,2)); dy=1 if frame%2 else 0
    def poly(v,c): d.polygon([(x,y+dy) for x,y in v], fill=c)
    def rect(v,c): d.rectangle((v[0],v[1]+dy,v[2],v[3]+dy),fill=c)
    # Boots, flared asymmetrical cloak and a folded hood.
    d.rectangle((19,38-int(foot),22,40-int(foot)),fill=P['leatherlight'])
    d.rectangle((26,38+int(foot),29,40+int(foot)),fill=P['leatherlight'])
    poly([(20,24),(28,24),(29,29),(32,37),(28,39),(23,38),(17,39),(19,30)],shade)
    poly([(20,25),(24,26),(25,37),(20,38),(18,37)],cloth)
    poly([(25,26),(28,25),(29,31),(31,36),(27,37)],cloth)
    rect((20,28,21,35),light); rect((26,28,27,34),light)
    poly([(20,24),(24,27),(28,24),(26,29),(23,30)],P['gold'])
    rect((23,29,24,36),P['goldlight']); rect((19,36,21,37),P['gold'])
    # Hands and wooden staff with a faceted teal crystal.
    rect((16,28,18,30),P['skin']); rect((30,27,32,29),P['skinlight'])
    rect((33,20,34,39),P['leather']); rect((33,21,33,37),P['gold'])
    poly([(34,13),(37,17),(35,21),(32,20),(31,17)],shade)
    poly([(34,14),(35,17),(34,20),(32,17)],light)
    rect((34,15,34,17),P['glint']); rect((32,21,35,22),P['goldlight'])
    # Pointed hood with a shaded face cavity, small visible eyes.
    poly([(23,14),(26,16),(28,18),(29,24),(25,26),(19,24),(18,21),(20,17)],shade)
    poly([(23,14),(24,17),(21,20),(20,23),(18,22),(20,17)],light)
    poly([(24,16),(27,18),(29,23),(26,24),(25,20)],cloth)
    rect((21,20,25,23),P['ink']); rect((22,21,22,21),P['glint']); rect((25,21,25,21),P['glint'])
    rect((22,24,25,24),P['skin'])
    return outline(im,True)

def zombie(frame):
    im=canvas();d=ImageDraw.Draw(im); stride=1 if frame%4 in(1,2) else -1;dy=frame%2
    d.rectangle((19,36,21,39+stride),fill=P['leather']); d.rectangle((26,36,28,39-stride),fill=P['leather'])
    d.polygon([(19,25),(28,25),(30,35),(27,38),(23,35),(19,37),(17,34)],fill=P['rot'])
    d.polygon([(19,25),(23,27),(22,34),(19,36),(18,33)],fill=P['rotlight'])
    d.rectangle((24,28,27,34),fill=P['leather']); d.rectangle((24,28,25,30),fill=P['bone'])
    d.line((20,29,27,34),fill=P['leatherlight'],width=1)
    # Outstretched ragged arms, exposed hands, non-square head.
    d.polygon([(18,26),(16,27+dy),(14,31+dy),(16,33+dy),(19,30)],fill=P['rot'])
    d.rectangle((14,31+dy,16,33+dy),fill=P['bone'])
    d.polygon([(28,26),(31,27-dy),(33,30-dy),(31,32-dy),(28,29)],fill=P['rot'])
    d.rectangle((31,30-dy,33,32-dy),fill=P['bone'])
    d.polygon([(21,17+dy),(26,17+dy),(29,20+dy),(28,25+dy),(24,27+dy),(19,24+dy),(19,20+dy)],fill=P['bone'])
    d.line((21,18+dy,26,18+dy),fill=P['bonelight'],width=1)
    d.rectangle((19,21+dy,21,22+dy),fill=P['ink']); d.rectangle((25,21+dy,27,22+dy),fill=P['ink'])
    d.point((20,21+dy),fill=P['goldlight']);d.point((26,21+dy),fill=P['goldlight'])
    d.rectangle((23,24+dy,26,24+dy),fill=P['ink']);d.point((24,24+dy),fill=P['bone'])
    return outline(im)

def bat(frame):
    im=canvas();d=ImageDraw.Draw(im); wing=[-4,-1,3,-1][frame];dy=frame%2
    for side in(-1,1):
        def points(v):return [(24+side*x,y) for x,y in v]
        d.polygon(points([(2,26),(7,23),(17,22+wing),(14,29),(11,27),(8,32),(6,29),(3,33)]),fill=P['wing'])
        d.line(points([(3,27),(8,25),(16,23+wing)]),fill=P['purplelight'],width=1)
        d.line(points([(7,25),(8,30)]),fill=P['winglight'],width=1)
        d.line(points([(10,25),(13,28)]),fill=P['winglight'],width=1)
    d.polygon([(20,25+dy),(21,19+dy),(24,23+dy),(27,19+dy),(28,26+dy),(27,32+dy),(24,35+dy),(21,32+dy)],fill=P['purple'])
    d.rectangle((22,26+dy,26,29+dy),fill=P['ink'])
    d.point((22,26+dy),fill=P['redlight']);d.point((26,26+dy),fill=P['redlight'])
    d.line((23,30+dy,23,31+dy),fill=P['bonelight']);d.point((25,30+dy),fill=P['bonelight'])
    return outline(im)

def guard(frame, boss=False):
    im=canvas();d=ImageDraw.Draw(im);dy=frame%2; stride=1 if frame in(1,2)else-1
    if boss:
        # The Warden's ragged crimson mantle is visible around the steel armour.
        d.polygon([(17,17),(31,17),(35,29),(37,39),(31,37),(28,41),(23,38),(18,41),(12,39),(15,28)],fill=P['red'])
        d.line((16,27,13,38),fill=P['redlight'],width=1)
    d.rectangle((18,35,22,39+stride),fill=P['metal']);d.rectangle((26,35,30,39-stride),fill=P['metal'])
    d.rectangle((18,39+stride,23,40+stride),fill=P['leather']);d.rectangle((26,39-stride,31,40-stride),fill=P['leather'])
    d.polygon([(17,24),(31,24),(33,33),(29,37),(24,35),(19,37),(15,33)],fill=P['purple'])
    d.polygon([(19,25),(24,27),(29,25),(28,32),(24,34),(20,32)],fill=P['metal'])
    d.line((20,26,24,28,28,26),fill=P['metallight'],width=1)
    d.rectangle((24,29,24,32),fill=P['gold'] if boss else P['purplelight'])
    # Chamfered shoulder plates and a heavy axe create a distinct broad silhouette.
    for x in(15,29):
        d.polygon([(x,23+dy),(x+3,21+dy),(x+6,24+dy),(x+5,28+dy),(x,28+dy)],fill=P['metal'])
        d.line((x+1,23+dy,x+3,22+dy,x+5,24+dy),fill=P['metallight'],width=1)
    d.rectangle((14,29+dy,16,34+dy),fill=P['leatherlight']);d.rectangle((32,29+dy,34,34+dy),fill=P['leatherlight'])
    d.line((35,23,35,40),fill=P['gold'],width=1)
    d.polygon([(35,23),(39,21),(40,25),(39,29),(36,28)],fill=P['metal'])
    d.line((39,22,40,25,39,28),fill=P['metallight'],width=1)
    d.polygon([(20,15+dy),(28,15+dy),(30,18+dy),(29,24+dy),(24,26+dy),(19,24+dy),(18,18+dy)],fill=P['metal'])
    d.line((21,16+dy,27,16+dy),fill=P['metallight'],width=1)
    d.rectangle((20,20+dy,28,21+dy),fill=P['ink'])
    d.rectangle((21,20+dy,22,20+dy),fill=P['redlight']);d.rectangle((26,20+dy,27,20+dy),fill=P['redlight'])
    d.line((24,22+dy,24,24+dy),fill=P['metallight'],width=1)
    if boss:
        d.polygon([(18,15+dy),(17,10+dy),(21,13+dy),(24,8+dy),(27,13+dy),(31,10+dy),(30,16+dy)],fill=P['gold'])
        d.line((19,15+dy,29,15+dy),fill=P['goldlight'],width=1)
        d.point((24,13+dy),fill=P['redlight'])
    else:
        d.line((19,18+dy,16,15+dy,16,13+dy),fill=P['bone'],width=2)
        d.line((29,18+dy,32,15+dy,32,13+dy),fill=P['bone'],width=2)
    return outline(im)

def prop(kind, frame=0):
    im=canvas();d=ImageDraw.Draw(im)
    if kind in('grave','cross','obelisk'):
        d.ellipse((13,36,35,42),fill=P['stoneshade'])
        d.rectangle((17,36,31,39),fill=P['stone'])
        if kind=='grave':
            d.polygon([(18,36),(18,24),(20,21),(27,21),(30,24),(30,36)],fill=P['stone'])
            d.line((19,34,19,25,21,22,27,22),fill=P['stonelight'])
            d.line((24,25,24,31),fill=P['stoneshade'],width=1);d.line((22,27,26,27),fill=P['stoneshade'])
            d.line((26,33,28,31,27,28),fill=P['stoneshade'])
        elif kind=='cross':
            d.rectangle((22,16,26,37),fill=P['stone']);d.rectangle((17,21,31,25),fill=P['stone'])
            d.line((23,17,23,22,18,22),fill=P['stonelight']);d.line((18,22,18,24),fill=P['stonelight'])
        else:
            d.polygon([(19,37),(20,19),(24,14),(28,19),(29,37)],fill=P['stone'])
            d.polygon([(24,16),(27,20),(28,35),(24,34)],fill=P['stoneshade'])
            d.line((21,20,24,16),fill=P['stonelight'])
            d.line((22,25,24,23,25,26,23,28),fill=P['moss'])
        d.line((18,37,23,37),fill=P['moss'],width=2)
    elif kind=='brazier':
        d.ellipse((15,37,33,41),fill=P['stoneshade'])
        d.line((20,31,18,39),fill=P['metal'],width=2);d.line((28,31,30,39),fill=P['metal'],width=2)
        d.polygon([(17,28),(31,28),(28,33),(20,33)],fill=P['metal'])
        d.line((18,28,30,28),fill=P['gold'])
        sway=[-1,0,1,0][frame]
        d.polygon([(19,27),(20,22),(22,24),(24+sway,17),(26,23),(28,21),(29,27)],fill=P['amber'])
        d.polygon([(22,27),(24,22),(26,25),(26,27)],fill=P['fire'])
    elif kind=='bones':
        d.line((17,37,29,32),fill=P['stone'],width=2);d.line((19,32,27,38),fill=P['stone'],width=2)
        d.polygon([(28,30),(32,30),(34,32),(33,35),(29,35),(27,33)],fill=P['stonelight'])
        d.point((29,32),fill=P['ink']);d.point((32,32),fill=P['ink'])
    elif kind=='grass':
        for x,y in ((16,38),(22,39),(28,37),(31,40)):
            d.line((x,y,x-2,y-4),fill=P['moss']);d.line((x,y,x+1,y-6),fill=P['stone'])
    return im

def icon(kind):
    im=canvas();d=ImageDraw.Draw(im)
    if kind=='Power':
        d.polygon([(19,37),(24,24),(26,23),(29,27),(25,33),(22,38)],fill=P['metallight'])
        d.line((24,26,21,35),fill=P['glint'],width=1)
        d.line((18,34,25,38),fill=P['gold'],width=2)
        d.line((20,37,18,41),fill=P['leatherlight'],width=2)
    elif kind=='Haste':
        d.rectangle((18,24,29,25),fill=P['gold']);d.rectangle((18,40,29,41),fill=P['gold'])
        d.polygon([(20,26),(28,26),(27,30),(24,33),(27,36),(28,39),(20,39),(21,36),(23,33),(21,30)],fill=P['metal'])
        d.polygon([(21,27),(27,27),(26,30),(24,32),(22,30)],fill=P['goldlight'])
        d.polygon([(24,35),(27,39),(21,39)],fill=P['goldlight'])
    elif kind=='Multishot':
        for x,y in((18,31),(24,26),(30,31)):
            d.polygon([(x,y-4),(x+2,y),(x,y+4),(x-2,y)],fill=P['light'])
            d.point((x,y-1),fill=P['glint']);d.line((x,y+6,x,y+9),fill=P['shade'])
    elif kind=='Frost':
        for dx,dy in((0,8),(7,4),(7,-4)):
            d.line((24-dx,33-dy,24+dx,33+dy),fill=P['glint'],width=1)
        d.polygon([(24,26),(27,33),(24,40),(21,33)],fill=P['light'])
        d.line((24,29,24,36),fill=P['glint'])
    elif kind=='Boots':
        for x,y in((18,25),(25,29)):
            d.polygon([(x,y),(x+5,y),(x+4,y+8),(x+7,y+10),(x+7,y+12),(x,y+12)],fill=P['leatherlight'])
            d.line((x,y,x+5,y),fill=P['goldlight'],width=2)
            d.line((x,y+12,x+7,y+12),fill=P['gold'],width=1)
    elif kind=='Vitality':
        d.polygon([(24,28),(21,25),(18,26),(16,29),(17,33),(24,40),(31,33),(32,29),(30,26),(27,25)],fill=P['red'])
        d.line((19,27,21,27,23,29),fill=P['redlight'],width=2)
        d.rectangle((23,30,25,36),fill=P['goldlight']);d.rectangle((21,32,27,34),fill=P['goldlight'])
    return outline(im)

def build():
    sprites=[]
    for team in range(4):
        for f in range(4):sprites.append((f'Wizard{team}_{f}',wizard(f,team)))
    for name, fn in [('Ghoul',zombie),('Bat',bat),('Guard',guard),('Warden',lambda f:guard(f,True))]:
        for f in range(4):sprites.append((f'{name}{f}',fn(f)))
    for k in ('grave','cross','obelisk','bones','grass'):
        sprites.append((k.title(),prop(k)))
    for f in range(4):sprites.append((f'Brazier{f}',prop('brazier',f)))
    for k in ('Power','Haste','Multishot','Frost','Boots','Vitality'):
        sprites.append(('Icon'+k,icon(k)))
    shadow=canvas();d=ImageDraw.Draw(shadow)
    d.ellipse((15,37,33,42),fill=(4,9,16,115))
    sprites.append(('Shadow',shadow))
    for name,rgb in [('EmberGlow',(172,107,49)),('ArcaneGlow',(62,179,163))]:
        glow=canvas();d=ImageDraw.Draw(glow)
        for rad in range(20,0,-2):
            d.ellipse((24-rad,24-rad,24+rad,24+rad),fill=(*rgb, int((21-rad)*1.8)))
        sprites.append((name,glow))
    return sprites
