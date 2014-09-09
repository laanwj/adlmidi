#ifndef H_AUDIOOUT
#define H_AUDIOOUT

#include <stdint.h>

/**
 * Abstract class that can produce a number of stereo samples (2*float) on
 * request.
 */
class AudioGenerator
{
public:
    virtual ~AudioGenerator() = 0;

    virtual void RequestSamples(unsigned long count, float* samples) = 0;
};

/**
 * Abstract class that can receive MIDI events. Some audio backends such as
 * JACK provide MIDI events in-line with other processing, so this is provided
 * from the audio driver.
 */
class MIDIReceiver
{
public:
    virtual ~MIDIReceiver() = 0;

    virtual void PushEvent(uint32_t timestamp, int port, const unsigned char *data, unsigned length) = 0;
};

class UIInterface;

/** Initialize audio system, in paused state,
 * create a buffer of AudioBufferLength seconds.
 */
void InitializeAudio(double AudioBufferLength, unsigned int *sample_rate);
/** Start audio playing from audio generator gen */
void StartAudio(AudioGenerator *gen, MIDIReceiver *midi, UIInterface *ui);
/** Shutdown audio system */
void ShutdownAudio();

#endif
