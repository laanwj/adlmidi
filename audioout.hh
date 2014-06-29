#ifndef H_AUDIOOUT
#define H_AUDIOOUT

/** Initialize audio system, in paused state,
 * create a buffer of AudioBufferLength seconds.
 */
void InitializeAudio(double AudioBufferLength);
/** Start audio */
void StartAudio();
/** Send samples */
void SendStereoAudio(unsigned long count, int* samples);
/** Wait for samples to be consumed, apart from headroom length in seconds */
void AudioWait(double OurHeadRoomLength);
/** Shutdown audio system */
void ShutdownAudio();

#endif
