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


#define blockSize 2048  // number of audio samples per bit of data
#define delay0 100      // echo delay for bit 0
#define delay1 200      // echo delay for bit 1
#define alpha .40f      // volume of the echo (must be in range [-32768,32767]

int hide_option(const char * hiddenFile, const char * coverFile, const char * outputFile);
int extract_option(const char * stegoFile, const char * outputFile);
static int delay_bit(const int16_t * samples, size_t count);
#endif 
