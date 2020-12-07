/* Stdio server for engines.

This file was written by me, Bill Cox, in 2011, and placed into the public domain.
Feel free to use this file in commercial projects or any other use you wish.

To simplify supporting engines in various languages and compile formats, such as
32-bit vs 64-bit, TTS engines talk to speech-switch through stdin and stdout.
There are just a few simple commands.

These should mostly be pretty self-explanitory, with the exception of speak.
The expectation is that the server is single threaded, and so it will read
characters from the socket until a newline is read, and then execute the
command.  The speak command takes text until a line is read with just '.'.  At
that point, it synthesizes the speech, and every so often writes a line of
encoded samples in hex.  After writing each line, the server should check to see
if there is a command waiting, and if so, is it 'cancel'.  If cancelled,
synthesis should stop.  When synthesis ends, a line with "done" should be
printed.

*/

//#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>

#include "engine.h"

#define MAX_LINE_LENGTH (1 << 12)
#define MAX_TEXT_LENGTH (1 << 16)

typedef unsigned char uchar;

static uchar line[MAX_LINE_LENGTH*2];
static uchar word[MAX_LINE_LENGTH*2];
static uchar *linePos;
static uchar *speechBuffer;
static int speechBufferSize;
static uchar *textBuffer;
static int textBufferSize;
static bool useANSI = false;

#ifdef DEBUG

#include <sys/time.h>

// Subtract two time values.
static int timeDiff(int start, int stop)
{
    // Perform the carry for the later subtraction by updating y.
    if(start <= stop) {
        return stop - start;
    }
    return 1000000 + stop - start;
}

// Get the time in microseconds.
static int getTime(void)
{
    struct timeval t;

    gettimeofday(&t, NULL);
    return t.tv_usec;
}

// Just used for debugging
static void LOG(char *format, ...)
{
    char buffer[MAX_TEXT_LENGTH];
    va_list ap;
    FILE *file;

    va_start(ap, format);
    vsnprintf(buffer, MAX_TEXT_LENGTH - 1, (char *)format, ap);
    va_end(ap);
    buffer[MAX_TEXT_LENGTH - 1] = '\0';
    file = fopen("/tmp/server.log", "a");
    fprintf(file, "%6d: %s", getTime(), buffer);
    fclose(file);
}

#else

void LOG(char *format, ...) {}
//static int timeDiff(int start, int stop) {return 0;}
//static int getTime(void) {return 0;}

#endif

#ifdef WIN32
#define strcasecmp stricmp
#endif

// Switch to ANSI rather than UTF-8.
void swSwitchToANSI(void)
{
    useANSI = true;
}

/* Return the length of the UTF-8 character pointed to by p.  Check that the
   encoding seems valid. We do the full check as defined on Wikipedia because
   so many applications, likely including commercial TTS engines, leave security
   holes open through UTF-8 encoding attacks.  This routine has been extensively
   tested by comparing it's response to that of iconv.  See checkutf8.c. */
static int findLengthAndValidate(uchar *p, bool *valid)
{
    int length, expectedLength, bits;
    unsigned long unicodeCharacter;
    uchar c = (uchar)*p;

    *valid = true;
    if((c & 0x80) == 0) {
        // It's ASCII 
        if(c < ' ') {
            // It's a control character - remove it. 
            *valid = false;
        }
        return 1;
    }
    c <<= 1;
    expectedLength = 1;
    while(c & 0x80) {
        expectedLength++;
        c <<= 1;
    }
    unicodeCharacter = c >> expectedLength;
    bits = 7 - expectedLength;
    if(expectedLength > 4 || expectedLength == 1) {
        // No unicode values are coded for more than 4 bytes 
        *valid = false;
    }
    if(expectedLength == 1 || (expectedLength == 2 && unicodeCharacter <= 1)) {
        // We could have coded this as ASCII 
        *valid = false;
    }
    length = 1;
    c = *++p;
    while((c & 0xc0) == 0x80) {
        unicodeCharacter = (unicodeCharacter << 6) | (c & 0x3f);
        bits += 6;
        length++;
        c = *++p;
    }
    if(length != expectedLength || unicodeCharacter > 0x10ffff ||
        (unicodeCharacter >= 0xd800 && unicodeCharacter <= 0xdfff)) {
        /* Unicode only defines characters up to 0x10ffff, and excludes values
           0xd800 through 0xdfff */
        *valid = false;
    }
    /* Check to see if we could have encoded the character in the next smaller
       number of bits, in which case it's invalid. */
    if(unicodeCharacter >> (bits - 5) == 0) {
        *valid = false;
    }
    return length;
}

// Make sure that only valid UTF-8 characters are in the line, and that all
// control characters are gone.
static void validateLine(void)
{
    uchar *p = line;
    uchar *q = line;
    int length;
    bool valid;

    while(*p != '\0') {
        if(useANSI) {
            length = 1;
            valid = *p >= ' ';
        } else {
            length = findLengthAndValidate(p, &valid);
        }
        if(valid) {
            while(length--) {
                *q++ = *p++;
            }
        } else {
            p += length;
        }
    }
    *q = '\0';
}

// Read a line.  If it's longer than some outragiously long ammount, truncate it. 
static bool readLineRaw(void)
{
    int c = getchar();
    int pos = 0;

    if(c == EOF) {
        return false;
    }
    while(c != '\n' && pos < MAX_LINE_LENGTH - 2) {
        line[pos++] = c;
        c = getchar();
        if(c == EOF) {
            return false;
        }
    }
    line[pos] = '\0';
    return true;
}

// Read a line and validate it, removing control characters and invalid UTF-8 characters.
static bool readLine(void)
{
    do {
        if(!readLineRaw()) {
            return false;
        }
        validateLine();
    } while(*line == '\0');
    LOG("Read %s\n", line);
    return true;
}

// Write a formatted string to the client.
static void writeClient(
    char *format,
    ...)
{
    va_list ap;
    char buf[MAX_TEXT_LENGTH];

    va_start(ap, format);
    vsnprintf(buf, MAX_TEXT_LENGTH - 1, format, ap);
    va_end(ap);
    buf[MAX_TEXT_LENGTH - 1] = '\0';
    LOG("Wrote %s", buf);
    puts(buf);
    fflush(stdout);
}

// Write a string to the client.
static void putClient(char *string)
{
    LOG("Wrote %s", string);
    puts(string);
    fflush(stdout);
}

// Execute the getSampleRate command.
static void execGetSampleRate(void)
{
    int sampleRate = swGetSampleRate();

    writeClient("%d", sampleRate);
}

// Write "true" or "false" to the client based on the boolean value passed.
static void writeBool(bool value)
{
    if(value) {
        putClient("true");
    } else {
        putClient("false");
    }
}

// Just copy a word from the current line position to the word buffer and return
// a pointer to it.  Return NULL if we are at the end.
static char *readWord(void)
{
    uchar *w = word;
    uchar c = *linePos;

    // Skip spaces.
    while(c == ' ') {
        c = *++linePos;
    }
    if(c == '\0') {
        return NULL;
    }
    while(c != '\0' && c != ' ') {
        *w++ = c;
        c = *++linePos;
    }
    *w = '\0';
    return (char *)word;
}

// Execute the getVoices command.
static void execGetVoices(void)
{
    uint32_t numVoices, i;
    char **voices = swGetVoices(&numVoices);

    writeClient("%d", numVoices);
    for(i = 0; i < numVoices; i++) {
        writeClient("%s", voices[i]);
    }
    swFreeStringList(voices, numVoices);
}

// Execute the getVariants command.
static void execGetVoiceVariants(void)
{
    uint32_t numVariants, i;
    char **variants = swGetVoiceVariants(&numVariants);

    if(variants == NULL) {
        writeClient("0");
        return;
    }
    writeClient("%d", numVariants);
    for(i = 0; i < numVariants; i++) {
        writeClient("%s", variants[i]);
    }
    swFreeStringList(variants, numVariants);
}

// Execute the setVoice command.
static void execSetVoice(void)
{
    char *voiceName = (char *)linePos;

    while(*voiceName == ' ') {
        voiceName++;
    }
    writeBool(*voiceName != '\0' && swSetVoice(voiceName));
}

// Execute the setVariant command.
static void execSetVoiceVariant(void)
{
    char *variantName = readWord();

    writeBool(variantName != NULL && swSetVoiceVariant(variantName));
}

// Read a floating point value from the line.
static float readFloat(bool *passed)
{
    char *floatString = readWord();
    char *end;
    float value;

    *passed = true;
    if(floatString == NULL) {
        *passed = false;
        return 0.0f;
    }
    value = strtod(floatString, &end);
    if(*end != '\0') {
        *passed = false;
        return 0.0f;
    }
    return value;
}

// Read a boolean value, either "true" or "false".
static bool readBool(bool *passed)
{
    char *boolString = readWord();

    *passed = true;
    if(boolString == NULL) {
        *passed = false;
        return false;
    }
    if(!strcasecmp(boolString, "true")) {
        return true;
    }
    if(!strcasecmp(boolString, "false")) {
        return false;
    }
    *passed = false;
    return false;
}

// Execute the setPitch command 
static void execSetPitch(void)
{
    bool passed;
    float pitch = readFloat(&passed);

    if(!passed) {
        writeBool(false);
        return;
    }
    writeBool(swSetPitch(pitch));
}

// Execute the setSpeed command 
static void execSetSpeed(void)
{
    bool passed;
    float speed = readFloat(&passed);

    if(!passed) {
        writeBool(false);
        return;
    }
    writeBool(swSetSpeed(speed));
}

// Execute the setPunctuation command 
static void execSetPunctuation(void)
{
    char *levelString = readWord();
    int level = PUNCT_NONE;

    if(levelString == NULL) {
        writeBool(false);
        return;
    }
    if(!strcasecmp(levelString, "none")) {
        level = PUNCT_NONE;
    } else if(!strcasecmp(levelString, "some")) {
        level = PUNCT_SOME;
    } else if(!strcasecmp(levelString, "most")) {
        level = PUNCT_MOST;
    } else if(!strcasecmp(levelString, "all")) {
        level = PUNCT_ALL;
    } else {
        writeBool(false);
        return;
    }
    writeBool(swSetPunctuationLevel(level));
}

// Execute the setSsml command 
static void execSetSsml(void)
{
    bool passed;
    bool value = readBool(&passed);

    if(!passed) {
        writeBool(false);
        return;
    }
    writeBool(swSetSSML(value));
}

// Just read one line at a time into the textBuffer until we see a line with "."
// by itself.  If we see a line starting with two dots, remove the first one.
static bool readText(void)
{
    int pos = 0;
    int length;
    char *lineBuf;

    while(readLine()) {
        if(!strcmp((char *)line, ".")) {
            textBuffer[pos] = '\0';
            return true;
        }
        if(pos > MAX_TEXT_LENGTH) {
            return false;
        }
        lineBuf = (char *)line;
        if(!strncmp((char *)line, "..", 2)) {
            lineBuf++;
        }
        length = strlen(lineBuf);
        if(textBufferSize < pos + length + 1) {
            textBufferSize = (pos + length) << 1;
            textBuffer = (uchar *)realloc(textBuffer, textBufferSize*sizeof(uchar));
        }
        strcpy((char *)textBuffer + pos, lineBuf);
        pos += length;
    }
    return false;
}

/* Execute a speak command.  This will not return until all speech has been synthesized,
   unless processAudio fails to read "true" from the client after sending speech
   samples. */
static bool execSpeak(void)
{
    LOG("entering execSpeak\n");
    if(!readText()) {
        return false;
    }
    LOG("Starting speakText: %s\n", textBuffer);
    writeBool(swSpeakText((char *)textBuffer));
    return true;
}

// Just send a simple summary of the commands.
static void execHelp(void)
{
    putClient(
        "cancel         - Interrupt speech while being synthesized\n"
        "quit/exit      - Close the connection and kill the speech server\n"
        "get samplerate - Show the sample rate in Hertz\n"
        "get voices     - List available voices\n"
        "get variants   - List available variations on voices\n"
        "get encoding   - Either UTF-8 or ANSI (most use UTF-8)\n"
        "help           - This command\n"
        "set voice      - Select a voice by it's identifier\n"
        "set variant    - Select a voice variant by it's identifier\n"
        "set pitch      - Set the pitch\n"
        "set punctuation [none|some|most|all] - Set punctuation level\n"
        "set speed      - Set the speed of speech\n"
        "set ssml [true|false] - Enable or disable ssml support\n"
        "speak          - Enter text on separate lines, ending with \".\" on a line by\n"
        "                 itself.  Synthesized samples will be generated in hexidecimal\n"
        "get version    - Report the speech-switch protocol version, currently 1");
}

// Execute the current command stored in 'line'.  If we read a close command, return false. 
static bool executeCommand(void)
{
    char *command, *key;

    LOG("Executing %s\n", line);
    linePos = line;
    command = readWord();
    if(command == NULL) {
        // Just spaces on the line
        return 1;
    }
    if(!strcasecmp(command, "get")) {
        key = readWord();
        if(!strcasecmp(key, "samplerate")) {
            execGetSampleRate();
        } else if(!strcasecmp(key, "voices")) {
            execGetVoices();
        } else if(!strcasecmp(key, "variants")) {
            execGetVoiceVariants();
        } else if(!strcasecmp(key, "version")) {
            putClient("1");
        } else if(!strcasecmp(key, "encoding")) {
            putClient(useANSI? "ANSI" : "UTF-8");
        } else {
            putClient("Unrecognized command");
        }
    } else if(!strcasecmp(command, "set")) {
        key = readWord();
        if(!strcasecmp(key, "voice")) {
            execSetVoice();
        } else if(!strcasecmp(key, "variant")) {
            execSetVoiceVariant();
        } else if(!strcasecmp(key, "pitch")) {
            execSetPitch();
        } else if(!strcasecmp(key, "speed")) {
            execSetSpeed();
        } else if(!strcasecmp(key, "punctuation")) {
            execSetPunctuation();
        } else if(!strcasecmp(key, "ssml")) {
            execSetSsml();
        } else {
            putClient("Unrecognized command");
        }
    } else if(!strcasecmp(command, "speak")) {
        return execSpeak();
    } else if(!strcasecmp(command, "cancel")) {
        // Nothing required - already finished speech 
    } else if(!strcasecmp(command, "quit") || !strcasecmp(command, "exit")) {
        return false;
    } else if(!strcasecmp(command, "help")) {
        execHelp();
    } else {
        putClient("Unrecognized command");
    }
    return true;
}

// Convert the short data to hex, in big-endian format.
static char *convertToHex(const short *data, int numSamples)
{
    int length = numSamples*4 + 1;
    int i, j;
    char *p, value;
    short sample;

    if(length > speechBufferSize) {
        speechBufferSize = length << 1;
        speechBuffer = (uchar *)realloc(speechBuffer, speechBufferSize*sizeof(char));
    }
    p = (char *)speechBuffer;
    for(i = 0; i < numSamples; i++) {
        sample = data[i];
        for(j = 0; j < 4; j++) {
            value = (sample >> 12) & 0xf;
            *p++ = value <= 9? '0' + value : 'A' + value - 10;
            sample <<= 4;
        }
    }
    *p++ = '\0';
    return (char *)speechBuffer;
}

// Send audio samples in hex to the client.  Return false if the client cancelled. 
bool swProcessAudio(const short *data, int numSamples)
{
    char *hexBuf = convertToHex(data, numSamples);

    putClient(hexBuf);
    if(!readLine()) {
        LOG("Unable to read from client\n");
        return false;
    }
    if(strcasecmp((char *)line, "true")) {
        LOG("Cancelled\n");
        return false;
    }
    return true;
}

/* Run the speech server.  The only argument will be a directory where the
   engine may find it's speech data */
int main(int argc, char **argv)
{
    char *synthDataDir = NULL;

    if(argc == 2) {
        synthDataDir = argv[1];
#ifdef CYGWIN
        /* Cygwin hack: cywin basically complains to death if you don't do this.
           Basically, we have to remove the drive letter and replace with /cygwin */
        if(strlen(synthDataDir) > 2 && synthDataDir[1] == ':') {
            char *buf = (char *)calloc(strlen(synthDataDir) + 10, sizeof(char));
            sprintf(buf, "/cygdrive/%c%s", synthDataDir[0], synthDataDir + 2);
            synthDataDir = buf;
        }
#endif
    } else if(argc != 1) {
        printf("Usage: %s [data_directory]\n", argv[0]);
        return 1;
    }
    if(!swInitializeEngine(synthDataDir)) {
        if(argc == 2) {
            printf("Unable to initialize the TTS engine with data directory %s.\n", argv[1]);
        } else {
            printf("Unable to initialize the TTS engine.\n");
        }
        return 1;
    }
    speechBufferSize = 4096;
    speechBuffer = (uchar *)calloc(speechBufferSize, sizeof(char));
    textBufferSize = 4096;
    textBuffer = (uchar *)calloc(textBufferSize, sizeof(char));
    while(readLine() && executeCommand());
    free(textBuffer);
    free(speechBuffer);
    swCloseEngine();
    return 0;
}