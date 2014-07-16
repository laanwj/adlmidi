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
#include "fraction"
#include "midievt.hh"
#include "parseargs.hh"
#include "ui.hh"

volatile sig_atomic_t QuitFlag = false;
unsigned SkipForward = 0;

// Read midi file and play back events
class MIDIplay
{
    std::map<std::string, unsigned> devices;
    // Information about each track
    struct Position
    {
        bool began;
        double wait;
        struct TrackInfo
        {
            size_t ptr;
            long   delay;
            int    status;

            TrackInfo(): ptr(0), delay(0), status(0) { }
        };
        std::vector<TrackInfo> track;

        Position(): began(false), wait(0.0), track() { }
    } CurrentPosition, LoopBeginPosition;

    std::vector< std::vector<unsigned char> > TrackData;
    std::map<unsigned/*track*/, unsigned/*channel begin index*/> current_device;
public:
    explicit MIDIplay(MIDIeventhandler *evh):
        evh(evh)
    {
    }

    fraction<long> InvDeltaTicks, Tempo;
    bool loopStart, loopEnd;
    MIDIeventhandler *evh;
public:
    static unsigned long ReadBEInt(const void* buffer, unsigned nbytes)
    {
        unsigned long result=0;
        const unsigned char* data = (const unsigned char*) buffer;
        for(unsigned n=0; n<nbytes; ++n)
            result = (result << 8) + data[n];
        return result;
    }
    unsigned long ReadVarLen(unsigned tk)
    {
        unsigned long result = 0;
        for(;;)
        {
            unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
            result = (result << 7) + (byte & 0x7F);
            if(!(byte & 0x80)) break;
        }
        return result;
    }

    bool LoadMIDI(const std::string& filename)
    {
        std::FILE* fp = std::fopen(filename.c_str(), "rb");
        if(!fp) { std::perror(filename.c_str()); return false; }
        char HeaderBuf[4+4+2+2+2]="";
    riffskip:;
        std::fread(HeaderBuf, 1, 4+4+2+2+2, fp);
        if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
            { std::fseek(fp, 6, SEEK_CUR); goto riffskip; }
        size_t DeltaTicks=192, TrackCount=1;

        bool is_GMF = false, is_MUS = false, is_IMF = false;
        std::vector<unsigned char> MUS_instrumentList;

        if(std::memcmp(HeaderBuf, "GMF\1", 4) == 0)
        {
            // GMD/MUS files (ScummVM)
            std::fseek(fp, 7-(4+4+2+2+2), SEEK_CUR);
            is_GMF = true;
        }
        else if(std::memcmp(HeaderBuf, "MUS\x1A", 4) == 0)
        {
            // MUS/DMX files (Doom)
            std::fseek(fp, 8-(4+4+2+2+2), SEEK_CUR);
            is_MUS = true;
            unsigned start = std::fgetc(fp); start += (std::fgetc(fp) << 8);
            std::fseek(fp, -8+start, SEEK_CUR);
        }
        else
        {
            // Try parsing as an IMF file
           {
            unsigned end = (unsigned char)HeaderBuf[0] + 256*(unsigned char)HeaderBuf[1];
            if(!end || (end & 3)) goto not_imf;

            long backup_pos = std::ftell(fp);
            unsigned sum1 = 0, sum2 = 0;
            std::fseek(fp, 2, SEEK_SET);
            for(unsigned n=0; n<42; ++n)
            {
                unsigned value1 = std::fgetc(fp); value1 += std::fgetc(fp) << 8; sum1 += value1;
                unsigned value2 = std::fgetc(fp); value2 += std::fgetc(fp) << 8; sum2 += value2;
            }
            std::fseek(fp, backup_pos, SEEK_SET);
            if(sum1 > sum2)
            {
                is_IMF = true;
                DeltaTicks = 1;
            }
           }

            if(!is_IMF)
            {
            not_imf:
                if(std::memcmp(HeaderBuf, "MThd\0\0\0\6", 8) != 0)
                { InvFmt:
                    std::fclose(fp);
                    std::fprintf(stderr, "%s: Invalid format\n", filename.c_str());
                    return false;
                }
                /*size_t  Fmt =*/ ReadBEInt(HeaderBuf+8,  2);
                TrackCount = ReadBEInt(HeaderBuf+10, 2);
                DeltaTicks = ReadBEInt(HeaderBuf+12, 2);
            }
        }
        TrackData.resize(TrackCount);
        CurrentPosition.track.resize(TrackCount);
        InvDeltaTicks = fraction<long>(1, 1000000l * DeltaTicks);
        //Tempo       = 1000000l * InvDeltaTicks;
        Tempo         = fraction<long>(1,            DeltaTicks);

        static const unsigned char EndTag[4] = {0xFF,0x2F,0x00,0x00};

        for(size_t tk = 0; tk < TrackCount; ++tk)
        {
            // Read track header
            size_t TrackLength;
            if(is_IMF)
            {
                //std::fprintf(stderr, "Reading IMF file...\n");
                long end = (unsigned char)HeaderBuf[0] + 256*(unsigned char)HeaderBuf[1];

                unsigned IMF_tempo = 1428;
                static const unsigned char imf_tempo[] = {0xFF,0x51,0x4,
                    (unsigned char)(IMF_tempo>>24),
                    (unsigned char)(IMF_tempo>>16),
                    (unsigned char)(IMF_tempo>>8),
                    (unsigned char)(IMF_tempo)};
                TrackData[tk].insert(TrackData[tk].end(), imf_tempo, imf_tempo + sizeof(imf_tempo));
                TrackData[tk].push_back(0x00);

                std::fseek(fp, 2, SEEK_SET);
                while(std::ftell(fp) < end)
                {
                    unsigned char special_event_buf[5];
                    special_event_buf[0] = 0xFF;
                    special_event_buf[1] = 0xE3;
                    special_event_buf[2] = 0x02;
                    special_event_buf[3] = std::fgetc(fp); // port index
                    special_event_buf[4] = std::fgetc(fp); // port value
                    unsigned delay = std::fgetc(fp); delay += 256 * std::fgetc(fp);

                    //if(special_event_buf[3] <= 8) continue;

                    //fprintf(stderr, "Put %02X <- %02X, plus %04X delay\n", special_event_buf[3],special_event_buf[4], delay);

                    TrackData[tk].insert(TrackData[tk].end(), special_event_buf, special_event_buf+5);
                    //if(delay>>21) TrackData[tk].push_back( 0x80 | ((delay>>21) & 0x7F ) );
                    if(delay>>14) TrackData[tk].push_back( 0x80 | ((delay>>14) & 0x7F ) );
                    if(delay>> 7) TrackData[tk].push_back( 0x80 | ((delay>> 7) & 0x7F ) );
                    TrackData[tk].push_back( ((delay>>0) & 0x7F ) );
                }
                TrackData[tk].insert(TrackData[tk].end(), EndTag+0, EndTag+4);
                CurrentPosition.track[tk].delay = 0;
                CurrentPosition.began = true;
                //std::fprintf(stderr, "Done reading IMF file\n");
            }
            else
            {
                if(is_GMF)
                {
                    long pos = std::ftell(fp);
                    std::fseek(fp, 0, SEEK_END);
                    TrackLength = ftell(fp) - pos;
                    std::fseek(fp, pos, SEEK_SET);
                }
                else if(is_MUS)
                {
                    long pos = std::ftell(fp);
                    std::fseek(fp, 4, SEEK_SET);
                    TrackLength = std::fgetc(fp); TrackLength += (std::fgetc(fp) << 8);
                    std::fseek(fp, pos, SEEK_SET);
                }
                else
                {
                    std::fread(HeaderBuf, 1, 8, fp);
                    if(std::memcmp(HeaderBuf, "MTrk", 4) != 0) goto InvFmt;
                    TrackLength = ReadBEInt(HeaderBuf+4, 4);
                }
                // Read track data
                TrackData[tk].resize(TrackLength);
                std::fread(&TrackData[tk][0], 1, TrackLength, fp);
                if(is_GMF || is_MUS)
                {
                    TrackData[tk].insert(TrackData[tk].end(), EndTag+0, EndTag+4);
                }
                // Read next event time
                CurrentPosition.track[tk].delay = ReadVarLen(tk);
            }
        }
        loopStart = true;

        evh->Reset();
        devices.clear();
        ChooseDevice("");

        return true;
    }

    /* Periodic tick handler.
     *   Input: s           = seconds since last call
     *   Input: granularity = don't expect intervals smaller than this, in seconds
     *   Output: desired number of seconds until next call
     */
    double Tick(double s, double granularity)
    {
        if(CurrentPosition.began) CurrentPosition.wait -= s;
        while(CurrentPosition.wait <= granularity * 0.5)
        {
            //std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
            ProcessEvents();
        }
        evh->Tick(s);
        return CurrentPosition.wait;
    }

    unsigned ChooseDevice(const std::string& name)
    {
        std::map<std::string, unsigned>::iterator i = devices.find(name);
        if(i != devices.end()) return i->second;
        size_t n = devices.size() * 16;
        devices.insert( std::make_pair(name, n) );
        evh->SetNumChannels(n + 16);
        return n;
    }

private:

    void ProcessEvents()
    {
        loopEnd = false;
        const size_t TrackCount = TrackData.size();
        const Position RowBeginPosition ( CurrentPosition );
        for(size_t tk = 0; tk < TrackCount; ++tk)
        {
            if(CurrentPosition.track[tk].status >= 0
            && CurrentPosition.track[tk].delay <= 0)
            {
                // Handle event
                HandleEvent(tk);
                // Read next event time (unless the track just ended)
                if(CurrentPosition.track[tk].ptr >= TrackData[tk].size())
                    CurrentPosition.track[tk].status = -1;
                if(CurrentPosition.track[tk].status >= 0)
                    CurrentPosition.track[tk].delay += ReadVarLen(tk);
            }
        }
        // Find shortest delay from all track
        long shortest = -1;
        for(size_t tk=0; tk<TrackCount; ++tk)
            if(CurrentPosition.track[tk].status >= 0
            && (shortest == -1
               || CurrentPosition.track[tk].delay < shortest))
            {
                shortest = CurrentPosition.track[tk].delay;
            }
        //if(shortest > 0) UI.PrintLn("shortest: %ld", shortest);

        // Schedule the next playevent to be processed after that delay
        for(size_t tk=0; tk<TrackCount; ++tk)
            CurrentPosition.track[tk].delay -= shortest;

        fraction<long> t = shortest * Tempo;
        if(CurrentPosition.began) CurrentPosition.wait += t.valuel();

        //if(shortest > 0) UI.PrintLn("Delay %ld (%g)", shortest, (double)t.valuel());

        /*
        if(CurrentPosition.track[0].ptr > 8119) loopEnd = true;
        // ^HACK: CHRONO TRIGGER LOOP
        */

        if(loopStart)
        {
            LoopBeginPosition = RowBeginPosition;
            loopStart = false;
        }
        if(shortest < 0 || loopEnd)
        {
            // Loop if song end reached
            loopEnd         = false;
            CurrentPosition = LoopBeginPosition;
            shortest        = 0;
            if(QuitWithoutLooping)
            {
                QuitFlag = true;
                //^ HACK: QUIT WITHOUT LOOPING
            }
        }
    }

    void HandleEvent(size_t tk)
    {
        unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
        unsigned length;
        if(byte == 0xF7 || byte == 0xF0) // SysEx
        {
            length = ReadVarLen(tk);
        }
        else if(byte == 0xFF)
        {
            // Special event FF
            unsigned char evtype = TrackData[tk][CurrentPosition.track[tk].ptr++];
            length = ReadVarLen(tk);
            std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
            CurrentPosition.track[tk].ptr += length;
            if(evtype == 0x2F) { CurrentPosition.track[tk].status = -1; return; }
            if(evtype == 0x51) { Tempo = InvDeltaTicks * fraction<long>( (long) ReadBEInt(data.data(), data.size())); return; }
            if(evtype == 6 && data == "loopStart") loopStart = true;
            if(evtype == 6 && data == "loopEnd"  ) loopEnd   = true;
            if(evtype == 9) current_device[tk] = ChooseDevice(data);
            if(evtype >= 1 && evtype <= 6)
                UI.PrintLn("Meta %d: %s", evtype, data.c_str());

            if(evtype == 0xE3) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
            {
                unsigned char i = data[0], v = data[1];
                if( (i&0xF0) == 0xC0 ) v |= 0x30;
                //fprintf(stderr, "OPL poke %02X, %02X\n", i,v);
                evh->opl.Poke(0, i,v);
            }
            return;
        }
        else
        {
            // Any normal event (80..EF)
            if(byte < 0x80) // Running status
              { byte = CurrentPosition.track[tk].status | 0x80;
                CurrentPosition.track[tk].ptr--; }
            length = MidiEventLength(byte);
        }
        evh->HandleEvent(current_device[tk], byte, &TrackData[tk][CurrentPosition.track[tk].ptr], length);
        if((byte&0xF0) == 0x90) // First note
            CurrentPosition.began  = true;
        CurrentPosition.track[tk].ptr += length;
        /*UI.PrintLn("@%X Track %u: %02X %02X",
            CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte,
            TrackData[tk][CurrentPosition.track[tk].ptr]);*/
        CurrentPosition.track[tk].status = byte;
    }
};

static void TidyupAndExit(int)
{
    UI.Cleanup();
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
        "(C) -- https://github.com/laanwj/adlmidi\n");
    std::fflush(stdout);
    UI.Color(7); std::fflush(stderr);

    signal(SIGTERM, TidyupAndExit);
    signal(SIGINT, TidyupAndExit);

    InitializeAudio(AudioBufferLength, OurHeadRoomLength);
    int rv = ParseArguments(argc, argv);
    if(rv >= 0)
        return rv;

    MIDIeventhandler evh;
    MIDIplay player(&evh);
    if(!player.LoadMIDI(argv[1]))
        return 2;

    StartAudio();
    for(int delay=0; !QuitFlag; )
    {
        const int n_samples = std::min(delay, (int)MaxSamplesAtTime);

        if(SkipForward > 0)
            SkipForward -= 1;
        else
        {
	    float buffer[MaxSamplesAtTime*2] = {};
	    evh.opl.Update(buffer, n_samples);
	    SendStereoAudio(n_samples, buffer);
            AudioWait();
        }
        int nextdelay = ceil(player.Tick(
		    n_samples / (double)PCM_RATE,
		    1.0 / (double)PCM_RATE) * (double)PCM_RATE);
        UI.ShowCursor();

        delay = nextdelay;
    }

    ShutdownAudio();
    UI.Cleanup();

    return 0;
}
