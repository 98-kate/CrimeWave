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

static int delay_bit(const int16_t * signal, size_t count) {
	int64_t final_delay0 = 0, final_delay1 = 0;
	size_t n; // current sample
	
	for (n = delay1 + 1; n < count; n++) {
   /** FORMULA: y[n] = x[n] - x[n - 1] 
		 This basically filters out low frequences/noise to focus on high-frequency 
		 changes by taking the difference between adjacent audio samples. We do so 
		 because low-frequencies have too many similarities to nearby samples.	   **/ 
		// signal[n] = current audio sound, signal[n - delayX] = what it sounded like
		// [delayX] samples ago before.
		int64_t y = signal[n] - signal[n - 1];
		int64_t delay0_diff   = signal[n - delay0] - signal[n - delay0 - 1];
      int64_t delay1_diff   = signal[n - delay1] - signal[n - delay1 - 1];

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

int extract_option(const char * stegoFile, const char * outputFile) {
	unsigned int channels, sample_rate, extracted_bytes = 0;
	drwav_uint64 total_pcm_frames;
   size_t j,k,l = 0;
	char header_bits[32], file_ext[13] = {0};
	int16_t * modified_audio = drwav_open_file_and_read_pcm_frames_s16(stegoFile, &channels, 
																		&sample_rate, &total_pcm_frames, NULL);
	if (modified_audio == NULL) {
		printf("ERROR: Failed to open %s\n", stegoFile);
		return -1;
	}

	/** Reads in file extension header (blocks 0 - 95)**/
	for (l = 0; l < 96; l++) {
		size_t block_begin = l * blockSize;
		if (block_begin + blockSize > total_pcm_frames) {
			printf("ERROR: Audio file does not have enough samples for the extension header.\n");
			printf("required: %zu, available: %llu\n", (block_begin + blocksize), total_pcm_frames);
			drwav_free(modified_audio, NULL);
			return -1;
		}
		int bit = delay_bit(&modified_audio[block_begin], blockSize);
		size_t byte_idx = l / 8, bit_idx = 7 - (l % 8);
		if (bit == 1) file_ext[byte_idx] |= (1 << bit_idx);
	}

	/** Reads in payload header (96 - 127) **/
	for (j = 0; j < 32; j++) {
		size_t block_begin = (96 + j) * blockSize;
		 if (block_begin + blockSize > total_pcm_frames) {
			 printf("ERROR: Audio file does not have enough samples for header.\n");
			 printf("required: %zu, available: %llu\n", (block_begin + blocksize), total_pcm_frames);
			 drwav_free(modified_audio, NULL);
			 return -1;
		 }
		
		int bit = delay_bit(&modified_audio[block_begin], blockSize);
		header_bits[j] = bit;
		extracted_bytes = (extracted_bytes << 1) | bit; // length of payload
	}

	if (extracted_bytes == 0) {
		printf("ERROR: 0 bytes found in header.\n");
		drwav_free(modified_audio, NULL);
		return -1;
	}

	size_t total_bits    = extracted_bytes * 8; 
   size_t required_bits = 128 + total_bits;

	if (required_bits * blockSize > total_pcm_frames) {
		printf("ERROR: Not enough PCM frames for extraction. File may be corrupted.\n");
		printf("Required: %zu, Available: %llu\n", (required_bits * blockSize), total_pcm_frames);
		drwav_free(modified_audio, NULL);
		return -1;
	}
	
	unsigned char * compressed_payload = malloc(extracted_bytes);
	if (compressed_payload == NULL) {
		printf("ERROR: Memory allocation failed for payload buffer.\n");
		drwav_free(modified_audio, NULL);
		return -1;
	}
	memset(compressed_payload, 0, extracted_bytes);
	
	for (k = 0; k < total_bits; k++) {
		/** Skip header and calculate starting index in PCM array.
			 See: Steganography in Audio (fixed-duration temporal windows)
			 Ex: If blockSize = 2048 && we're seeking payload bit 0 (block_index = 128)
				  block_begin = 128 * 2048 = 262,144 == sample index in memory		    **/
		size_t block_index = 128 + k, block_begin = block_index * blockSize;

		/** extracts 1 bit per audio block of a fixed size  **/
		int bit = delay_bit(&modified_audio[block_begin], blockSize);

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
		 compressed_payload[0] = 1000 0001														**/

	drwav_free(modified_audio, NULL);

	char filename[256];
	if (outputFile != NULL) {
		/** User provided -o flag **/
		snprintf(filename, sizeof(filename), "%s", outputFile);
	} else {
		/** No -o flag, grab file ext used for message when hiding if exists; otherwise default: .bin  **/
		snprintf(filename, sizeof(filename), "hidden_file.%s", (file_ext[0] != '\0') ? file_ext : "bin");
	}

	const char * final_out = filename;	
	FILE * pipe;
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "perl -MCompress::Zlib -e 'undef $/; my $data = <STDIN>; print uncompress($data);' > \"%s\"", final_out);
	
	if ((pipe = popen(cmd, "w")) == NULL) {
		printf("ERROR: Failed to decompress extracted data. \n");
		free(compressed_payload);
		return -1;
	}

	/** fflush() because when reading in the binary data to decompress, w/ "slurping STDIN" in Perl
		 it will block and wait. fwrite() will also hold onto the bytes. 									**/
	fwrite(compressed_payload, 1, extracted_bytes, pipe);
    fflush(pipe);
	pclose(pipe);
	free(compressed_payload);
	printf("SUCCESS: Extracted file saved to %s\n", final_out);
	
	return 0;
}
