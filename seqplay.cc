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

#include <signal.h>

#include "adldata.hh"
#include "audioout.hh"
#include "config.hh"
#include "midievt.hh"
#include "parseargs.hh"
#include "ui.hh"

static volatile sig_atomic_t QuitFlag = false;
static volatile bool terminateAlsaThread = false;
static snd_seq_t *seq;
static int port_count;
static snd_midi_event_t *midi_enc;

const uint64_t NANOS_PER_S = 1000000000LL;

// Add this number of samples to time for new midi events to make sure that
// events arrive in the future so to preserve relative timing.
static const int MIDI_DELAY_FRAMES = 2000;

static void TidyupAndExit(int)
{
    UI.Cleanup();
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
    terminateAlsaThread = true;
}

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

class MidiEventQueue
{
private:
    struct MidiEvent
    {
        /* timestamp in samples when this event is to be processed */
        uint32_t timestamp;
        /* output port (channel offset divided by 16) */
        uint8_t port;
        /* first byte of midi message -- if 0xF7 or 0xF0 must be accompanied by
         * entry in sysex queue */
        uint8_t byte;
        /* data bytes of midi message */
        uint8_t data[2];
    };
    typedef std::vector<uint8_t> MidiSysExData;

    std::deque<MidiEvent> eventqueue;
    // std::deque<MidiSysExData> sysexqueue; TODO
    MutexType mutex;
public:
    MidiEventQueue();
    ~MidiEventQueue();

    void PushEvent(uint32_t timestamp, int chanofs, unsigned char byte, const unsigned char *data, unsigned length);
    bool PeekEvent(uint32_t &nextEventTime);
    bool ProcessEvent(MIDIeventhandler *evh);
};
MidiEventQueue::MidiEventQueue()
{
}
MidiEventQueue::~MidiEventQueue()
{
}
void MidiEventQueue::PushEvent(uint32_t timestamp, int chanofs, unsigned char byte, const unsigned char *data, unsigned length)
{
    MidiEvent evt;
    evt.timestamp = timestamp;
    evt.port = chanofs / 16;
    evt.byte = byte;
    // TODO handle sysex
    if(length>0)
        evt.data[0] = data[0];
    if(length>1)
        evt.data[1] = data[1];
    //printf("Inserting event: %i %02x %02x %02x\n", (int)timestamp, evt.byte, evt.data[0], evt.data[1]);
    mutex.Lock();
    eventqueue.push_back(evt);
    mutex.Unlock();
}
bool MidiEventQueue::PeekEvent(uint32_t &nextEventTime)
{
    bool rv = false;
    mutex.Lock();
    if(!eventqueue.empty())
    {
        nextEventTime = eventqueue.front().timestamp;
        rv = true;
    }
    mutex.Unlock();
    return rv;
}
bool MidiEventQueue::ProcessEvent(MIDIeventhandler *evh)
{
    bool rv = false;
    MidiEvent evt;
    // MidiSysExData data; TODO
    mutex.Lock();
    if(!eventqueue.empty())
    {
        evt = eventqueue.front();
        eventqueue.pop_front();
        rv = true;
    }
    mutex.Unlock();
    if(!rv)
        return false;

    // TODO handle sysex
    //printf("Processing event: %i port=%02x %02x %02x %02x length=%i\n", evt.timestamp, evt.port, evt.byte, evt.data[0], evt.data[1],
    //        MidiEventLength(evt.byte));
    evh->HandleEvent(evt.port * 16, evt.byte, evt.data, MidiEventLength(evt.byte));
    return true;
}

uint64_t GetTimeNanos()
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_sec * NANOS_PER_S + tv.tv_nsec;
}

/* Comparison functions with 32-bit wrap-around */
// Return max(to - from, 0)
uint32_t SamplesDiff(uint32_t to, uint32_t from)
{
    uint32_t timeDiff = to - from;
    if(timeDiff > 0x80000000) // Event is in the past!
        return 0;
    else
        return timeDiff;
}
// Return to - from
int32_t SamplesSignedDiff(uint32_t to, uint32_t from)
{
    return to - from; // XXX does this use undefined behavior?
}
// Return true if to > from, false otherwise
bool SamplesLargerThan(uint32_t to, uint32_t from) { return (from - to) > 0x80000000; }
// Return true if to < from, false otherwise
bool SamplesSmallerThan(uint32_t to, uint32_t from) { return (to - from) > 0x80000000; }

class Clock
{
    uint64_t start_time;
public:
    /* Pass value 'ahead' (in nanos) to take audio buffer into account */
    Clock(uint64_t ahead);
    uint32_t NanosToSamples(uint64_t nanos);
    uint32_t CurrentSamples();
    /* Tell the clock what time (in samples) we're currently processing
     * at the receiving end.
     */
    void Sync(uint32_t cur_samples);
};

Clock::Clock(uint64_t ahead):
    start_time(GetTimeNanos() - ahead)
{
}

uint32_t Clock::NanosToSamples(uint64_t nanos)
{
    return (nanos - start_time) * PCM_RATE / NANOS_PER_S;
}

uint32_t Clock::CurrentSamples()
{
    return NanosToSamples(GetTimeNanos());
}

void Clock::Sync(uint32_t out_samples)
{
    uint64_t clk_samples = CurrentSamples();
    int32_t difference = SamplesSignedDiff(clk_samples, out_samples);
    if(abs(difference) > 1000)
    {
        start_time += (int64_t)difference * NANOS_PER_S / PCM_RATE;
        UI.PrintLn("Clock sync correcting difference of %d samples", difference);
    }
}

Clock *midiclock;
MidiEventQueue *midiqueue;

/**
 * in second thread, add incoming MIDI events to queue with timestamp (in nanos)
 * in main thread
 *   - peek into queue
 *   - get current time (in samples)
 *   - generate samples until next event (or up to MaxSamplesAtTime)
 *   - pop and process event
 *   - repeat
 * main thread must be behind realtime by a # of nanos for this to work. This is known as midiLatency in munt.
 * timer must use monotonic time clock_gettime(CLOCK_MONOTONIC)
 * AudioStream::estimateMIDITimestamp(nanos) called to determine at what time to insert the event into the queue
 * -> this converts nanos to samples.
 * Define midi event type with timestamp in samples and either embedded data or pointer to sysex (or ignore sysex for now).
 * Use a std::deque as event queue, or a ring buffer, protected by mutex.
 */
static void handle_alsa_event(MidiEventQueue *evh, uint32_t timestamp, const snd_seq_event_t *ev)
{
    // Handle SysEx as special case as it is already in raw format
    if(ev->type == SND_SEQ_EVENT_SYSEX)
    {
        if(ev->data.ext.len >= 1)
        {
            const unsigned char *data = (unsigned char*)ev->data.ext.ptr;
            evh->PushEvent(timestamp, 0, data[0], &data[1], ev->data.ext.len-1);
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
        evh->PushEvent(timestamp, 0, byte, &buf[ptr], length);
        ptr += length;
    }
}

int AlsaThread(void *)
{
    int err;
    int npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
    struct pollfd *pfds = (struct pollfd *)alloca(sizeof(*pfds) * npfds);
    while(!terminateAlsaThread)
    {
        snd_seq_poll_descriptors(seq, pfds, npfds, POLLIN);
        int rv = poll(pfds, npfds, 100);
        if(rv < 0)
            break;
        do {
            snd_seq_event_t *event;
            err = snd_seq_event_input(seq, &event);
            if (err < 0)
                break;
            if (event)
                handle_alsa_event(midiqueue, midiclock->CurrentSamples() + MIDI_DELAY_FRAMES, event);
        } while (err > 0);
    }
    return 0;
}

int main(int argc, char** argv)
{
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    const double AudioBufferLength = 0.025;
    // How much do WE buffer, in seconds? The smaller the value, // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.05;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.

    UI.InitMessage(15, "ADLSEQ: OPL3 softsynth for Linux\n");
    UI.InitMessage(3, "(C) -- https://github.com/laanwj/adlmidi\n");

    signal(SIGTERM, TidyupAndExit);
    signal(SIGINT, TidyupAndExit);

    InitializeAudio(AudioBufferLength, OurHeadRoomLength);
    int rv = ParseArguments(argc, argv);
    if(rv >= 0)
        return rv;

    // ALSA
    int err;
    init_seq();
    create_port();
    err = snd_seq_nonblock(seq, 1);
    check_snd("set nonblock mode", err);
    if (port_count > 0)
        UI.InitMessage(-1, "Waiting for data.");
    else
        UI.InitMessage(-1, "Waiting for data at port %d:0.",
               snd_seq_client_id(seq));
    UI.InitMessage(-1, "\n");
    SDL_Thread *thread = SDL_CreateThread(AlsaThread, 0);

    MIDIeventhandler evh;
    evh.Reset();

    midiclock = new Clock((AudioBufferLength + OurHeadRoomLength) * NANOS_PER_S);
    midiqueue = new MidiEventQueue();
    StartAudio();
    uint32_t cur_samples = 0;
    while( !QuitFlag )
    {
        float buffer[MaxSamplesAtTime*2] = {};
        unsigned long n_samples = MaxSamplesAtTime;
        uint32_t nextEventTime;
        if(midiqueue->PeekEvent(nextEventTime))
        {
            n_samples = std::min(SamplesDiff(nextEventTime, cur_samples), MaxSamplesAtTime);
            //printf("current time %i, next event at %i\n", (int)cur_samples, (int)nextEventTime);
        } else {
            //printf("current time %i, no next event\n", (int)cur_samples);
        }
	evh.opl.Update(buffer, n_samples);
	SendStereoAudio(n_samples, buffer);

        AudioWait();
        evh.Tick(n_samples / (double)PCM_RATE);

        cur_samples += n_samples;
        // Process events as long as they're either now or in the past
        while(midiqueue->PeekEvent(nextEventTime))
        {
            if(SamplesSmallerThan(nextEventTime, cur_samples))
                UI.PrintLn("Warning: processing event in the past %i<%i\n", (int)nextEventTime, (int)cur_samples);
            uint32_t timeDiff = SamplesDiff(nextEventTime, cur_samples);
            if(timeDiff > 0)
                break;
            midiqueue->ProcessEvent(&evh);
        }
        midiclock->Sync(cur_samples);
    }

    ShutdownAudio();

    // Terminate ALSA
    terminateAlsaThread = true;
    SDL_WaitThread(thread, NULL);
    delete midiclock;
    delete midiqueue;
    snd_seq_close(seq);
    UI.Cleanup();
    return 0;
}
