#ifndef H_CONFIG
#define H_CONFIG

enum OPLEmuType
{
OPLEMU_DBOPL,        // Old DOSBOX OPL3
OPLEMU_DBOPLv2,      // New DOSBOX OPL3
OPLEMU_VintageTone,  // 'That vintage tone' emulator by Robson Cozendey ported to C++ from zdoom
OPLEMU_YMF262        // YMF262 from MAME (via VGMPlay)
};

static const unsigned MaxCards = 100;
static const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
static const unsigned MaxWidth = 120;
static const unsigned MaxHeight = 1 + 23*MaxCards;
static const float SAMPLE_MULT_OUTPUT_FLOAT = 0.33f; // Scaling applied to output samples
static const double ReverbScale = 0.1;

extern unsigned AdlBank;
extern unsigned NumFourOps;
extern unsigned NumCards;
extern bool HighTremoloMode;
extern bool HighVibratoMode;
extern bool AdlPercussionMode;
extern bool QuitWithoutLooping;
extern bool WritePCMfile;
extern bool ScaleModulators;
extern OPLEmuType EmuType;
extern bool FullPan;
extern bool AllowBankSwitch;
extern bool EnableReverb;

#endif

