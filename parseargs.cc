#include "parseargs.hh"

#include "adldata.hh"
#include "config.hh"
#include "ui.hh"

#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

unsigned AdlBank    = 0;
unsigned NumFourOps = 7;
unsigned NumCards   = 2;
bool HighTremoloMode   = false;
bool HighVibratoMode   = false;
bool AdlPercussionMode = false;
bool QuitWithoutLooping = false;
bool WritePCMfile = false;
bool ScaleModulators = false;
OPLEmuType EmuType = OPLEMU_DBOPLv2;
bool FullPan = true;
bool AllowBankSwitch = false;
bool EnableReverb = true;

int ParseArguments(int argc, char **argv)
{
    if(argc < 2)
    {
        InitMessage(-1,
            "Usage: adlmidi <midifilename> [ <options> ] [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            "       adlmidi <midifilename> -1   To enter instrument tester\n"
            " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
            " -emu=<emu> Set OPL emulator to use (dbopl, dboplv2, vintage, ym3812, ymf262)\n"
            " -fp Enable full stereo panning\n"
            " -bs Allow bank switch (Bank LSB changes bank)\n"
            " -noreverb Disable reverb\n"
        );
        for(unsigned a=0; a<sizeof(banknames)/sizeof(*banknames); ++a)
            InitMessage(-1, "%10s%2u = %s\n",
                a?"":"Banks:",
                a,
                banknames[a]);
        InitMessage(-1,
            "     Use banks 2-5 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            "     The fourth parameter can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
            );
        return 0;
    }

    while(argc > 2)
    {
        if(!std::strcmp("-p", argv[2]))
            AdlPercussionMode = true;
        else if(!std::strcmp("-v", argv[2]))
            HighVibratoMode = true;
        else if(!std::strcmp("-t", argv[2]))
            HighTremoloMode = true;
        else if(!std::strcmp("-nl", argv[2]))
            QuitWithoutLooping = true;
        else if(!std::strcmp("-w", argv[2]))
            WritePCMfile = true;
        else if(!std::strcmp("-s", argv[2]))
            ScaleModulators = true;
        else if(!std::strcmp("-fp", argv[2]))
            FullPan = true;
        else if(!std::strncmp("-emu=", argv[2], 5))
	{
	    const char *emu = argv[2]+5;
	    if(!std::strcmp("dbopl", emu))
		EmuType = OPLEMU_DBOPL;
	    else if(!std::strcmp("dboplv2", emu))
	        EmuType = OPLEMU_DBOPLv2;
	    else if(!std::strcmp("vintage", emu))
	        EmuType = OPLEMU_VintageTone;
	    else if(!std::strcmp("ymf262", emu))
	        EmuType = OPLEMU_YMF262;
	    else
	    {
		InitMessage(12, "unknown opl emulator %s.\n", emu);
		return 0;
	    }
	}
        else if(!std::strcmp("-bs", argv[2]))
            AllowBankSwitch = true;
        else if(!std::strcmp("-noreverb", argv[2]))
            EnableReverb = false;
        else break;

        for(int p=2; p<argc; ++p) argv[p] = argv[p+1];
        --argc;
    }

    if(argc >= 3)
    {
        int bankno = std::atoi(argv[2]);
        AdlBank = bankno;
        if(AdlBank >= NumBanks)
        {
            InitMessage(12, "bank number may only be 0..%u.\n", NumBanks-1);
            return 0;
        }
        InitMessage(-1, "FM instrument bank %u '%s' selected.\n", AdlBank, banknames[AdlBank]);
    }

    unsigned n_fourop[2] = {0,0}, n_total[2] = {0,0};
    for(unsigned a=0; a<256; ++a)
    {
        unsigned insno = banks[AdlBank][a];
        if(insno == 198) continue;
        ++n_total[a/128];
        if(adlins[insno].adlno1 != adlins[insno].adlno2)
            ++n_fourop[a/128];
    }
    InitMessage(-1, "This bank has %u/%u four-op melodic instruments and %u/%u percussive ones.\n",
        n_fourop[0], n_total[0],
        n_fourop[1], n_total[1]);

    if(argc >= 4)
    {
        NumCards = std::atoi(argv[3]);
        if(NumCards < 1 || NumCards > MaxCards)
        {
            InitMessage(12, "number of cards may only be 1..%u.\n", MaxCards);
            return 0;
        }
    }
    if(argc >= 5)
    {
        NumFourOps = std::atoi(argv[4]);
        if(NumFourOps > 6 * NumCards)
        {
            InitMessage(12, "number of four-op channels may only be 0..%u when %u OPL3 cards are used.\n",
                6*NumCards, NumCards);
            return 0;
        }
    }
    else
        NumFourOps =
            (n_fourop[0] >= n_total[0]*7/8) ? NumCards * 6
          : (n_fourop[0] < n_total[0]*1/8) ? 0
          : (NumCards==1 ? 1 : NumCards*4);

    InitMessage(-1,
        "Simulating %u OPL3 cards for a total of %u operators.\n"
        "Setting up the operators as %u four-op channels, %u dual-op channels",
        NumCards, NumCards*36,
        NumFourOps, (AdlPercussionMode ? 15 : 18) * NumCards - NumFourOps*2);
    if(AdlPercussionMode)
        InitMessage(-1, ", %u percussion channels", NumCards * 5);
    InitMessage(-1, "\n");

    if(n_fourop[0] >= n_total[0]*15/16 && NumFourOps == 0)
    {
        InitMessage(12,
            "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
            "       The results (silence + much cpu load) would be probably\n"
            "       not what you want, therefore ignoring the request.\n");
        return 0;
    }

    return -1;
}

