#!/usr/bin/python3
'''Determine ASCII/unicode symbols for midi instruments'''
from __future__ import division, print_function
from collections import defaultdict
import colorsys
import itertools
# Ideas:
#   fix attributes per glyph
#   background color (dark grey, grey, ...)
#   256 colors
MIDIsymbols = (
"PPPPPPhcckmvmxbd" # Ins  0-15
"oooooahoGGGGGGGG" # Ins 16-31
"BBBBBBBBVVVVVHHM" # Ins 32-47
"SSSSOOOcTTTTTTTT" # Ins 48-63
"XXXXTTTFFFFFFFFF" # Ins 64-79
"LLLLLLLLpppppppp" # Ins 80-95
"XXXXXXXXGGGGGTSS" # Ins 96-111
"bbbbMMMcGXXXXXXX" # Ins 112-127
"????????????????" # Prc 0-15
"???????????DDDDD" # Prc 16-31
"DDDDDshMhhhCCCbM" # Prc 32-47
"CBDMMDDDMMDDDDDD" # Prc 48-63
"DDDDDDDDDDDDDDDD" # Prc 64-79
"DDDDDDDD????????" # Prc 80-95
"????????????????" # Prc 96-111
"????????????????" # Prc 112-127
)

# General Midi Instruments
GMI = {
 0: 'Acoustic Grand Piano',
 1: 'Bright Acoustic Piano',
 2: 'Electric Grand Piano',
 3: 'Honky-tonk Piano',
 4: 'Electric Piano 1',
 5: 'Electric Piano 2',
 6: 'Harpsichord',
 7: 'Clavi',
 8: 'Celesta',
 9: 'Glockenspiel',
 10: 'Music Box',
 11: 'Vibraphone',
 12: 'Marimba',
 13: 'Xylophone',
 14: 'Tubular Bells',
 15: 'Dulcimer',
 16: 'Drawbar Organ',
 17: 'Percussive Organ',
 18: 'Rock Organ',
 19: 'Church Organ',
 20: 'Reed Organ',
 21: 'Accordion',
 22: 'Harmonica',
 23: 'Tango Accordion',
 24: 'Acoustic Guitar (nylon)',
 25: 'Acoustic Guitar (steel)',
 26: 'Electric Guitar (jazz)',
 27: 'Electric Guitar (clean)',
 28: 'Electric Guitar (muted)',
 29: 'Overdriven Guitar',
 30: 'Distortion Guitar',
 31: 'Guitar harmonics',
 32: 'Acoustic Bass',
 33: 'Fingered Bass',
 34: 'Picked Bass',
 35: 'Fretless Bass',
 36: 'Slap Bass 1',
 37: 'Slap Bass 2',
 38: 'Synth Bass 1',
 39: 'Synth Bass 2',
 40: 'Violin',
 41: 'Viola',
 42: 'Cello',
 43: 'Contrabass',
 44: 'Tremolo Strings',
 45: 'Pizzicato Strings',
 46: 'Orchestral Harp',
 47: 'Timpani',
 48: 'String Ensemble 1',
 49: 'String Ensemble 2',
 50: 'SynthStrings 1',
 51: 'SynthStrings 2',
 52: 'Choir Aahs',
 53: 'Voice Oohs',
 54: 'Synth Voice',
 55: 'Orchestra Hit',
 56: 'Trumpet',
 57: 'Trombone',
 58: 'Tuba',
 59: 'Muted Trumpet',
 60: 'French Horn',
 61: 'Brass Section',
 62: 'SynthBrass 1',
 63: 'SynthBrass 2',
 64: 'Soprano Sax',
 65: 'Alto Sax',
 66: 'Tenor Sax',
 67: 'Baritone Sax',
 68: 'Oboe',
 69: 'English Horn',
 70: 'Bassoon',
 71: 'Clarinet',
 72: 'Piccolo',
 73: 'Flute',
 74: 'Recorder',
 75: 'Pan Flute',
 76: 'Blown Bottle',
 77: 'Shakuhachi',
 78: 'Whistle',
 79: 'Ocarina',
 80: 'Lead 1 (square)',
 81: 'Lead 2 (sawtooth)',
 82: 'Lead 3 (calliope)',
 83: 'Lead 4 (chiff)',
 84: 'Lead 5 (charang)',
 85: 'Lead 6 (voice)',
 86: 'Lead 7 (fifths)',
 87: 'Lead 8 (bass + lead)',
 88: 'Pad 1 (new age)',
 89: 'Pad 2 (warm)',
 90: 'Pad 3 (polysynth)',
 91: 'Pad 4 (choir)',
 92: 'Pad 5 (bowed)',
 93: 'Pad 6 (metallic)',
 94: 'Pad 7 (halo)',
 95: 'Pad 8 (sweep)',
 96: 'FX 1 (rain)',
 97: 'FX 2 (soundtrack)',
 98: 'FX 3 (crystal)',
 99: 'FX 4 (atmosphere)',
 100: 'FX 5 (brightness)',
 101: 'FX 6 (goblins)',
 102: 'FX 7 (echoes)',
 103: 'FX 8 (sci-fi)',
 104: 'Sitar',
 105: 'Banjo',
 106: 'Shamisen',
 107: 'Koto',
 108: 'Kalimba',
 109: 'Bag pipe',
 110: 'Fiddle',
 111: 'Shanai',
 112: 'Tinkle Bell',
 113: 'Agogo',
 114: 'Steel Drums',
 115: 'Woodblock',
 116: 'Taiko Drum',
 117: 'Melodic Tom',
 118: 'Synth Drum',
 119: 'Reverse Cymbal',
 120: 'Guitar Fret Noise',
 121: 'Breath Noise',
 122: 'Seashore',
 123: 'Bird Tweet',
 124: 'Telephone Ring',
 125: 'Helicopter',
 126: 'Applause',
 127: 'Gunshot'
}

# General Midi Percussion
GMP = {
 # GS extensions
 27: 'High Q',
 28: 'Slap',
 29: 'Scratch Push',
 30: 'Scratch Pull',
 31: 'Sticks',
 32: 'Square Click',
 33: 'Metronome Click',
 34: 'Metronome Bell',
 # GM
 35: 'Acoustic Bass Drum',
 36: 'Bass Drum 1',
 37: 'Side Stick',
 38: 'Acoustic Snare',
 39: 'Hand Clap',
 40: 'Electric Snare',
 41: 'Low Floor Tom',
 42: 'Closed Hi-Hat',
 43: 'High Floor Tom',
 44: 'Pedal Hi-Hat',
 45: 'Low Tom',
 46: 'Open Hi-Hat',
 47: 'Low-Mid Tom',
 48: 'Hi-Mid Tom',
 49: 'Crash Cymbal 1',
 50: 'High Tom',
 51: 'Ride Cymbal 1',
 52: 'Chinese Cymbal',
 53: 'Ride Bell',
 54: 'Tambourine',
 55: 'Splash Cymbal',
 56: 'Cowbell',
 57: 'Crash Cymbal 2',
 58: 'Vibraslap',
 59: 'Ride Cymbal 2',
 60: 'Hi Bongo',
 61: 'Low Bongo',
 62: 'Mute Hi Conga',
 63: 'Open Hi Conga',
 64: 'Low Conga',
 65: 'High Timbale',
 66: 'Low Timbale',
 67: 'High Agogo',
 68: 'Low Agogo',
 69: 'Cabasa',
 70: 'Maracas',
 71: 'Short Whistle',
 72: 'Long Whistle',
 73: 'Short Guiro',
 74: 'Long Guiro',
 75: 'Claves',
 76: 'Hi Wood Block',
 77: 'Low Wood Block',
 78: 'Mute Cuica',
 79: 'Open Cuica',
 80: 'Mute Triangle',
 81: 'Open Triangle',
 # GS extensions
 82: 'Shaker',
 83: 'Jingle Bell',
 84: 'Belltree',
 85: 'Castanets',
 86: 'Mute Surdo',
 87: 'Open Surdo'
}

def compute_intensity(rr,gg,bb):
    return 0.2125 * rr + 0.7154 * gg + 0.0721 * bb

if __name__ == '__main__':
    syms = defaultdict(int)
    for ins,ch in enumerate(MIDIsymbols):
        name = None
        if ins < 128:
            name = GMI.get(ins)
        else:
            name = GMP.get(ins-128)

        syms[ins>=128,ch] += 1

        #print('%3i %s %s' % (ins,ch,name))
    print()
    print('Symbol counts')
    print()
    for key in sorted(syms.keys()):
        count = syms[key]
        print('%i:%s %i' % (int(key[0]), key[1], count))

    class Color(object):
        def __init__(self, colid, rr, gg, bb):
            self.intensity = compute_intensity(rr,gg,bb)
            self.colid = colid
            self.r = rr
            self.g = gg
            self.b = bb
            (hh,ss,vv) = colorsys.rgb_to_hsv(rr,gg,bb)
            if ss == 0: # monochromes into one bin
                hh = vv # 'random' sort order
                ss = 0.51  # lighter colors are more 'saturated'
            self.h = hh
            self.s = ss
            self.v = vv

        def hue_difference(self, o):
            # could be weighted according to sensitivity
            return abs(self.h - o.h)

        def colorize(self, s):
            return "\x1b[0;38;5;%im%s\x1b[0m" % (self.colid, s)

    def colorlist_str(b,ch='X'):
        return ''.join(c.colorize(ch) for c in b)

    # color allocation: make list of colors
    colors = []
    for r in range(0,6):
        for g in range(0,6):
            for b in range(0,6):
                rr = r / 5.0
                gg = g / 5.0
                bb = b / 5.0
                colid = 16+r*36+g*6+b
                colors.append(Color(colid, rr, gg, bb))
    for i in range(0,24):
        rr = gg = bb = (8 + i * 10) / 255.0
        colid = i + 232
        colors.append(Color(colid, rr, gg, bb))

    # sort into bins
    bins = defaultdict(list)
    for c in colors:
        bins[c.v,c.s].append(c)

    # sort bins by hue
    colors_sorted = []
    for key in sorted(bins.keys()):
        bin = bins[key]
        bin.sort(key=lambda c:(c.h,c.s,c.v))
        colors_sorted.extend(bin)
   
    print()
    print("Available colors")
    for c in colors_sorted:
        print('\x1b[0;38;5;%imc\x1b[0m  %.02f %.02f %.02f = %3i int=%.2f h=%.2f s=%.2f v=%.2f' % (c.colid,c.r,c.g,c.b,c.colid,c.intensity,c.h,c.s,c.v))

    # selection filter
    all_colors = [[],[]]
    for c in colors_sorted:
        if c.v > 0.2 and c.s > 0.7: # v threshold
            # keep the bright and nice colors for instruments
            all_colors[0].append(c)
        elif c.v > 0.2:
            # leave the rest for drums, if not too dark
            all_colors[1].append(c)
    
    # sort by hue
    for x in [0,1]:
        all_colors[x].sort(key=lambda c:c.h)

    # bucket per hue
    # this makes sure that succesive colors in the assign order are sorted by hue
    # but differ enough to be distinguishable.
    BUCKETS = 20
    for cols in all_colors:
        idx = 0
        buckets = [[] for x in range(BUCKETS)]
        prev = None
        for c in cols:
            buckets[min(int(c.h * BUCKETS),BUCKETS-1)].append(c)
        print()
        print("Buckets")
        chance = []
        for i,b in enumerate(buckets):
            b.sort(key=lambda c:c.intensity)
            if i%2: # ramp up/down alternatingly to alternate bright and darker colors
                # ideally we want to pair colors that are sufficiently different, either in
                # hue, saturation or value to be distinguishable
                half = len(b)//2
                b[:] = b[half:] + b[0:half]
            print(colorlist_str(b))
            chance.append(len(b))

        maxlen = max(chance)
        chance = [c/maxlen for c in chance]
        out = []
        nums = [0.5 for c in chance]
        while len(out) < len(cols):
            for i,x in enumerate(buckets):
                nums[i] += chance[i]
                if x and nums[i] >= 1:
                    out.append(x.pop(0))
                    nums[i] -= 1.0
        cols[:] = out
    '''
    # shuffle assign order a bit (but in predictable pattern)
    SHUFFLE=20
    for cols in all_colors:
        u = [[] for x in range(SHUFFLE)]
        for i,c in enumerate(cols):
            u[i%SHUFFLE].append(c)
        cols[:] = list(itertools.chain.from_iterable(u)) 
    '''
    # convert color tuples to loose color codes
    print('Available colors')
    print('instruments (%i): ' % len(all_colors[0]))
    print(colorlist_str(all_colors[0], 'X'))
    print('drums (%i): ' % len(all_colors[1]))
    print(colorlist_str(all_colors[1], 'D'))

    # Color allocation
    #available = {}
    #for key in sorted(syms.keys()):
    #    colors = all_colors[key[0]][:]
    #    random.shuffle(colors)
    #    available[key] = colors
    ptr = [0,0]
    per_sym = defaultdict(list)
    SKIP = 1
    for key in sorted(syms.keys()):
        (cat,ch) = key
        count = syms[key]
        for x in range(0, count):
            c = all_colors[cat][ptr[cat]]
            per_sym[key].append(c)
            ptr[cat] = (ptr[cat] + SKIP)%len(all_colors[cat])

    print()
    print('Assigned')
    collision = set()
    syms_out = []
    for ins,ch in enumerate(MIDIsymbols):
        name = None
        if ins < 128:
            name = GMI.get(ins)
        else:
            name = GMP.get(ins-128)

        #color = available[ins>=128,ch].pop()
        color = per_sym[ins>=128,ch].pop()
        if (color.colid,ch) in collision:
            print("Color/symbol collision detected")
        collision.add((color.colid, ch))
        syms_out.append((color.colid, ch))

        print('%3i \x1b[0;38;5;%im%s\x1b[0m %s' % (ins,color.colid,ch,name))

    # print in block format
    s = [] 
    for x in range(0, 256):
        if x == 0:
            s.append('Instruments:\n')
        if x == 128:
            s.append('\nPercussion:\n')
        if (x%16) == 0:
            s.append('%3i ' % (x%128))
        s.append('\x1b[0;38;5;%im%s\x1b[0m' % syms_out[x])
        if (x%16) == 15:
            s.append('\n')
    print(''.join(s))

    # write to disk
    with open('midi_symbols_256.hh', 'w') as f:
        f.write('/* AUTO-GENERATED by instruments.py */\n')
        f.write('static const unsigned char MIDIcolors256[256] = {\n')
        for x in range(0,256):
            (color,ch) = syms_out[x]
            f.write("%3i" % (color))
            if x != 255:
                f.write(',')
            if (x % 16)==15:
                f.write('\n')
        f.write('};\n')

