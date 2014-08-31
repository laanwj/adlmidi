#ifndef H_AUDIOOUT
#define H_AUDIOOUT

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

/** Initialize audio system, in paused state,
 * create a buffer of AudioBufferLength seconds.
 */
void InitializeAudio(double AudioBufferLength);
/** Start audio playing from audio generator gen */
void StartAudio(AudioGenerator *gen);
/** Shutdown audio system */
void ShutdownAudio();

#endif
