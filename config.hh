#ifndef H_CONFIG
#define H_CONFIG

static const unsigned long PCM_RATE = 49716;
static const unsigned MaxCards = 100;
static const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
static const unsigned MaxWidth = 160;
static const unsigned MaxHeight = 1 + 23*MaxCards;
static const unsigned SAMPLE_MULT_FACTOR = 10240; // Conversion from float to 16-bit signed sample

extern unsigned AdlBank;
extern unsigned NumFourOps;
extern unsigned NumCards;
extern bool HighTremoloMode;
extern bool HighVibratoMode;
extern bool AdlPercussionMode;
extern bool QuitWithoutLooping;
extern bool WritePCMfile;
extern bool ScaleModulators;

#endif

