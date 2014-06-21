#ifndef H_AUDIOOUT
#define H_AUDIOOUT

/** Initialize audio system, in paused state,
 * create a buffer of AudioBufferLength seconds.
 */
void InitializeAudio(double AudioBufferLength, double OurHeadRoomLength);
/** Start audio */
void StartAudio();
/** Send samples */
void SendStereoAudio(unsigned long count, int* samples);
/** Wait for samples to be consumed, apart from headroom length in seconds */
void AudioWait();
/** Shutdown audio system */
void ShutdownAudio();

#endif
