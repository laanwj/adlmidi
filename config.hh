#ifndef H_CONFIG
#define H_CONFIG

static const unsigned long PCM_RATE = 48000;
static const unsigned MaxCards = 100;
static const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation

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

