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

#include "fraction"

#include "adldata.hh"
#include "config.hh"
#include "ui.hh"
#include "midievt.hh"

unsigned AdlBank    = 0;
unsigned NumFourOps = 7;
unsigned NumCards   = 2;
bool HighTremoloMode   = false;
bool HighVibratoMode   = false;
bool AdlPercussionMode = false;
volatile sig_atomic_t QuitFlag = false;
unsigned SkipForward = 0;
bool DoingInstrumentTesting = false;
bool QuitWithoutLooping = false;
bool WritePCMfile = false;
bool ScaleModulators = false;

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
        else if(std::memcmp(HeaderBuf, "MUS\1x1A", 4) == 0)
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
        if(byte == 0xF7 || byte == 0xF0) // Ignore SysEx
        {
            unsigned length = ReadVarLen(tk);
            //std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
            CurrentPosition.track[tk].ptr += length;
            UI.PrintLn("SysEx %02X: %u bytes", byte, length/*, data.c_str()*/);
            return;
        }
        if(byte == 0xFF)
        {
            // Special event FF
            unsigned char evtype = TrackData[tk][CurrentPosition.track[tk].ptr++];
            unsigned long length = ReadVarLen(tk);
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
        // Any normal event (80..EF)
        if(byte < 0x80)
          { byte = CurrentPosition.track[tk].status | 0x80;
            CurrentPosition.track[tk].ptr--; }
        if(byte == 0xF3) { CurrentPosition.track[tk].ptr += 1; return; }
        if(byte == 0xF2) { CurrentPosition.track[tk].ptr += 2; return; }
        /*UI.PrintLn("@%X Track %u: %02X %02X",
            CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte,
            TrackData[tk][CurrentPosition.track[tk].ptr]);*/
        unsigned MidCh = byte & 0x0F, EvType = byte >> 4;
        MidCh += current_device[tk];

        CurrentPosition.track[tk].status = byte;
        switch(EvType)
        {
            case 0x8: // Note off
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                // Note-off volume is unused
                CurrentPosition.track[tk].ptr++;
                evh->NoteOff(MidCh, note);
                break;
            }
            case 0x9: // Note on
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                evh->NoteOn(MidCh, note, vol);
                CurrentPosition.began  = true;
                break;
            }
            case 0xA: // Note touch
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                evh->NoteTouch(MidCh, note, vol);
                break;
            }
            case 0xB: // Controller change
            {
                int ctrlno = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  value = TrackData[tk][CurrentPosition.track[tk].ptr++];
                evh->ControllerChange(MidCh, ctrlno, value);
                break;
            }
            case 0xC: // Patch change
                evh->PatchChange(MidCh, TrackData[tk][CurrentPosition.track[tk].ptr++]);
                break;
            case 0xD: // Channel after-touch
            {
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                evh->ChannelAfterTouch(MidCh, vol);
                break;
            }
            case 0xE: // Wheel/pitch bend
            {
                int a = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int b = TrackData[tk][CurrentPosition.track[tk].ptr++];
                evh->WheelPitchBend(MidCh, a, b);
                break;
            }
        }
    }

};

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

#ifdef __WIN32__
namespace WindowsAudio
{
  static const unsigned BUFFER_COUNT = 16;
  static const unsigned BUFFER_SIZE  = 8192;
  static HWAVEOUT hWaveOut;
  static WAVEHDR headers[BUFFER_COUNT];
  static volatile unsigned buf_read=0, buf_write=0;

  static void CALLBACK Callback(HWAVEOUT,UINT msg,DWORD,DWORD,DWORD)
  {
      if(msg == WOM_DONE)
      {
          buf_read = (buf_read+1) % BUFFER_COUNT;
      }
  }
  static void Open(const int rate, const int channels, const int bits)
  {
      WAVEFORMATEX wformat;
      MMRESULT result;

      //fill waveformatex
      memset(&wformat, 0, sizeof(wformat));
      wformat.nChannels       = channels;
      wformat.nSamplesPerSec  = rate;
      wformat.wFormatTag      = WAVE_FORMAT_PCM;
      wformat.wBitsPerSample  = bits;
      wformat.nBlockAlign     = wformat.nChannels * (wformat.wBitsPerSample >> 3);
      wformat.nAvgBytesPerSec = wformat.nSamplesPerSec * wformat.nBlockAlign;

      //open sound device
      //WAVE_MAPPER always points to the default wave device on the system
      result = waveOutOpen
      (
        &hWaveOut,WAVE_MAPPER,&wformat,
        (DWORD_PTR)Callback,0,CALLBACK_FUNCTION
      );
      if(result == WAVERR_BADFORMAT)
      {
          fprintf(stderr, "ao_win32: format not supported\n");
          return;
      }
      if(result != MMSYSERR_NOERROR)
      {
          fprintf(stderr, "ao_win32: unable to open wave mapper device\n");
          return;
      }
      char* buffer = new char[BUFFER_COUNT*BUFFER_SIZE];
      std::memset(headers,0,sizeof(headers));
      std::memset(buffer, 0,BUFFER_COUNT*BUFFER_SIZE);
      for(unsigned a=0; a<BUFFER_COUNT; ++a)
          headers[a].lpData = buffer + a*BUFFER_SIZE;
  }
  static void Close()
  {
      waveOutReset(hWaveOut);
      waveOutClose(hWaveOut);
  }
  static void Write(const unsigned char* Buf, unsigned len)
  {
      static std::vector<unsigned char> cache;
      size_t cache_reduction = 0;
      if(0&&len < BUFFER_SIZE&&cache.size()+len<=BUFFER_SIZE)
      {
          cache.insert(cache.end(), Buf, Buf+len);
          Buf = &cache[0];
          len = cache.size();
          if(len < BUFFER_SIZE/2)
              return;
          cache_reduction = cache.size();
      }

      while(len > 0)
      {
          unsigned buf_next = (buf_write+1) % BUFFER_COUNT;
          WAVEHDR* Work = &headers[buf_write];
          while(buf_next == buf_read)
          {
              /*UI.Color(4);
              UI.GotoXY(60,-5+5); fprintf(stderr, "waits\r"); UI.x=0; std::fflush(stderr);
              UI.Color(4);
              UI.GotoXY(60,-4+5); fprintf(stderr, "r%u w%u n%u\r",buf_read,buf_write,buf_next); UI.x=0; std::fflush(stderr);
              */
              /* Wait until at least one of the buffers is free */
              Sleep(0);
              /*UI.Color(2);
              UI.GotoXY(60,-3+5); fprintf(stderr, "wait completed\r"); UI.x=0; std::fflush(stderr);*/
          }

          unsigned npending = (buf_write + BUFFER_COUNT - buf_read) % BUFFER_COUNT;
          static unsigned counter=0, lo=0;
          if(!counter-- || npending < lo) { lo = npending; counter=100; }

          if(!DoingInstrumentTesting)
          {
              if(UI.maxy >= 5) {
                  UI.Color(9);
                  UI.GotoXY(70,-5+6); fprintf(stderr, "%3u bufs\r", (unsigned)npending); UI.x=0; std::fflush(stderr);
                  UI.GotoXY(71,-4+6); fprintf(stderr, "lo:%3u\r", lo); UI.x=0; }
          }

          //unprepare the header if it is prepared
          if(Work->dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(hWaveOut, Work, sizeof(WAVEHDR));
          unsigned x = BUFFER_SIZE; if(x > len) x = len;
          std::memcpy(Work->lpData, Buf, x);
          Buf += x; len -= x;
          //prepare the header and write to device
          Work->dwBufferLength = x;
          {int err=waveOutPrepareHeader(hWaveOut, Work, sizeof(WAVEHDR));
           if(err != MMSYSERR_NOERROR) fprintf(stderr, "waveOutPrepareHeader: %d\n", err);}
          {int err=waveOutWrite(hWaveOut, Work, sizeof(WAVEHDR));
           if(err != MMSYSERR_NOERROR) fprintf(stderr, "waveOutWrite: %d\n", err);}
          buf_write = buf_next;
          //if(npending>=BUFFER_COUNT-2)
          //    buf_read=(buf_read+1)%BUFFER_COUNT; // Simulate a read
      }
      if(cache_reduction)
          cache.erase(cache.begin(), cache.begin()+cache_reduction);
  }
}
#else
static std::deque<short> AudioBuffer;
static MutexType AudioBuffer_lock;
static void SDL_AudioCallback(void*, Uint8* stream, int len)
{
    SDL_LockAudio();
    short* target = (short*) stream;
    AudioBuffer_lock.Lock();
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len/2; // number of shorts
    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();
    for(unsigned a=0; a<ate; ++a)
        target[a] = AudioBuffer[a];
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    //fprintf(stderr, " - remain %u\n", (unsigned) AudioBuffer.size()/2);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}
#endif // WIN32

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


static void SendStereoAudio(unsigned long count, int* samples)
{
    if(count % 2 == 1)
    {
        // An uneven number of samples? To avoid complicating matters,
        // just ignore the odd sample.
        count   -= 1;
        samples += 1;
    }
    if(!count) return;

    // Attempt to filter out the DC component. However, avoid doing
    // sudden changes to the offset, for it can be audible.
    double average[2]={0,0};
    for(unsigned w=0; w<2; ++w)
        for(unsigned long p = 0; p < count; ++p)
            average[w] += samples[p*2+w];
    static float prev_avg_flt[2] = {0,0};
    float average_flt[2] =
    {
        prev_avg_flt[0] = (prev_avg_flt[0] + average[0]*0.04/double(count)) / 1.04,
        prev_avg_flt[1] = (prev_avg_flt[1] + average[1]*0.04/double(count)) / 1.04
    };
    // Figure out the amplitude of both channels
    if(!DoingInstrumentTesting)
    {
        static unsigned amplitude_display_counter = 0;
        if(!amplitude_display_counter--)
        {
            amplitude_display_counter = (PCM_RATE / count) / 24;
            double amp[2]={0,0};
            for(unsigned w=0; w<2; ++w)
            {
                average[w] /= double(count);
                for(unsigned long p = 0; p < count; ++p)
                    amp[w] += std::fabs(samples[p*2+w] - average[w]);
                amp[w] /= double(count);
                // Turn into logarithmic scale
                const double dB = std::log(amp[w]<1 ? 1 : amp[w]) * 4.328085123;
                const double maxdB = 3*16; // = 3 * log2(65536)
                amp[w] = dB/maxdB;
            }
            UI.IllustrateVolumes(amp[0], amp[1]);
        }
    }

    //static unsigned counter = 0; if(++counter < 8000)  return;

#if defined(__WIN32__) && 0
    // Cheat on dosbox recording: easier on the cpu load.
   {count*=2;
    std::vector<short> AudioBuffer(count);
    for(unsigned long p = 0; p < count; ++p)
        AudioBuffer[p] = samples[p];
    WindowsAudio::Write( (const unsigned char*) &AudioBuffer[0], count*2);
    return;}
#endif

    // Convert input to float format
    std::vector<float> dry[2];
    for(unsigned w=0; w<2; ++w)
    {
        dry[w].resize(count);
        float a = average_flt[w];
        for(unsigned long p = 0; p < count; ++p)
        {
            int   s = samples[p*2+w];
            dry[w][p] = (s - a) * double(0.3/32768.0);
        }
        // ^  Note: ftree-vectorize causes an error in this loop on g++-4.4.5
        reverb_data.chan[w].input_fifo.insert(
        reverb_data.chan[w].input_fifo.end(),
            dry[w].begin(), dry[w].end());
    }
    // Reverbify it
    for(unsigned w=0; w<2; ++w)
        reverb_data.chan[w].Process(count);

    // Convert to signed 16-bit int format and put to playback queue
#ifdef __WIN32__
    std::vector<short> AudioBuffer(count*2);
    const size_t pos = 0;
#else
    AudioBuffer_lock.Lock();
    size_t pos = AudioBuffer.size();
    AudioBuffer.resize(pos + count*2);
#endif
    for(unsigned long p = 0; p < count; ++p)
        for(unsigned w=0; w<2; ++w)
        {
            float out = ((1 - reverb_data.wetonly) * dry[w][p] +
                .5 * (reverb_data.chan[0].out[w][p]
                    + reverb_data.chan[1].out[w][p])) * 32768.0f
                 + average_flt[w];
            AudioBuffer[pos+p*2+w] =
                out<-32768.f ? -32768 :
                out>32767.f ?  32767 : out;
        }
    if(WritePCMfile)
    {
        /* HACK: Cheat on DOSBox recording: Record audio separately on Windows. */
        static FILE* fp = 0;
        if(!fp)
        {
            fp = fopen("adlmidi.wav", "wb");
            if(fp)
            {
                FourChars Bufs[] = {
                    "RIFF", (0x24u),  // RIFF type, file length - 8
                    "WAVE",           // WAVE file
                    "fmt ", (0x10u),  // fmt subchunk, which is 16 bytes:
                      "\1\0\2\0",     // PCM (1) & stereo (2)
                      (48000u    ), // sampling rate
                      (48000u*2*2), // byte rate
                      "\2\0\20\0",    // block align & bits per sample
                    "data", (0x00u)  //  data subchunk, which is so far 0 bytes.
                };
                for(unsigned c=0; c<sizeof(Bufs)/sizeof(*Bufs); ++c)
                    std::fwrite(Bufs[c].ret, 1, 4, fp);
            }
        }

        // Using a loop, because our data type is a deque, and
        // the data might not be contiguously stored in memory.
        for(unsigned long p = 0; p < 2*count; ++p)
            std::fwrite(&AudioBuffer[pos+p], 1, 2, fp);

        /* Update the WAV header */
        if(true)
        {
            long pos = std::ftell(fp);
            if(pos != -1)
            {
                long datasize = pos - 0x2C;
                if(std::fseek(fp, 4,  SEEK_SET) == 0) // Patch the RIFF length
                    std::fwrite( FourChars(0x24u+datasize).ret, 1,4, fp);
                if(std::fseek(fp, 40, SEEK_SET) == 0) // Patch the data length
                    std::fwrite( FourChars(datasize).ret, 1,4, fp);
                std::fseek(fp, pos, SEEK_SET);
            }
        }

        std::fflush(fp);

        //if(std::ftell(fp) >= 48000*4*10*60)
        //    raise(SIGINT);
    }
#ifndef __WIN32__
    AudioBuffer_lock.Unlock();
#else
    if(!WritePCMfile)
        WindowsAudio::Write( (const unsigned char*) &AudioBuffer[0], 2*AudioBuffer.size());
#endif
}

static void TidyupAndExit(int)
{
    UI.ShowCursor();
    UI.Color(7);
    std::fflush(stderr);
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
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
static int ParseCommandLine(char *cmdline, char **argv)
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

#ifndef __WIN32__
    // Set up SDL
    static SDL_AudioSpec spec, obtained;
    spec.freq     = PCM_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = spec.freq * AudioBufferLength;
    spec.callback = SDL_AudioCallback;
    if(SDL_OpenAudio(&spec, &obtained) < 0)
    {
        std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        //return 1;
    }
    if(spec.samples != obtained.samples)
        std::fprintf(stderr, "Wanted (samples=%u,rate=%u,channels=%u); obtained (samples=%u,rate=%u,channels=%u)\n",
            spec.samples,    spec.freq,    spec.channels,
            obtained.samples,obtained.freq,obtained.channels);
#endif

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
        if(bankno == -1)
        {
            bankno = 0;
            DoingInstrumentTesting = true;
        }
        AdlBank = bankno;
        if(AdlBank >= NumBanks)
        {
            std::fprintf(stderr, "bank number may only be 0..%u.\n", NumBanks-1);
            return 0;
        }
        std::printf("FM instrument bank %u selected.\n", AdlBank);
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
            DoingInstrumentTesting ? 2
          : (n_fourop[0] >= n_total[0]*7/8) ? NumCards * 6
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

    MIDIeventhandler evh;
    MIDIplay player(&evh);
    if(!player.LoadMIDI(argv[1]))
        return 2;

    if(n_fourop[0] >= n_total[0]*15/16 && NumFourOps == 0)
    {
        std::fprintf(stderr,
            "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
            "       The results (silence + much cpu load) would be probably\n"
            "       not what you want, therefore ignoring the request.\n");
        return 0;
    }

    const double mindelay = 1 / (double)PCM_RATE;
    const double maxdelay = MaxSamplesAtTime / (double)PCM_RATE;

#ifdef __WIN32
    WindowsAudio::Open(PCM_RATE, 2, 16);
#else
    SDL_PauseAudio(0);
#endif

    for(double delay=0; !QuitFlag; )
    {
        const double eat_delay = delay < maxdelay ? delay : maxdelay;
        delay -= eat_delay;

        static double carry = 0.0;
        carry += PCM_RATE * eat_delay;
        const unsigned long n_samples = (unsigned) carry;
        carry -= n_samples;

        if(SkipForward > 0)
            SkipForward -= 1;
        else
        {
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
            }

            //fprintf(stderr, "Enter: %u (%.2f ms)\n", (unsigned)AudioBuffer.size(),
            //    AudioBuffer.size() * .5e3 / obtained.freq);
        #ifndef __WIN32__
            while(AudioBuffer.size() > obtained.samples + (obtained.freq*2) * OurHeadRoomLength)
            {
                if(!WritePCMfile)
                    SDL_Delay(1); // std::min(10.0, 1e3 * eat_delay) );
                else
                {
                    AudioBuffer_lock.Lock();
                    AudioBuffer.clear();
                    AudioBuffer_lock.Unlock();
                }
            }
        #else
            //Sleep(1e3 * eat_delay);
        #endif
            //fprintf(stderr, "Exit: %u\n", (unsigned)AudioBuffer.size());
        }
        double nextdelay = player.Tick(eat_delay, mindelay);
        UI.ShowCursor();

        delay = nextdelay;
    }

#ifdef __WIN32__
    WindowsAudio::Close();
#else
    SDL_CloseAudio();
#endif

    return 0;
}
