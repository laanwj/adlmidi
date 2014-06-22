#!/usr/bin/python3
# Determine ASCII/unicode symbols for midi instruments
MIDIsymbols_orig = (
"PPPPPPhcckmvmxbd" # Ins  0-15
"oooooahoGGGGGGGG" # Ins 16-31
"BBBBBBBVVVVVHHMS" # Ins 32-47
"SSSOOOcTTTTTTTTX" # Ins 48-63
"XXXTTTFFFFFFFFFL" # Ins 64-79
"LLLLLLLppppppppX" # Ins 80-95
"XXXXXXXGGGGGTSSb" # Ins 96-111
"bbbMMMcGXXXXXXXD" # Ins 112-127
"????????????????" # Prc 0-15
"????????????????" # Prc 16-31
"???DDshMhhhCCCbM" # Prc 32-47
"CBDMMDDDMMDDDDDD" # Prc 48-63
"DDDDDDDDDDDDDD??" # Prc 64-79
"????????????????" # Prc 80-95
"????????????????" # Prc 96-111
"????????????????" # Prc 112-127
)
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
"????????????????" # Prc 16-31
"???DDshMhhhCCCbM" # Prc 32-47
"CBDMMDDDMMDDDDDD" # Prc 48-63
"DDDDDDDDDDDDDD??" # Prc 64-79
"????????????????" # Prc 80-95
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
 81: 'Open Triangle'
}

if __name__ == '__main__':
    for ins,ch in enumerate(MIDIsymbols):
        name = None
        if ins < 128:
            name = GMI.get(ins)
        else:
            name = GMP.get(ins-128)

        print('%3i %s %s' % (ins,ch,name))
