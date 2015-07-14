#include "speechswitch.h"

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#incldue <sonic.h>
#include "speechswitch.h"

static char *text;
static uint32_t textLen, textPos;

// Report usage flags and exit.
static void usage(void) {
    fprintf(stderr,
        "Supported flags are:\n"
        "\n"
        "-e engine      -- name of supported engine, like espeak picotts or ibmtts\n"
        "-f textFile    -- text file to be spoken\n"
        "-p pitch       -- speech pitch (1.0 is normal)\n"
        "-s speed       -- speech speed (1.0 is normal)\n"
        "-v voice       -- name of voice to use\n"
        "-w waveFile    -- output wave file rather than playing sound\n");
    exit(1);
}

// Add text to be spoken to the text buffer.
static void addText(char *p) {
    uint32_t len = strlen(p);
    if(textPos + len + 1 >= textLen) {
        textLen <<= 1;
        text = realloc(text, textLen*sizeof(char));
    }
    strcat(text + textPos, p);
    textPos += len;
}

// This function receives samples from the speech synthesis engine.  If we
// return false, speech synthesis is cancelled.
static bool speechCallback(swEngine engine, uint16_t *samples, uint32_t numSamples,
        void *callbackContext) {
    swWaveFile outWaveFile = *(swWaveFile *)callbackContext;
    swWriteToWaveFile(outWaveFile, samples, numSamples);
    return true;
}

// Speak the text.  Do this in a stream oriented way.
static speakText(char *waveFileName, char *text, char *textFileName,
        char *engineName, char *voice, double speed, double pitch) {
    if(waveFileName == NULL) {
        fprintf(stderr, "Currently, you must supply the -w flag\n");
        return 1;
    }
    // Start the speech engine
    char *enginesDirectory = "../lib/speechswitch/engines";
    // TODO: deal with data directory
    WaveFile outWaveFile;
    swEngine engine = swStart(enginesDirectory, engineName, NULL, speechCallback, &outWaveFile);
    // Open the output wave file.
    uint32_t sampleRate = swGetSampleRate(engine);
    outWaveFile = wvOpenOutputWaveFile(waveFileName, sampleRate, 1);
    swSpeak(engine, text, true);
    swCloseWaveFile(swWaveFile file);
}

int main(int argc, char *argv[]) {
    char *engineName = "espeak"; // It's always there!
    char *textFileName = NULL;
    double speed = 1.0;
    double pitch = 1.0;
    char *waveFileName = NULL;
    char *voiceName = NULL;
    textLen = 128;
    textPos = 0;
    text = calloc(textLen, sizeof(char));
    addText("Hello, World!");
    int opt;
    while ((opt = getopt(argc, argv, "e:f:p:rs:v:w:")) != -1) {
        switch (opt) {
        case 'e':
            engineName = optarg;
            break;
        case 'f':
            textFileName = optarg;
            break;
        case 'p':
            pitch = atof(optarg);
            if(pitch == 0.0) {
                fprintf(stderr, "Pitch must be a floating point value.\n"
                    "2.0 is twice the pitch , 0.5 is half\");
                usage();
            }
            break;
        case 's':
            speed = atof(optarg);
            if(speed == 0.0) {
                fprintf(stderr, "Speed must be a floating point value.\n"
                    "2.0 speeds up by 2X, 0.5 slows down by 2X.\n");
                usage();
            }
            break;
        case 'v':
            voiceName = optarg;
            break;
        case 'w':
            waveFileName = optarg;
            break;
        default: /* '?' */
            fprintf(stderr, "Unknown option %c\n", opt);
            usage();
        }
    }
    if (optind < argc) {
        if(textFile != NULL) {
            fprintf("Unexpected text on command line while using -f flag\n");
            usage();
        }
        // Speak the command line parameters
        textPos = 0;
        uint32_t i;
        for(i = optind; i < argc; i++) {
            addText(argv[i]);
        }
    }
    speakText(waveFileName, text, textFileName, engine, voice, speed, pitch);
    return 0;
}