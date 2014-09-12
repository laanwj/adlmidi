/* LV2 softsynth */
#include "adldata.hh"
#include "config.hh"
#include "midievt.hh"
#include "uiinterface.hh"

#include <assert.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <string>
#include <unistd.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/ext/log/log.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define ADLMIDI_URI "http://github.com/laanwj/adlmidi"

/* XXX duplicated from parseargs.cc, get rid of these globals completely */
unsigned AdlBank    = 0;
unsigned NumFourOps = 7;
unsigned NumCards   = 2;
bool HighTremoloMode   = false;
bool HighVibratoMode   = false;
bool AdlPercussionMode = false;
bool ScaleModulators = false;
OPLEmuType EmuType = OPLEMU_DBOPLv2;
bool FullPan = true;
bool AllowBankSwitch = true;

class AdlMidiPlugin
{
public:
    enum PortIndex: uint32_t
    {
        CONTROL = 0,
        OUT_L   = 1,
        OUT_R   = 2
    };
    AdlMidiPlugin(const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
        const LV2_Feature* const* features);
    ~AdlMidiPlugin();

    void connect_port(uint32_t port, void* data);
    void activate();
    void run(uint32_t sample_count);
    void deactivate();

    struct Features
    {
        LV2_URID_Map* map;
        LV2_Log_Log* log;
    };
    struct URIs
    {
        LV2_URID midi_MidiEvent;
        LV2_URID log_Error;
        LV2_URID log_Note;
        LV2_URID log_Trace;
        LV2_URID log_Warning;
    };
private:
    Features m_features;
    URIs m_uris;
    struct Ports
    {
        const LV2_Atom_Sequence* control;
        float *out[2];
    } m_ports;

    std::string m_bundle_path;
    double m_rate;

    MIDIeventhandler *evh;
    UIInterface *ui;
};

class ADLUIInterface_LV2: public UIInterface
{
public:
    ADLUIInterface_LV2(const AdlMidiPlugin::Features &features, const AdlMidiPlugin::URIs &uris):
        m_features(features), m_uris(uris) {}
    ~ADLUIInterface_LV2() {}

    void PrintLn(const char* fmt, ...) __attribute__((format(printf,2,3)))
    {
        if (!m_features.log)
            return;
        va_list ap;
        va_start(ap, fmt);
        m_features.log->vprintf(m_features.log->handle, m_uris.log_Note, fmt, ap);
        m_features.log->printf(m_features.log->handle, m_uris.log_Note, "\n");
        va_end(ap);
    }
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend) {}
    void IllustrateVolumes(double left, double right) {}
    void IllustratePatchChange(int MidCh, int patch, int adlinsid) {}
private:
    const AdlMidiPlugin::Features &m_features;
    const AdlMidiPlugin::URIs &m_uris;
};

AdlMidiPlugin::AdlMidiPlugin(const LV2_Descriptor* descriptor, double rate, const char* bundle_path,
    const LV2_Feature* const* features):
    m_rate(rate), evh(0), ui(0)
{
    memset(&m_features, 0, sizeof(m_features));
    memset(&m_uris, 0, sizeof(m_uris));
    memset(&m_ports, 0, sizeof(m_ports));

    for (int i = 0; features[i]; ++i)
    {
        if (!strcmp(features[i]->URI, LV2_URID__map))
            m_features.map = (LV2_URID_Map*)features[i]->data;
        if (!strcmp(features[i]->URI, LV2_LOG__log))
            m_features.log = (LV2_Log_Log*)features[i]->data;
    }

    if (m_features.map)
    {
        m_uris.midi_MidiEvent = m_features.map->map(m_features.map->handle, LV2_MIDI__MidiEvent);
        m_uris.log_Error = m_features.map->map(m_features.map->handle, LV2_LOG__Error);
        m_uris.log_Note = m_features.map->map(m_features.map->handle, LV2_LOG__Note);
        m_uris.log_Trace = m_features.map->map(m_features.map->handle, LV2_LOG__Trace);
        m_uris.log_Warning = m_features.map->map(m_features.map->handle, LV2_LOG__Warning);
    }

    m_bundle_path = bundle_path;
}

AdlMidiPlugin::~AdlMidiPlugin()
{
}

void AdlMidiPlugin::connect_port(uint32_t port, void* data)
{
    switch(port)
    {
    case PortIndex::CONTROL:
        m_ports.control = static_cast<const LV2_Atom_Sequence*>(data);
        break;
    case PortIndex::OUT_L:
        m_ports.out[0] = static_cast<float*>(data);
        break;
    case PortIndex::OUT_R:
        m_ports.out[1] = static_cast<float*>(data);
        break;
    }
}

void AdlMidiPlugin::activate()
{
    ui = new ADLUIInterface_LV2(m_features, m_uris);
    evh = new MIDIeventhandler((int)m_rate, ui);
    evh->Reset();
}

static inline void write_samples(MIDIeventhandler *evh, float *out[2], uint32_t offset, uint32_t count)
{
    float outbuf[MaxSamplesAtTime*2];
    while (count > 0) // Some of the underlying synths are limited to 512 samples at a time
    {
        uint32_t n_samples = std::min(count, MaxSamplesAtTime);
        // Update adds in samples, so initialize to zero
        memset(outbuf, 0, n_samples*2*sizeof(float));
        evh->Update(outbuf, n_samples);
        for(unsigned a = 0; a < n_samples; ++a)
        {
            out[0][offset + a] = outbuf[a*2+0];
            out[1][offset + a] = outbuf[a*2+1];
        }
        count -= n_samples;
        offset += n_samples;
    }
}

void AdlMidiPlugin::run(uint32_t sample_count)
{
    if (!m_ports.control || !m_ports.out[0] || !m_ports.out[1])
        return;
    uint32_t offset = 0;
    LV2_ATOM_SEQUENCE_FOREACH(m_ports.control, ev)
    {
        write_samples(evh, m_ports.out, offset, ev->time.frames - offset);
        offset = (uint32_t)ev->time.frames;

        if (ev->body.type == m_uris.midi_MidiEvent)
            evh->HandleEvent(0, (uint8_t *)LV2_ATOM_BODY(&ev->body), ev->body.size);
    }
    write_samples(evh, m_ports.out, offset, sample_count - offset);
}

void AdlMidiPlugin::deactivate()
{
    delete ui; ui = 0;
    delete evh; evh = 0;
}

//////////////////////////////////////////
// LV2 plugin C++ wrapper

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
    if (!strcmp(descriptor->URI, ADLMIDI_URI))
        return static_cast<LV2_Handle>(
            new AdlMidiPlugin(descriptor, rate, bundle_path, features));
    return NULL;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
    static_cast<AdlMidiPlugin*>(instance)->connect_port(port, data);
}

static void
activate(LV2_Handle instance)
{
    static_cast<AdlMidiPlugin*>(instance)->activate();
}

static void
run(LV2_Handle instance, uint32_t sample_count)
{
    static_cast<AdlMidiPlugin*>(instance)->run(sample_count);
}

static void
deactivate(LV2_Handle instance)
{
    static_cast<AdlMidiPlugin*>(instance)->deactivate();
}

static void
cleanup(LV2_Handle instance)
{
    delete static_cast<AdlMidiPlugin*>(instance);
}

static const void*
extension_data(const char* uri)
{
    return NULL;
}

static const LV2_Descriptor descriptor = {
    ADLMIDI_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
__attribute__ ((visibility ("default")))
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    switch (index)
    {
    case 0: return &descriptor;
    default: return NULL;
    }
}

