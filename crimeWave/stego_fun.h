#ifndef STEGO_FUN_H
#define STEGO_FUN_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "dr_wav.h"
#include <stdint.h>

/** NOTE: When testing && altering block size --
	 You are dealing with a trade-off between capacity, robustness, and imperceptibility.
	 smaller block size = more bits/sec, greater risk of bit flips, shorter audio file required
	 larger  block size = higher extraction accuracy, less capacity 
	 When testing I have used the values: 2048, 100, 200, .40f with OK success. 
	 Embedded data is still somewhat audible in cover file. 											  **/

/**
	The channel capacity R in bits/sec is inversely proportional to the block size.
	R = sample_rate/N_block, if sample_rate = 44,100 Hz && block size = 2048
	that's 21.5 bits/sec. If the block size was 1024, we get 43 bits/sec, but our
	correlation size is halved and makes extraction more fragile.
**/

#define default_blockSize 1024    // number of audio samples per bit of data
#define default_delay0    100     // echo delay for bit 0
#define default_delay1    200     // echo delay for bit 1
#define default_alpha    .30f     // volume of the echo (must be in range [-32768,32767]

#define BLOCKSIZE default_blockSize
#define DELAY0	   default_delay0
#define DELAY1    default_delay1
#define ALPHA     default_alpha

#define PARAMETER 128 // [blocksize = 32] + [delay0 = 32] + [delay1 = 32] + [alpha = 32]
#define EXT 		 96 // file extension -- 12 bytes
#define SIZE 		 32 // payload byte count

typedef struct {
	size_t blockSize;
	int delay0;
	int delay1;
	float alpha;
} Parameters;

int hide_option(const char * hiddenFile, const char * coverFile, const char * outputFile, Parameters p);
int extract_option(const char * stegoFile, const char * outputFile, Parameters p);
#endif 
