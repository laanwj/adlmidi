#include "audioout.hh"

#include "adldata.hh"
#include "config.hh"
#include "ui.hh"

#include <assert.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>  // deque
#include <map>
#include <set>
#include <stdarg.h>
#include <string>
#include <unistd.h>
#include <vector>

#ifdef AUDIO_JACK
#include <jack/jack.h>
#include <jack/midiport.h>
#endif

// Comment this out to disable reverb and other postprocessing of the audio
// #define USE_REVERB
// Frequency of volume updates in UI
#define VOLUME_UPDATE_FREQ 24

class AudioPostprocessor;
static AudioGenerator *audio_gen;
static AudioPostprocessor *audio_postprocessor;
static MIDIReceiver *midi_if;

AudioGenerator::~AudioGenerator()
{
}

MIDIReceiver::~MIDIReceiver()
{
}


inline short short_sample_from_float(float in)
{
    const float out = in * 32767.0;
    return out<-32768 ? -32768 : (out>32767 ?  32767 : out);
}

#ifdef AUDIO_SDL
static SDL_AudioSpec obtained;
static void SDL_AudioCallback(void*, Uint8* stream, int len)
{
    short* target = (short*) stream;
    unsigned nframes = len/(2*sizeof(short));
    unsigned bufsize = nframes*2;
    float in[bufsize]; /* need temporary buffer for interleaved samples */
    if(audio_gen)
        audio_gen->RequestSamples(nframes, in);
    for(unsigned a = 0; a < bufsize; ++a)
        target[a] = short_sample_from_float(in[a]);
}
#endif // AUDIO_SDL

#ifdef AUDIO_JACK
jack_port_t *output_port[2];
#define NUM_MIDI_PORTS 1
jack_port_t *midi_port[NUM_MIDI_PORTS];
jack_client_t *client;
// JACK audio callback
static int JACK_AudioCallback(jack_nframes_t nframes, void *)
{
    float *out[2] = {(jack_default_audio_sample_t *) jack_port_get_buffer(output_port[0], nframes),
                     (jack_default_audio_sample_t *) jack_port_get_buffer(output_port[1], nframes)};
    unsigned bufsize = nframes*2;
    float in[bufsize]; /* need temporary buffer for interleaved samples */

    if(midi_if)
    {
        // Process MIDI input
        for(int port=0; port<NUM_MIDI_PORTS; ++port)
        {
            void *port_buf = jack_port_get_buffer(midi_port[port], nframes);
            jack_nframes_t event_count = jack_midi_get_event_count(port_buf);
            for(jack_nframes_t idx=0; idx<event_count; ++idx)
            {
                jack_midi_event_t in_event;
                jack_midi_event_get(&in_event, port_buf, idx);
                midi_if->PushEvent(in_event.time, port, in_event.buffer, in_event.size);
            }
        }
    }

    if(audio_gen)
        audio_gen->RequestSamples(nframes, in);

    for(unsigned a = 0; a < bufsize; ++a)
        out[a&1][a>>1] = in[a];
    return 0;
}
static void JACK_ShutdownCallback(void *)
{
    UI.Cleanup();
    exit(1);
}
#endif // AUDIO_JACK

struct FourChars
{
    char ret[4];

    FourChars(const char* s)
    {
        for(unsigned c=0; c<4; ++c) ret[c] = s[c];
    }
    FourChars(unsigned w) // Little-endian
    {
        for(unsigned c=0; c<4; ++c) ret[c] = (w >> (c*8)) & 0xFF;
    }
};

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
unsigned long upper_power_of_two(unsigned long v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

void InitializeAudio(double AudioBufferLength)
{
#ifdef AUDIO_SDL
    // Set up SDL
    SDL_AudioSpec spec;
    spec.freq     = PCM_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = upper_power_of_two(spec.freq * AudioBufferLength);
    spec.callback = SDL_AudioCallback;
    if(SDL_OpenAudio(&spec, &obtained) < 0)
    {
        UI.InitMessage(-1, "Couldn't open audio: %s\n", SDL_GetError());
        //return 1;
    }
    if(spec.samples != obtained.samples)
        UI.InitMessage(-1, "Wanted (samples=%u,rate=%u,channels=%u); obtained (samples=%u,rate=%u,channels=%u)\n",
            spec.samples,    spec.freq,    spec.channels,
            obtained.samples,obtained.freq,obtained.channels);
#endif
#ifdef AUDIO_JACK
    jack_options_t options = JackNullOption;
    jack_status_t status;
    const char *server_name = NULL;
    const char **ports;
    if ((client = jack_client_open("adlmidi", options, &status, server_name)) == 0) {
        UI.InitMessage(-1, "jack_client_open() failed, status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            UI.InitMessage(-1, "Unable to connect to JACK server\n");
        }
        exit(1);
    }
    if (status & JackServerStarted) {
        UI.InitMessage(-1, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
        UI.InitMessage(-1, "unique name `%s' assigned\n", jack_get_client_name(client));
    }
    jack_set_process_callback(client, JACK_AudioCallback, 0);
    jack_on_shutdown(client, JACK_ShutdownCallback, 0);

    unsigned int jack_rate = (unsigned int)jack_get_sample_rate(client);

    UI.InitMessage(-1, "JACK engine sample rate: %u ", jack_rate);
    if(jack_rate != PCM_RATE)
        UI.InitMessage(-1, "(warning: this differs from adlmidi PCM rate, %u)", (unsigned)PCM_RATE);
    UI.InitMessage(-1, "\n");

    // create two ports, for stereo audio
    const char * const portnames[] = { "out_1", "out_2" };
    const char * const midi_portnames[] = { "midi_1" };
    for(int port=0; port<2; ++port)
    {
        output_port[port] = jack_port_register(client, portnames[port],
                                         JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput, 0);
        if (output_port[port] == NULL) {
            UI.InitMessage(-1, "no more JACK ports available\n");
            exit(1);
        }
    }

    for(int port=0; port<NUM_MIDI_PORTS; ++port)
    {
        midi_port[port] = jack_port_register(client, midi_portnames[port],
                                         JACK_DEFAULT_MIDI_TYPE,
                                         JackPortIsInput, 0);
        if (midi_port[port] == NULL) {
            UI.InitMessage(-1, "no more JACK midi ports available\n");
            exit(1);
        }
    }

    if (jack_activate(client)) {
        UI.InitMessage(-1, "JACK: cannot activate client\n");
        exit(1);
    }

    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical|JackPortIsInput);
    if (ports == NULL) {
        UI.InitMessage(-1, "JACK: no physical playback ports\n");
    }

    for(int port=0; port<2; ++port)
    {
        if (jack_connect(client, jack_port_name(output_port[port]), ports[port])) {
            UI.InitMessage(-1, "JACK: cannot connect output ports\n");
        }
    }
    jack_free(ports);
#endif
}

/** Wrap audio_gen to provide
 *  - volume visualization
 *  - reverb
 *  - final volume scaling
 */
class AudioPostprocessor: public AudioGenerator
{
public:
    AudioPostprocessor(AudioGenerator *source):
        source(source)
    {
    }
    void RequestSamples(unsigned long count, float* samples)
    {
        source->RequestSamples(count, samples);
        // Attempt to filter out the DC component. However, avoid doing
        // sudden changes to the offset, for it can be audible.
        double average[2]={0,0};
        for(unsigned w=0; w<2; ++w)
            for(unsigned long p = 0; p < count; ++p)
                average[w] += samples[p*2+w];
        for(unsigned w=0; w<2; ++w)
                average[w] /= double(count);
        static float prev_avg_flt[2] = {0,0};
        float average_flt[2] =
        {
            prev_avg_flt[0] = (prev_avg_flt[0] + average[0]*0.04) / 1.04,
            prev_avg_flt[1] = (prev_avg_flt[1] + average[1]*0.04) / 1.04
        };
        // Figure out the amplitude of both channels
        static unsigned amplitude_display_counter = 0;
        if(!amplitude_display_counter--)
        {
            amplitude_display_counter = (PCM_RATE / count) / VOLUME_UPDATE_FREQ;
            double amp[2]={0,0};
            for(unsigned w=0; w<2; ++w)
            {
                for(unsigned long p = 0; p < count; ++p)
                    amp[w] += std::fabs(samples[p*2+w] - average[w]);
                amp[w] /= double(count);
                amp[w] *= 10240;
                // Turn into logarithmic scale
                const double dB = std::log(amp[w]<1 ? 1 : amp[w]) * 4.328085123;
                const double maxdB = 3*16; // = 3 * log2(65536)
                amp[w] = dB/maxdB;
            }
            UI.IllustrateVolumes(amp[0], amp[1]);
        }

        if(EnableReverb)
        {
            // TODO: reverb
        }
        for(unsigned long p = 0; p < 2 * count; ++p)
            samples[p] *= SAMPLE_MULT_OUTPUT_FLOAT;
    }

private:
    AudioGenerator *source;
};

void StartAudio(AudioGenerator *gen, MIDIReceiver *midi)
{
    audio_postprocessor = new AudioPostprocessor(gen);
    audio_gen = audio_postprocessor;
    midi_if = midi;
}

#if 0
    /// Code for reverb

struct Reverb /* This reverb implementation is based on Freeverb impl. in Sox */
{
    float feedback, hf_damping, gain;
    struct FilterArray
    {
        struct Filter
        {
            std::vector<float> Ptr;  size_t pos;  float Store;
            void Create(size_t size) { Ptr.resize(size); pos = 0; Store = 0.f; }
            float Update(float a, float b)
            {
                Ptr[pos] = a;
                if(!pos) pos = Ptr.size()-1; else --pos;
                return b;
            }
            float ProcessComb(float input, const float feedback, const float hf_damping)
            {
                Store = Ptr[pos] + (Store - Ptr[pos]) * hf_damping;
                return Update(input + feedback * Store, Ptr[pos]);
            }
            float ProcessAllPass(float input)
            {
                return Update(input + Ptr[pos] * .5f, Ptr[pos]-input);
            }
        } comb[8], allpass[4];
        void Create(double rate, double scale, double offset)
        {
            /* Filter delay lengths in samples (44100Hz sample-rate) */
            static const int comb_lengths[8] = {1116,1188,1277,1356,1422,1491,1557,1617};
            static const int allpass_lengths[4] = {225,341,441,556};
            double r = rate * (1 / 44100.0); // Compensate for actual sample-rate
            const int stereo_adjust = 12;
            for(size_t i=0; i<8; ++i, offset=-offset)
                comb[i].Create( scale * r * (comb_lengths[i] + stereo_adjust * offset) + .5 );
            for(size_t i=0; i<4; ++i, offset=-offset)
                allpass[i].Create( r * (allpass_lengths[i] + stereo_adjust * offset) + .5 );
        }
        void Process(size_t length,
            const std::deque<float>& input, std::vector<float>& output,
            const float feedback, const float hf_damping, const float gain)
        {
            for(size_t a=0; a<length; ++a)
            {
                float out = 0, in = input[a];
                for(size_t i=8; i-- > 0; ) out += comb[i].ProcessComb(in, feedback, hf_damping);
                for(size_t i=4; i-- > 0; ) out += allpass[i].ProcessAllPass(out);
                output[a] = out * gain;
            }
        }
    } chan[2];
    std::vector<float> out[2];
    std::deque<float> input_fifo;

    void Create(double sample_rate_Hz,
        double wet_gain_dB,
        double room_scale, double reverberance, double fhf_damping, /* 0..1 */
        double pre_delay_s, double stereo_depth,
        size_t buffer_size)
    {
        size_t delay = pre_delay_s  * sample_rate_Hz + .5;
        double scale = room_scale * .9 + .1;
        double depth = stereo_depth;
        double a =  -1 /  std::log(1 - /**/.3 /**/);          // Set minimum feedback
        double b = 100 / (std::log(1 - /**/.98/**/) * a + 1); // Set maximum feedback
        feedback = 1 - std::exp((reverberance*100.0 - b) / (a * b));
        hf_damping = fhf_damping * .3 + .2;
        gain = std::exp(wet_gain_dB * (std::log(10.0) * 0.05)) * .015;
        input_fifo.insert(input_fifo.end(), delay, 0.f);
        for(size_t i = 0; i <= std::ceil(depth); ++i)
        {
            chan[i].Create(sample_rate_Hz, scale, i * depth);
            out[i].resize(buffer_size);
        }
    }
    void Process(size_t length)
    {
        for(size_t i=0; i<2; ++i)
            if(!out[i].empty())
                chan[i].Process(length,
                    input_fifo,
                    out[i], feedback, hf_damping, gain);
        input_fifo.erase(input_fifo.begin(), input_fifo.begin() + length);
    }
};
static struct MyReverbData
{
    bool wetonly;
    Reverb chan[2];

    MyReverbData() : wetonly(false)
    {
        for(size_t i=0; i<2; ++i)
            chan[i].Create(PCM_RATE,
                6.0,  // wet_gain_dB  (-10..10)
                .7,   // room_scale   (0..1)
                .6,   // reverberance (0..1)
                .8,   // hf_damping   (0..1)
                .000, // pre_delay_s  (0.. 0.5)
                1,   // stereo_depth (0..1)
                MaxSamplesAtTime);
    }
} reverb_data;

    if(EnableReverb)
    {
        std::vector<float> dry;
        dry.resize(count);
        // Insert input into reverb fifo
        for(unsigned w=0; w<2; ++w)
        {
            const float a = average_flt[w];
            for(unsigned long p = 0; p < count; ++p)
            {
                dry[p] = (samples[p*2+w] - a) * ReverbScale;
            }
            // ^  Note: ftree-vectorize causes an error in this loop on g++-4.4.5
            reverb_data.chan[w].input_fifo.insert(
            reverb_data.chan[w].input_fifo.end(),
                dry.begin(), dry.end());
        }
        // Reverbify it
        for(unsigned w=0; w<2; ++w)
            reverb_data.chan[w].Process(count);
    }

    if(EnableReverb)
    {
        for(unsigned long p = 0; p < count; ++p)
        {
            for(unsigned w = 0; w < 2; ++w)
            {
                const float dry = samples[p*2 + w] - average_flt[w];
                const float wet = ((1 - reverb_data.wetonly) * dry +
                    .5 * (reverb_data.chan[0].out[w][p]
                        + reverb_data.chan[1].out[w][p]));
                AudioBuffer.push_back(sample_from_float(wet));
            }
        }
    }
#endif

void ShutdownAudio()
{
#ifdef AUDIO_SDL
    SDL_CloseAudio();
#endif
#ifdef AUDIO_JACK
    jack_deactivate(client);
    for(int port=0; port<2; ++port)
        jack_port_unregister(client, output_port[port]);
    for(int port=0; port<NUM_MIDI_PORTS; ++port)
        jack_port_unregister(client, midi_port[port]);
    jack_client_close(client);
#endif
    audio_gen = 0;
    delete audio_postprocessor;
}

