//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif

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
#include <alsa/asoundlib.h>

#include <assert.h>

#if !defined(__WIN32__) || defined(__CYGWIN__)
# include <termio.h>
# include <fcntl.h>
# include <sys/ioctl.h>
#endif

#include <signal.h>

#include "adldata.hh"
#include "audioout.hh"
#include "config.hh"
#include "midievt.hh"
#include "parseargs.hh"
#include "ui.hh"

volatile sig_atomic_t QuitFlag = false;

static void TidyupAndExit(int)
{
    UI.Cleanup();
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

static snd_seq_t *seq;
static int port_count;
static snd_midi_event_t *midi_enc;

/* prints an error message to stderr, and dies */
static void fatal(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

/* memory allocation error handling */
static void check_mem(void *p)
{
    if (!p)
        fatal("Out of memory");
}

/* error handling for ALSA functions */
static void check_snd(const char *operation, int err)
{
    if (err < 0)
        fatal("Cannot %s - %s", operation, snd_strerror(err));
}

static void init_seq()
{
    int err;

    /* open sequencer */
    err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
    check_snd("open sequencer", err);

    /* set our client's name */
    err = snd_seq_set_client_name(seq, "adlmidi");
    check_snd("set client name", err);

    /* create event encoder */
    err = snd_midi_event_new(16, &midi_enc);
    check_snd("create midi event encoder", err);
    /* always write command bytes */
    snd_midi_event_no_status(midi_enc, 1);
}

static void create_port(void)
{
    int err;

    err = snd_seq_create_simple_port(seq, "adlmidi",
                     SND_SEQ_PORT_CAP_WRITE |
                     SND_SEQ_PORT_CAP_SUBS_WRITE,
                     SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                     SND_SEQ_PORT_TYPE_APPLICATION);
    check_snd("create port", err);
}

static void handle_alsa_event(MIDIeventhandler *evh, const snd_seq_event_t *ev)
{
    // Handle SysEx as special case as it is already in raw format
    if(ev->type == SND_SEQ_EVENT_SYSEX)
    {
        if(ev->data.ext.len >= 1)
        {
            const unsigned char *data = (unsigned char*)ev->data.ext.ptr;
            evh->HandleEvent(0, data[0], &data[1], ev->data.ext.len-1);
        }
        return;
    }
    // Convert to MIDI bytes and feed to sequencer
    unsigned char buf[16];
    long bytes = snd_midi_event_decode(midi_enc, buf, sizeof(buf), ev);
    if(bytes == -EINVAL || bytes == -ENOENT)
        return; // ev is not a MIDI event or not valid according to the function, just ignore it
    check_snd("decode midi event", (int)bytes);
    for(long ptr=0; ptr<bytes; )
    {
        unsigned char byte = buf[ptr++];
        unsigned length = MidiEventLength(byte);
        evh->HandleEvent(0, byte, &buf[ptr], length);
        ptr += length;
    }
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
    const double AudioBufferLength = 0.025;
    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.05;
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

    InitializeAudio(AudioBufferLength, OurHeadRoomLength);
    int rv = ParseArguments(argc, argv);
    if(rv >= 0)
        return rv;

    MIDIeventhandler evh;
    evh.Reset();

    const unsigned long n_samples = MaxSamplesAtTime;
    const double delay = n_samples / (double)PCM_RATE;

    // ALSA
    int err;
    init_seq();
    create_port();
    err = snd_seq_nonblock(seq, 1);
    check_snd("set nonblock mode", err);
    if (port_count > 0)
        printf("Waiting for data.");
    else
        printf("Waiting for data at port %d:0.",
               snd_seq_client_id(seq));
    printf("\n");

    StartAudio();
    while( !QuitFlag )
    {
        if(NumCards == 1)
        {
            evh.opl.cards[0].Generate(0, SendStereoAudio, n_samples);
        }
        else
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
        }

        AudioWait();
        evh.Tick(delay);

        do {
            snd_seq_event_t *event;
            err = snd_seq_event_input(seq, &event);
            if (err < 0)
                break;
            if (event)
                handle_alsa_event(&evh, event);
        } while (err > 0);

        UI.ShowCursor();
    }

    ShutdownAudio();
    snd_seq_close(seq);
    UI.Cleanup();
    return 0;
}
