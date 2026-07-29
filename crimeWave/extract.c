#include "dr_wav.h"
#include "stego_fun.h"

/** KEY EXAMPLE:
	 If you're in a massive, empty, quiet building and you shout a phrase: you hear your
	 voice once, then in a millisecond or so you hear an echo off of a wall. The echo
	 is a slightly quieter copy, delayed in time. The same applies here. **/ 

/** This block of code is responsible for reading bits out of a single
	 block via high-pass autocorrelation. See: Echo Hiding by Daniel Gruhl, A. Lu, and W. Bender
	 which describes a signal x[n] is multiplied by a delayed version of itself x[n-d]. 
	 Though, this caused unwanted bit flips due to the delays causing false positives. 
	 Had to implement differential detection; comparing two delays against eachother.
	 See: Time-Spread Echo Hiding **/

static int delay_bit(const int16_t * signal, size_t count, Parameters p) {
	int64_t final_delay0 = 0, final_delay1 = 0;

   for (size_t n = p.delay1 + 1; n < count; n++) {
   /** FORMULA: y[n] = x[n] - x[n - 1] 
       This basically filters out low frequences/noise to focus on high-frequency 
       changes by taking the difference between adjacent audio samples. We do so 
       because low-frequencies have too many similarities to nearby samples.     **/
      // signal[n] = current audio sound, signal[n - delayX] = what it sounded like
      // [delayX] samples ago before.
      int64_t y = signal[n] - signal[n - 1];
      int64_t delay0_diff   = signal[n - p.delay0] - signal[n - p.delay0 - 1];
      int64_t delay1_diff   = signal[n - p.delay1] - signal[n - p.delay1 - 1];

      final_delay0 += y * delay0_diff;
      final_delay1 += y * delay1_diff;
	 }
   /** Final check for differential -- matching waveforms == high correlation score
       See: Cross-Correlation Accumulation
      Ex: y = +100, delay0_diff = +100 --> y * delay0_diff = +10000 == match found
          y = +100, delay1_diff = -50  --> y * delay1_diff = -5000  == uncorrelated
          whichever offset (final_delayX) has the larger total correlation num
          identifies the hidden bit [0|1]   **/
   return (final_delay1 > final_delay0) ? 1 : 0;
}

int extract_option(const char * stegoFile, const char * outputFile, Parameters p) {
   unsigned int channels, sample_rate;
	uint32_t extracted_bytes = 0;
   drwav_uint64 total_pcm_frames;
   char file_ext[13] = {0};
   int16_t * modified_audio = drwav_open_file_and_read_pcm_frames_s16(stegoFile, &channels,
                                                      &sample_rate, &total_pcm_frames, NULL);

	if (modified_audio == NULL) {
		printf("ERROR: Failed to open %s\n", stegoFile);
		return -1;
	}

	drwav_uint64 min_frames = (drwav_uint64)PARAMETER * BLOCKSIZE;
	if (total_pcm_frames < min_frames) {
		printf("ERROR: %s is too short to contain a valid parameter header.\n", stegoFile);
      printf("Required minimum frames: %llu Available: %llu\n", min_frames, total_pcm_frames);
      drwav_free(modified_audio, NULL);
      return -1;
	}

	/** Decoding the parameter header to recover the echo parameters if the user
       entered any so they don't have to retype them when extracting.         **/
	Parameters header_p = {BLOCKSIZE, DELAY0, DELAY1, ALPHA};
	uint32_t blocksize = 0, delay0 = 0, delay1 = 0, alpha_bits = 0;

	for (size_t l = 0; l < PARAMETER; l++) {
		size_t block_begin = l * BLOCKSIZE;
		if (block_begin + BLOCKSIZE > total_pcm_frames) {
			printf("ERROR: Audio file does not have enough samples for the parameter header.\n");
			drwav_free(modified_audio, NULL);
			return -1;
		}
		int bit = delay_bit(&modified_audio[block_begin], BLOCKSIZE, header_p);
		if (l < 32) { 
			if (bit) blocksize |= (1U << (31 - l)); 
		} else if (l < 64) { 
			if (bit) delay0 |= (1U << (31 - (l - 32))); 
		} else if (l < 96) { 
			if (bit) delay1 |= (1U << (31 - (l - 64))); 
		} else { 
			if (bit) alpha_bits |= (1U << (31 - (l - 96))); 
		}
	}

	Parameters real_p;
	real_p.blockSize = (size_t)blocksize;
	real_p.delay0 = (int)delay0;
	real_p.delay1 = (int)delay1;
	memcpy(&real_p.alpha, &alpha_bits, sizeof(real_p.alpha));

	if (real_p.blockSize == 0 || real_p.blockSize > total_pcm_frames || real_p.delay0 < 0 || real_p.delay1 < 0) {
	 printf("ERROR: Decoded parameter header is invalid (blockSize=%zu delay0=%d delay1=%d).\n", 
					real_p.blockSize, real_p.delay0, real_p.delay1);
		drwav_free(modified_audio, NULL);
		return -1;
	}
	// printf("DEBUG: Parameters entered during embedding: blockSize=%zu delay0=%d delay1=%d alpha=%f\n", 
	// real_p.blockSize, real_p.delay0, real_p.delay1, real_p.alpha);

	size_t phase2_start = (size_t)PARAMETER * BLOCKSIZE;
	for (size_t l = 0; l < EXT; l++) {
		size_t block_begin = phase2_start + l * real_p.blockSize;
		if (block_begin + real_p.blockSize > total_pcm_frames) {
        printf("ERROR: Audio file does not have enough samples for the extension header.\n");
        printf("Required: %zu, Available: %llu\n", (block_begin + real_p.blockSize), total_pcm_frames);
        drwav_free(modified_audio, NULL);
        return -1;
      }
		int bit = delay_bit(&modified_audio[block_begin], real_p.blockSize, real_p);
		size_t byte_idx = l / 8, bit_idx = 7 - (l % 8);
		if (bit == 1) {
			file_ext[byte_idx] |= (1 << bit_idx);
		}
	}

	/** Payload size header. **/
	for (size_t l = 0; l < SIZE; l++) {
		size_t block_begin = phase2_start + (EXT + l) * real_p.blockSize;
		if (block_begin + real_p.blockSize > total_pcm_frames) {
			printf("ERROR: Audio file does not have enough samples for payload size header.\n");
			drwav_free(modified_audio, NULL);
			return -1;
		}
		int bit = delay_bit(&modified_audio[block_begin], real_p.blockSize, real_p);
		size_t bit_idx = 31 - l;
		if (bit == 1) {
			extracted_bytes |= (1U << bit_idx);
		}
	}
   printf("DEBUG: Extracted raw header size value = %u (0x%08X)\n", extracted_bytes, extracted_bytes);
	size_t total_bits 	 = (size_t)extracted_bytes * 8;
	size_t payload_start = phase2_start + (size_t)(EXT + SIZE) * real_p.blockSize;
	size_t required_end  = payload_start + total_bits * real_p.blockSize;

	if (required_end > total_pcm_frames) {
		printf("ERROR: Not enough PCM frames for extraction. File may be corrupted.\n");
		printf("Required: %zu, Available: %llu\n", required_end, total_pcm_frames);
		drwav_free(modified_audio, NULL);
		return -1;
	}

	/** extract payload bits **/
	unsigned char * compressed_payload = malloc(extracted_bytes);
   if (compressed_payload == NULL) {
      printf("ERROR: Memory allocation failed for payload buffer.\n");
      drwav_free(modified_audio, NULL);
      return -1;
   }

	memset(compressed_payload, 0, extracted_bytes);
   for (size_t k = 0; k < total_bits; k++) {
      size_t block_begin = payload_start + k * real_p.blockSize;
      /** extracts 1 bit per audio block of a fixed size  **/
      int bit = delay_bit(&modified_audio[block_begin], real_p.blockSize, real_p);
      /** Need to find byte to store bit in & its bit position.
          byte_idx finds which byte. 
          Example: byte_idx = 0 -> bits 0 - 7, byte_idx = 1 -> bits 8 to 15
          bit_idx works from MSB to LSB.
          Example: bit 0 (k = 0) == 7 - (0 % 8) -> bit 7
                   bit 1 (k = 1) == 7 - (1 % 8) -> bit 6 ..and so on **/
      size_t byte_idx = k / 8, bit_idx = 7 - (k % 8);
      if (bit == 1) {
         /** compressed_payload was initialized to 0, so bitwise OR to pack **/
         compressed_payload[byte_idx] |= (1 << bit_idx);
      }
   }

   /** VISUAL EXAMPLE OF BYTE REASSEMBLY 
       Say we are extracting 0x81 (1000 0001) from k = 0 through k = 7
       so 8 continuous blocks. 
       k = 0, block_index = 32, delay_bit() = 1, byte_idx = 0, bit_idx = 7
       byte in compressed_payload[0] looks like: 1000 0000 
       k = 1 - 6, compressed_payload[0] will stay the same because all are 0's...
       k = 7 where block_index = 39, delay_bit() = 1, byte_idx = 0, bit_idx = 0
       compressed_payload[0] = 1000 0001                                         **/

	drwav_free(modified_audio, NULL);
	char filename[256], cmd[512];
	if (outputFile != NULL) {   /** User provided -o flag **/
		snprintf(filename, sizeof(filename), "%s", outputFile);
	} else { /** No -o flag -- grab file ext used when hiding, otherwise default to .bin **/
		snprintf(filename, sizeof(filename), "hidden_file.%s", (file_ext[0] != '\0') ? file_ext : "bin");
	}

	const char * final_out = filename;
	FILE * fout; 
	if ((fout = fopen(final_out, "wb")) == NULL) {
     printf("ERROR: Failed to write to file: %s\n", final_out);
	  free(compressed_payload);
     return -1;
	}
	fwrite(compressed_payload, 1, extracted_bytes, fout);
	fclose(fout);
	free(compressed_payload);

	printf("SUCCESS: Decompressed %u bytes and saved to %s\n", extracted_bytes, final_out);
	return 0;
}
