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
#include "audioout.hh"
#include "config.hh"
#include "midievt.hh"
#include "parseargs.hh"
#include "ui.hh"

volatile sig_atomic_t QuitFlag = false;

class Input
{
#ifdef __WIN32__
    void* inhandle;
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    struct termio back;
#endif
public:
    Input()
    {
#ifdef __WIN32__
        inhandle = GetStdHandle(STD_INPUT_HANDLE);
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
        ioctl(0, TCGETA, &back);
        struct termio term = back;
        term.c_lflag &= ~(ICANON|ECHO);
        term.c_cc[VMIN] = 0; // 0=no block, 1=do block
        if(ioctl(0, TCSETA, &term) < 0)
            fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
    }
    ~Input()
    {
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
        if(ioctl(0, TCSETA, &back) < 0)
            fcntl(0, F_SETFL, fcntl(0, F_GETFL) &~ O_NONBLOCK);
#endif
    }

    char PeekInput()
    {
#ifdef __DJGPP__
        if(kbhit()) { int c = getch(); return c ? c : getch(); }
#endif
#ifdef __WIN32__
        DWORD nread=0;
        INPUT_RECORD inbuf[1];
        while(PeekConsoleInput(inhandle,inbuf,sizeof(inbuf)/sizeof(*inbuf),&nread)&&nread)
        {
            ReadConsoleInput(inhandle,inbuf,sizeof(inbuf)/sizeof(*inbuf),&nread);
            if(inbuf[0].EventType==KEY_EVENT
            && inbuf[0].Event.KeyEvent.bKeyDown)
            {
                char c = inbuf[0].Event.KeyEvent.uChar.AsciiChar;
                unsigned s = inbuf[0].Event.KeyEvent.wVirtualScanCode;
                if(c == 0) c = s;
                return c;
        }   }
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
        char c = 0;
        if(read(0, &c, 1) == 1) return c;
#endif
        return '\0';
    }
} Input;

class Tester
{
    unsigned cur_gm;
    unsigned ins_idx;
    std::vector<unsigned> adl_ins_list;
    OPL3& opl;
public:
    Tester(OPL3& o) : opl(o)
    {
        cur_gm   = 0;
        ins_idx  = 0;
    }
    ~Tester()
    {
    }

    // Find list of adlib instruments that supposedly implement this GM
    void FindAdlList()
    {
        const unsigned NumBanks = sizeof(banknames)/sizeof(*banknames);

        std::set<unsigned> adl_ins_set;
        for(unsigned bankno=0; bankno<NumBanks; ++bankno)
            adl_ins_set.insert(banks[bankno][cur_gm]);
        adl_ins_list.assign( adl_ins_set.begin(), adl_ins_set.end() );
        ins_idx = 0;
        NextAdl(0);
        opl.Silence();
    }

    void DoNote(int note)
    {
        if(adl_ins_list.empty()) FindAdlList();
        int meta = adl_ins_list[ins_idx];
        const adlinsdata& ains = adlins[meta];
        int tone = (cur_gm & 128) ? (cur_gm & 127) : (note+50);
        if(ains.tone)
        {
            if(ains.tone < 20)
                tone += ains.tone;
            else if(ains.tone < 128)
                tone = ains.tone;
            else
                tone -= ains.tone-128;
        }
        double hertz = 172.00093 * std::exp(0.057762265 * (tone + 0.0));
        int i[2] = { ains.adlno1, ains.adlno2 };
        int adlchannel[2] = { 0, 3 };
        if(i[0] == i[1])
        {
            adlchannel[1] = -1;
            adlchannel[0] = 6; // single-op
            std::printf("noteon at %d(%d) for %g Hz\n",
                adlchannel[0], i[0], hertz);
        }
        else
        {
            std::printf("noteon at %d(%d) and %d(%d) for %g Hz\n",
                adlchannel[0], i[0], adlchannel[1], i[1], hertz);
        }

        opl.NoteOff(0); opl.NoteOff(3); opl.NoteOff(6);
        for(unsigned c=0; c<2; ++c)
        {
            if(adlchannel[c] < 0) continue;
            opl.Patch(adlchannel[c], i[c]);
            opl.Touch(adlchannel[c], 127*127*100);
            opl.Pan(adlchannel[c], 0x30);
            opl.NoteOn(adlchannel[c], hertz);
        }
    }

    void NextGM(int offset)
    {
        cur_gm = (cur_gm + 256 + offset) & 0xFF;
        FindAdlList();
    }

    void NextAdl(int offset)
    {
        if(adl_ins_list.empty()) FindAdlList();
        const unsigned NumBanks = sizeof(banknames)/sizeof(*banknames);
        ins_idx = (ins_idx + adl_ins_list.size() + offset) % adl_ins_list.size();

        UI.Color(15); std::fflush(stderr);
        std::printf("SELECTED G%c%d\t%s\n",
            cur_gm<128?'M':'P', cur_gm<128?cur_gm+1:cur_gm-128,
            "<-> select GM, ^v select ins, qwe play note");
        std::fflush(stdout);
        UI.Color(7); std::fflush(stderr);
        for(unsigned a=0; a<adl_ins_list.size(); ++a)
        {
            unsigned i = adl_ins_list[a];
            char ToneIndication[8] = "   ";
            if(adlins[i].tone)
            {
                if(adlins[i].tone < 20)
                    sprintf(ToneIndication, "+%-2d", adlins[i].tone);
                else if(adlins[i].tone < 128)
                    sprintf(ToneIndication, "=%-2d", adlins[i].tone);
                else
                    sprintf(ToneIndication, "-%-2d", adlins[i].tone-128);
            }
            std::printf("%s%s%s%u\t",
                ToneIndication,
                adlins[i].adlno1 != adlins[i].adlno2 ? "[2]" : "   ",
                (ins_idx == a) ? "->" : "\t",
                i
            );

            for(unsigned bankno=0; bankno<NumBanks; ++bankno)
                if(banks[bankno][cur_gm] == i)
                    std::printf(" %u", bankno);

            std::printf("\n");
        }
    }

    void HandleInputChar(char ch)
    {
        static const char notes[] = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
        //                           c'd'ef'g'a'bC'D'EF'G'A'Bc'd'e
        switch(ch)
        {
            case '/': case 'H': case 'A': NextAdl(-1); break;
            case '*': case 'P': case 'B': NextAdl(+1); break;
            case '-': case 'K': case 'D': NextGM(-1); break;
            case '+': case 'M': case 'C': NextGM(+1); break;
            case 3:
        #if !((!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__))
            case 27:
        #endif
                QuitFlag=true; break;
            default:
                const char* p = strchr(notes, ch);
                if(p && *p) DoNote( (p - notes) - 12 );
    }   }

    double Tick(double eat_delay, double mindelay)
    {
        HandleInputChar( Input.PeekInput() );
        return 0.1; //eat_delay;
    }
};

static void TidyupAndExit(int)
{
    UI.ShowCursor();
    UI.Color(7);
    std::fflush(stderr);
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

#ifdef __WIN32__
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    extern int main(int,char**);
    char* cmdline = GetCommandLine();
    int argc = ParseCommandLine(cmdline, NULL);
    char**argv = new char* [argc+1];
    ParseCommandLine(cmdline, argv);
#else
#undef main

int main(int argc, char** argv)
{
#endif
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    const double AudioBufferLength = 0.045;
    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.1;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.

    UI.Color(15); std::fflush(stderr);
    std::printf(
        "ADLMIDI: MIDI player for Linux and Windows with OPL3 emulation\n"
    );
    std::fflush(stdout);
    UI.Color(3); std::fflush(stderr);
    std::printf(
        "(C) 2011 Joel Yliluoma -- http://bisqwit.iki.fi/source/adlmidi.html\n");
    std::fflush(stdout);
    UI.Color(7); std::fflush(stderr);

    signal(SIGTERM, TidyupAndExit);
    signal(SIGINT, TidyupAndExit);

    InitializeAudio(AudioBufferLength);
    int rv = ParseArguments(argc, argv);
    if(rv >= 0)
        return rv;

    MIDIeventhandler evh;
    evh.Reset();
    Tester InstrumentTester(evh.opl);

    const double mindelay = 1 / (double)PCM_RATE;
    const double maxdelay = MaxSamplesAtTime / (double)PCM_RATE;

    StartAudio();
    for(double delay=0; !QuitFlag; )
    {
        const double eat_delay = delay < maxdelay ? delay : maxdelay;
        delay -= eat_delay;

        static double carry = 0.0;
        carry += PCM_RATE * eat_delay;
        const unsigned long n_samples = (unsigned) carry;
        carry -= n_samples;

        if(NumCards == 1)
        {
            evh.opl.cards[0].Generate(0, SendStereoAudio, n_samples);
        }
        else if(n_samples > 0)
        {
            /* Mix together the audio from different cards */
            static std::vector<int> sample_buf;
            sample_buf.clear();
            sample_buf.resize(n_samples*2);
            struct Mix
            {
                static void AddStereoAudio(unsigned long count, int* samples)
                {
                    for(unsigned long a=0; a<count*2; ++a)
                        sample_buf[a] += samples[a];
                }
            };
            for(unsigned card = 0; card < NumCards; ++card)
            {
                evh.opl.cards[card].Generate(
                    0,
                    Mix::AddStereoAudio,
                    n_samples);
            }
            /* Process it */
            SendStereoAudio(n_samples, &sample_buf[0]);

            AudioWait(OurHeadRoomLength);
        }
        double nextdelay = InstrumentTester.Tick(eat_delay, mindelay);
        UI.ShowCursor();

        delay = nextdelay;
    }

    ShutdownAudio();

    return 0;
}

