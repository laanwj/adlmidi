#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <stdarg.h>
#include <cstdio>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil

#include <assert.h>

#if !defined(__WIN32__) || defined(__CYGWIN__)
# include <termio.h>
# include <fcntl.h>
# include <sys/ioctl.h>
#endif

#include <deque>
#include <algorithm>

#include <signal.h>

#include "adldata.hh"
#include "config.hh"
#include "ui.hh"

unsigned AdlBank    = 0;
unsigned NumFourOps = 7;
unsigned NumCards   = 2;
bool HighTremoloMode   = false;
bool HighVibratoMode   = false;
bool AdlPercussionMode = false;
bool QuitWithoutLooping = false;
bool WritePCMfile = false;
bool ScaleModulators = false;

int ParseArguments(int argc, char **argv)
{
    if(argc < 2)
    {
        UI.Color(7);  std::fflush(stderr);
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            "       adlmidi <midifilename> -1   To enter instrument tester\n"
            " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
        );
        for(unsigned a=0; a<sizeof(banknames)/sizeof(*banknames); ++a)
            std::printf("%10s%2u = %s\n",
                a?"":"Banks:",
                a,
                banknames[a]);
        std::printf(
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
        else break;

        for(int p=2; p<argc; ++p) argv[p] = argv[p+1];
        --argc;
    }

    if(argc >= 3)
    {
        const unsigned NumBanks = sizeof(banknames)/sizeof(*banknames);
        int bankno = std::atoi(argv[2]);
        AdlBank = bankno;
        if(AdlBank >= NumBanks)
        {
            std::fprintf(stderr, "bank number may only be 0..%u.\n", NumBanks-1);
            return 0;
        }
        std::printf("FM instrument bank %u '%s' selected.\n", AdlBank, banknames[AdlBank]);
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
    std::printf("This bank has %u/%u four-op melodic instruments and %u/%u percussive ones.\n",
        n_fourop[0], n_total[0],
        n_fourop[1], n_total[1]);

    if(argc >= 4)
    {
        NumCards = std::atoi(argv[3]);
        if(NumCards < 1 || NumCards > MaxCards)
        {
            std::fprintf(stderr, "number of cards may only be 1..%u.\n", MaxCards);
            return 0;
        }
    }
    if(argc >= 5)
    {
        NumFourOps = std::atoi(argv[4]);
        if(NumFourOps > 6 * NumCards)
        {
            std::fprintf(stderr, "number of four-op channels may only be 0..%u when %u OPL3 cards are used.\n",
                6*NumCards, NumCards);
            return 0;
        }
    }
    else
        NumFourOps =
            (n_fourop[0] >= n_total[0]*7/8) ? NumCards * 6
          : (n_fourop[0] < n_total[0]*1/8) ? 0
          : (NumCards==1 ? 1 : NumCards*4);

    std::printf(
        "Simulating %u OPL3 cards for a total of %u operators.\n"
        "Setting up the operators as %u four-op channels, %u dual-op channels",
        NumCards, NumCards*36,
        NumFourOps, (AdlPercussionMode ? 15 : 18) * NumCards - NumFourOps*2);
    if(AdlPercussionMode)
        std::printf(", %u percussion channels", NumCards * 5);
    std::printf("\n");
    std::fflush(stdout);

    if(n_fourop[0] >= n_total[0]*15/16 && NumFourOps == 0)
    {
        std::fprintf(stderr,
            "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
            "       The results (silence + much cpu load) would be probably\n"
            "       not what you want, therefore ignoring the request.\n");
        return 0;
    }


    return -1;
}

#ifdef __WIN32__
/* Parse a command line buffer into arguments */
static void UnEscapeQuotes( char *arg )
{
    for(char *last=0; *arg != '\0'; last=arg++)
        if( *arg == '"' && *last == '\\' ) {
            char *c_last = last;
            for(char*c_curr=arg; *c_curr; ++c_curr) {
                *c_last = *c_curr;
                c_last = c_curr;
            }
            *c_last = '\0';
        }
}
int ParseCommandLine(char *cmdline, char **argv)
{
    char *bufp, *lastp=NULL;
    int argc=0, last_argc=0;
    for (bufp = cmdline; *bufp; ) {
        /* Skip leading whitespace */
        while ( std::isspace(*bufp) ) ++bufp;
        /* Skip over argument */
        if ( *bufp == '"' ) {
            ++bufp;
            if ( *bufp ) { if (argv) argv[argc]=bufp; ++argc; }
            /* Skip over word */
            while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
                lastp = bufp;
                ++bufp;
            }
        } else {
            if ( *bufp ) { if (argv) argv[argc] = bufp; ++argc; }
            /* Skip over word */
            while ( *bufp && ! std::isspace(*bufp) ) ++bufp;
        }
        if(*bufp) { if(argv) *bufp = '\0'; ++bufp; }
        /* Strip out \ from \" sequences */
        if( argv && last_argc != argc ) UnEscapeQuotes( argv[last_argc]);
        last_argc = argc;
    }
    if(argv) argv[argc] = 0;
    return(argc);
}
#endif

