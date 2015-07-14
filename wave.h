/* Sonic library
   Copyright 2010
   Bill Cox
   This file is part of the Sonic Library.

   This file is licensed under the Apache 2.0 license. */

/* Support for reading and writing wave files. */

typedef struct WaveFileStruct *WaveFile;

swWaveFile swOpenInputWaveFile(char *fileName, int *sampleRate, int *numChannels);
swWaveFile swOpenOutputWaveFile(char *fileName, int sampleRate, int numChannels);
int swCloseWaveFile(swWaveFile file);
int swReadFromWaveFile(swWaveFile file, short *buffer, int maxSamples);
int swWriteToWaveFile(swWaveFile file, short *buffer, int numSamples);