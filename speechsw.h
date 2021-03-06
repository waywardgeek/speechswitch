// This interface provides a light weight C interface for talking to the
// supported voice engines.

#include <stdbool.h>
#include <stdint.h>

#define SW_API_VERSION 1

typedef enum {
  SW_UTF8,
  SW_ANSI
} swEncoding;

typedef enum {
  SW_PUNCT_NONE = 0,
  SW_PUNCT_SOME = 1,
  SW_PUNCT_MOST = 2,
  SW_PUNCT_ALL = 3
} swPunctuationLevel;

struct swEngineSt;

typedef struct swEngineSt *swEngine;

typedef unsigned char uchar;

// If a speech callback returns false, it will terminate the current speech
// synthesis operation.  If the user calls swCancel, then cancel will be set on
// the next callback.  Cancellation means the speech buffers should be
// cleared/flushed as soon as possible.  This function is called with 0 samples
// to indicate that speech synthesis is complete.
typedef bool (*swCallback)(swEngine engine, int16_t *samples, uint32_t numSamples,
    bool cancel, void *callbackContext);

// These functions start/stop engines and synthesize speech.

// List available engines.
char **swListEngines(const char *libDirectory, uint32_t *numEngines);
// Create and initialize a new swEngine object, and connect to the speech engine.
swEngine swStart(const char *libDirectory, const char *engineName,
    swCallback callback, void *callbackContext);
// Shut down the speech engine, and free the swEngine object.
void swStop(swEngine engine);
// Synthesize speech samples.  Synthesized samples will be passed to the 
// callback function passed to swStart.  This function blocks until speech
// synthesis is complete.
bool swSpeak(swEngine engine, const char *text, bool isUTF8);
// Synthesize speech samples to speak a single character.  Synthesized samples
// will be passed to the callback function passed to swStart.
// This function blocks until speech synthesis is complete.
bool swSpeakChar(swEngine engine, const char *utf8Char, size_t bytes);

// These functions control speech synthesis parameters.

// Interrupt speech while being synthesized.
void swCancel(swEngine engine);
// Returns true if swCancel has been called since the last call to swSpeak.
bool swSpeechCanceled(swEngine engine);
// Enable/disable using Sonic to set pitch.
void swEnableSonicPitch(swEngine engine, bool enable);
// Enable/disable using Sonic to set speed.
void swEnableSonicSpeed(swEngine engine, bool enable);
// Return true of Sonic is currently used to adjust pitch.
bool swSonicUsedForPitch(swEngine engine);
// Return true of Sonic is currently used to adjust speed.
bool swSonicUsedForSpeed(swEngine engine);
// Get the sample rate in Hertz.
uint32_t swGetSampleRate(swEngine engine);
// Get a list of supported voices.  The caller can call swFreeStringList to free
// them.
char **swListVoices(swEngine engine, uint32_t *numVoices);
// List available variations on voices.
char **swGetVariants(swEngine engine, uint32_t *numVariants);
// Return the native encoding of the engine.
swEncoding swGetEncoding(swEngine engine);
// Select a voice by it's identifier
bool swSetVoice(swEngine engine, const char *voice);
// Select a voice variant by it's identifier
bool swSetVariant(swEngine engine, const char *variant);
// Set the pitch.  0 means default, -100 is min pitch, and 100 is max pitch.
bool swSetPitch(swEngine engine, float pitch);
// Set the punctuation level: none, some, most, or all.
bool swSetPunctuation(swEngine engine, swPunctuationLevel level);
// Set the speech speed.  Speed is from -100.0 to 100.0, and 0 is the default.
bool swSetSpeed(swEngine engine, float speed);
// Enable or disable ssml support.
bool swSetSSML(swEngine engine, bool enable);
// Return the protocol version, Currently 1 for all engines.
uint32_t swGetVersion(swEngine engine);
