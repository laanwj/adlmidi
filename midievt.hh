#ifndef H_MIDIEVT
#define H_MIDIEVT

#include "oplsynth/opl.h"

struct OPL3IF
{
    unsigned NumChannels;

    std::vector<OPLEmul*> cards;
    bool fullpan;
private:
    std::vector<unsigned short> ins; // index to adl[], cached, needed by Touch()
    std::vector<unsigned char> pit;  // value poked to B0, cached, needed by NoteOff)(
    std::vector<unsigned char> regBD;

    void Cleanup();
public:
    ~OPL3IF();

    std::vector<char> four_op_category; // 1 = quad-master, 2 = quad-slave, 0 = regular
                                        // 3 = percussion BassDrum
                                        // 4 = percussion Snare
                                        // 5 = percussion Tom
                                        // 6 = percussion Crash cymbal
                                        // 7 = percussion Hihat
                                        // 8 = percussion slave

    void Poke(unsigned card, unsigned index, unsigned value);
    void NoteOff(unsigned c);
    void NoteOn(unsigned c, double hertz); // Hertz range: 0..131071
    void Touch_Real(unsigned c, unsigned volume);
    void Touch(unsigned c, unsigned volume); // Volume maxes at 127*127*127
    void Patch(unsigned c, unsigned i);
    void Pan(unsigned c, unsigned value);
    void Silence();
    void Reset(OPLEmuType emutype, bool fullpan);
    void Update(float *buffer, int length);
};

// Return length of midi event, excluding first byte
static inline unsigned MidiEventLength(unsigned char byte)
{
    switch(byte & 0xF0)
    {
        case 0x80: return 2; // Note off
        case 0x90: return 2; // Note on
        case 0xA0: return 2; // Note touch
        case 0xB0: return 2; // Controller change
        case 0xC0: return 1; // Patch change
        case 0xD0: return 1; // Channel after-touch
        case 0xE0: return 2; // Wheel/pitch bend
        default: break;
    }
    switch(byte)
    {
        case 0xF2: return 2; // Time code quarter frame
        case 0xF3: return 1; // Song select
        // Rest of the events all have 0 data bytes (or variable, in case of sysex)
    }
    return 0;
}

// Process MIDI events and send them to OPL
class MIDIeventhandler
{
    // Persistent settings for each MIDI channel
    struct MIDIchannel
    {
        unsigned short portamento;
        unsigned char bank_lsb, bank_msb;
        unsigned char patch;
        unsigned char volume, expression;
        unsigned char panning, vibrato, sustain;
        double bend, bendsense;
        double vibpos, vibspeed, vibdepth;
        long   vibdelay;
        unsigned char lastlrpn,lastmrpn; bool nrpn;
        struct NoteInfo
        {
            // Current pressure
            unsigned char  vol;
            // Tone selected on noteon:
            short tone;
            // Patch selected on noteon; index to banks[AdlBank][]
            unsigned char midiins;
            // Index to physical adlib data structure, adlins[]
            unsigned short insmeta;
            // List of adlib channels it is currently occupying.
            std::map<unsigned short/*adlchn*/,
                     unsigned short/*ins, inde to adl[]*/
                    > phys;
        };
        typedef std::map<unsigned char,NoteInfo> activenotemap_t;
        typedef activenotemap_t::iterator activenoteiterator;
        activenotemap_t activenotes;

        MIDIchannel();
    };
    std::vector<MIDIchannel> Ch;

    // Additional information about AdLib channels
    struct AdlChannel
    {
        // For collisions
        struct Location
        {
            unsigned short MidCh;
            unsigned char  note;
            bool operator==(const Location&b) const
                { return MidCh==b.MidCh && note==b.note; }
            bool operator< (const Location&b) const
                { return MidCh<b.MidCh || (MidCh==b.MidCh&& note<b.note); }
        };
        struct LocationData
        {
            bool sustained;
            unsigned short ins;  // a copy of that in phys[]
            long kon_time_until_neglible;
            long vibdelay;
        };
        typedef std::map<Location, LocationData> users_t;
        users_t users;

        // If the channel is keyoff'd
        long koff_time_until_neglible;
        // For channel allocation:
        AdlChannel();

        void AddAge(long ms);
    };
    std::vector<AdlChannel> ch;
public:
    OPL3IF opl;
private:
    enum { Upd_Patch  = 0x1,
           Upd_Pan    = 0x2,
           Upd_Volume = 0x4,
           Upd_Pitch  = 0x8,
           Upd_All    = Upd_Pan + Upd_Volume + Upd_Pitch,
           Upd_Off    = 0x20 };

    void NoteUpdate
        (unsigned MidCh,
         MIDIchannel::activenoteiterator i,
         unsigned props_mask,
         int select_adlchn = -1);
    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    long CalculateAdlChannelGoodness
        (unsigned c, unsigned ins, unsigned /*MidCh*/) const;
    // A new note will be played on this channel using this instrument.
    // Kill existing notes on this channel (or don't, if we do arpeggio)
    void PrepareAdlChannelForNewNote(int c, int ins);
    void KillOrEvacuate(
        unsigned from_channel,
        AdlChannel::users_t::iterator j,
        MIDIchannel::activenoteiterator i);
    void KillSustainingNotes(int MidCh = -1, int this_adlchn = -1);
    void SetRPN(unsigned MidCh, unsigned value, bool MSB);
    void UpdatePortamento(unsigned MidCh);
    void NoteUpdate_All(unsigned MidCh, unsigned props_mask);
    void UpdateVibrato(double amount);
    void UpdateArpeggio(double /*amount*/);
    int GetBank(int MidCh);

    // Specific MIDI Event handlers
    void NoteOff(unsigned MidCh, int note);
    void NoteOn(int MidCh, int note, int vol);
    void NoteTouch(int MidCh, int note, int vol);
    void ControllerChange(int MidCh, int ctrlno, int value);
    void PatchChange(int MidCh, int patch);
    void ChannelAfterTouch(int MidCh, int vol);
    void WheelPitchBend(int MidCh, int a, int b);
public:
    void HandleEvent(int chanofs, unsigned char byte, const unsigned char *data, unsigned length);
    void SetNumChannels(int channels);
    void Reset();
    void Tick(double s);
};

#endif
