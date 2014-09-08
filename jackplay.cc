#include "adldata.hh"
#include "config.hh"
#include "midievt.hh"
#include "parseargs.hh"
#include "ui.hh"

#include <assert.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <string>
#include <unistd.h>
#include <jack/jack.h>
#include <jack/midiport.h>

static volatile sig_atomic_t QuitFlag = false;
static MIDIeventhandler *evh;
static jack_port_t *output_port[2];
static jack_port_t *midi_port;
static jack_client_t *client;

static void TidyupAndExit(int)
{
    UI.Cleanup();
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

// JACK audio callback
static int JACK_AudioCallback(jack_nframes_t nframes, void *)
{
    float *out[2] = {(jack_default_audio_sample_t *) jack_port_get_buffer(output_port[0], nframes),
                     (jack_default_audio_sample_t *) jack_port_get_buffer(output_port[1], nframes)};
    jack_nframes_t offset = 0;
    jack_nframes_t event_idx = 0;
    void *midi_buf = jack_port_get_buffer(midi_port, nframes);
    jack_nframes_t event_count = jack_midi_get_event_count(midi_buf);
    // invariant: in_event holds event[event_idx] if event_idx<event_count, otherwise undefined
    jack_midi_event_t in_event;

    if(event_idx < event_count)
        jack_midi_event_get(&in_event, midi_buf, event_idx);
    while(offset < nframes)
    {
        jack_nframes_t next_event = nframes;
        if(event_idx < event_count)
            next_event = std::min(in_event.time, next_event);

        float outbuf[MaxSamplesAtTime*2];
        jack_nframes_t n_samples = std::min(next_event - offset, (jack_nframes_t)MaxSamplesAtTime);
        // opl.Update adds in samples, so initialize to zero
        memset(outbuf, 0, n_samples*2*sizeof(float));
        evh->opl.Update(outbuf, n_samples);
        evh->Tick(n_samples / (double)PCM_RATE);
        for(unsigned a = 0; a < n_samples; ++a)
        {
            out[0][offset + a] = outbuf[a*2+0];
            out[1][offset + a] = outbuf[a*2+1];
        }

        while(event_idx < event_count && in_event.time <= offset)
        {
            evh->HandleEvent(0, in_event.buffer, in_event.size);
            event_idx += 1;
            if(event_idx < event_count)
                jack_midi_event_get(&in_event, midi_buf, event_idx);
        }
        offset += n_samples;
    }

    return 0;
}

static void JACK_ShutdownCallback(void *)
{
    UI.Cleanup();
    exit(1);
}

void InitializeAudio()
{
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

    midi_port = jack_port_register(client, "midi_in",
                                 JACK_DEFAULT_MIDI_TYPE,
                                 JackPortIsInput, 0);
    if (midi_port == NULL) {
        UI.InitMessage(-1, "no more JACK midi ports available\n");
        exit(1);
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
}

void ShutdownAudio()
{
    jack_deactivate(client);
    for(int port=0; port<2; ++port)
        jack_port_unregister(client, output_port[port]);
    jack_port_unregister(client, midi_port);
    jack_client_close(client);
}

int main(int argc, char** argv)
{
    UI.InitMessage(15, "ADLSEQ: OPL3 softsynth for Linux\n");
    UI.InitMessage(3, "(C) -- https://github.com/laanwj/adlmidi\n");

    signal(SIGTERM, TidyupAndExit);
    signal(SIGINT, TidyupAndExit);

    int rv = ParseArguments(argc, argv);
    if(rv >= 0)
        return rv;
    InitializeAudio();

    UI.StartGrid();
    evh = new MIDIeventhandler();
    evh->Reset();

    /// XXX no way to quit right now
    while(true)
        sleep(10);

    ShutdownAudio();

    UI.Cleanup();
    return 0;
}
