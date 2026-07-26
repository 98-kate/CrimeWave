#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "stego_fun.h"

/** Helper function to make things a bit less ugly.. **/
void converter(const unsigned char * bytes, size_t count, char * bits) {
	for (size_t i = 0; i < count; i++) {
		for (int j = 0; j < 8; j++) {
			bits[i * 8 + j] = (bytes[i] >> (7 - j)) & 1;
		}
	}
}

int hide_option(const char * hiddenFile, const char * coverFile, const char * outputFile) {
	unsigned char * payloadByteVal = NULL;
	size_t payloadByteCount = 64;
	FILE * check_file;	

	if ((check_file = fopen(hiddenFile, "rb")) == NULL) {
		printf("ERROR: The file \"%s\" you are trying to hide does not exist.\n", hiddenFile);
		return -1;
	}
	fclose(check_file);
	
	if (strcmp(hiddenFile, "random") == 0) {
		size_t i,j = 0;
		FILE * urand;
		payloadByteVal = malloc(payloadByteCount);

		/** NOTE: I wanted more entropy, so if this is ran with Linux, it will
			 read the raw bytes from the kernel via /dev/urandom. If that fails, it
			 falls back onto srand(). /urandom is used because it doesn't block.  **/
		if ((urand = fopen("/dev/urandom", "rb"))) {
			fread(payloadByteVal, 1, payloadByteCount, urand);
			fclose(urand);
		} else {
			srand((unsigned int)time(NULL));
			for (i = 0; i < payloadByteCount; i++) { 
				payloadByteVal[i] = rand() % 256; 
			} 
		}
	} else { 
		/** WELCOME TO PERL HELL! 
			 snprintf formats text into a buffer where cmd (destination buffer), sizeof(cmd) is max num of 
			 bytes to be written.
			 * "-Mcompress::Zlib loads Perl's compress() module w/ Zlib. -e means "execute from the cmd line"
			 *	undef $/; is the input record separator & also reads the entire file at once
			 *	<> global input stream to read data sequentially from files (similar to cat, sed, or awk) 	**/
		FILE * pipe;
		char cmd[512], temp_buffer[512];	
		snprintf(cmd, sizeof(cmd), "perl -MCompress::Zlib -e \"undef $/; print compress(<>);\" \"%s\"", hiddenFile);

		/** Added for Windows compatibility due to compilation issues with Perl **/
		#ifdef _WIN32
			pipe = _popen(cmd, "rb");
		#else
			pipe = popen(cmd, "r");
		#endif

		if (pipe == NULL) {
			printf("ERROR: File failed to compress.\n");
			return -1;
		}

		/** begin with assuming the compressed file will fit in 1KB **/
		size_t capacity = 1024, bytesRead = 0;
		payloadByteCount = 0, payloadByteVal = malloc(capacity);
		
		while ((bytesRead = fread(temp_buffer, 1, sizeof(temp_buffer), pipe)) > 0) {
			if (payloadByteCount + bytesRead > capacity) {
				capacity *= 2; // doubles allocated capacity size (1KB -> 2KB -> ...) IF NEEDED
				payloadByteVal = realloc(payloadByteVal, capacity);
			}
			memcpy(payloadByteVal + payloadByteCount, temp_buffer, bytesRead);
			payloadByteCount += bytesRead;
		}

	#ifdef _WIN32
		_pclose(pipe);
	#else
		 pclose(pipe);
	#endif
	} // END OF IF STATEMENT FOR FILE OR RANDOM DATA TO EMBED

	/** BEGIN CONVERTING FILE PAYLOAD TO BITS, BUILD THE HEADER, & COMBINE BOTH INTO ONE MSG BUFFER **/
	/** NOTE: Because the header is a 32-bit unsigned integer (header_val), we can support files
		 up to 2^32-1 bytes, or about 4GB.																				**/
	size_t i = 0;
	size_t payloadBitCount = (payloadByteCount * 8);  // total bits in payload to be hidden (ex: 64 * 8 = 512 bits)
	char * payload_bits = malloc(payloadBitCount);    // converts payload bytes to 1 bit per array
	converter(payloadByteVal, payloadByteCount, payload_bits);	

	/** Used to find extension in hiddenFile for extraction **/
	char ext_header[12] = {0}, ext_bits[96] = {0};
	const char * file_ext = strrchr(hiddenFile, '.');
	strncpy(ext_header, file_ext ? file_ext + 1 : "bin", sizeof(ext_header) - 1);
	converter((unsigned char *)ext_header, sizeof(ext_header), ext_bits);
	
	/** bit representation of payloadByteCount for decoder later **/
	char header_bits[32]; 

	/** converts payloadByteCount into a standard 32-bit unsigned int for easier bit-shifting**/
	unsigned int header_val = (unsigned int)payloadByteCount; 
	for (i = 0; i < 32; i++) {
		header_bits[i] = (header_val >> (31 - i)) & 1;
	}

   /** LENGTH of the combined array of header + payload **/
	size_t bitstreamLength = 128 + payloadBitCount;
	/** final bitstream consisting of header + payload_bits (sequence to embed) **/
	char * final_stream = malloc(bitstreamLength); 		
	memcpy(final_stream, ext_bits, 96);
	memcpy(final_stream + 96, header_bits, 32);
	memcpy(final_stream + 128, payload_bits, payloadBitCount);
	free(payload_bits);

	/** EXAMPLE FOR VAR NAMES TO HELP: 
		 payloadByteCount - size_t - count of raw bytes (ex: 64B)
		 payloadBitCount  - size_t - count of payload bits (ex: 512 (64 * 8))
		 bitstreamLength  - size_t - count of total message bits (ex: 544)
		 payloadByteVal   - buffer - raw byte values [0, 255]
		 payload_bits     - buffer - expanded payload bits [0,1]
		 header_bits      - buffer - header length bits [0,1]
		 final_stream     - buffer - combined header + payload bits				**/

 /** BEGIN USING DR_WAV TO READ COVER AUDIO **/
	unsigned int channels, sample_rate;
	drwav_uint64 total_pcm_frames;
	int16_t * input_audio = drwav_open_file_and_read_pcm_frames_s16(coverFile, &channels, 
																	&sample_rate, &total_pcm_frames, NULL);		
	
	int16_t * yN_audio = malloc(total_pcm_frames * sizeof(int16_t));

	if (input_audio == NULL) {
		printf("ERROR: Failed to parse cover file: %s \n", coverFile);
		free(payloadByteVal), free(final_stream);
		return -1;
	}

	if (channels != 1) {
		printf("ERROR: This program uses Mono (1-channel) files only.\n");
		drwav_free(input_audio, NULL);	
		free(payloadByteVal), free(final_stream);
		return -1;
	}

	if (yN_audio == NULL) {
		printf("ERROR: No memory to further allocate to the yN audio file.\n");
		drwav_free(input_audio, NULL);	
		free(payloadByteVal), free(final_stream);
		return -1;
	}

	memcpy(yN_audio, input_audio, total_pcm_frames * sizeof(int16_t));
	size_t truncate_check = bitstreamLength * blockSize;

	if (truncate_check > total_pcm_frames) {
		printf("\nWARNING! Not all of the data was hidden and will be truncated!\n");
	}

/** BEGIN ECHO HIDING! HOW EXCITING! **/
	size_t bits_embedded = 0;
	for (size_t z = 0; z < bitstreamLength; z++) {
   	 size_t block_begin = z * blockSize;
    	 char bit = final_stream[z]; // info bit being embedded into block z
    	 if (block_begin >= total_pcm_frames) break;

		 /** With Gruhl & other's papers, a one bit is represented by a single 
			  delay [d1|d0]. This method is implementing a differential echo scheme **/
	    int dpos = (bit == 1) ? delay1 : delay0;
	    int dneg = (bit == 1) ? delay0 : delay1;

	    for (size_t y = block_begin; y < (block_begin + blockSize); y++) {
	        if (y >= total_pcm_frames) break;

	        int16_t echo_pos = 0;
	        int16_t echo_neg = 0;
			  // x[n-dpos], x[n-dneg]
   	     if ((int64_t)y - dpos >= 0) echo_pos = input_audio[y - dpos];
	        if ((int64_t)y - dneg >= 0) echo_neg = input_audio[y - dneg];
		// alpha is echo attenuation gain (0 < a << 1)
	        float yN = (float)input_audio[y] + (alpha * (float)echo_pos) - (alpha * (float)echo_neg);

	        if (yN > 32767.0f)  yN = 32767.0f;
	        if (yN < -32768.0f) yN = -32768.0f;

   	     yN_audio[y] = (int16_t)yN;
    	}
    bits_embedded++;
}

	if (bits_embedded < bitstreamLength) {
    	printf("\tOnly %zu of %zu bits were embedded.\n", bits_embedded, bitstreamLength);
	}

/** BEGIN WRITING OUT **/

	drwav_data_format format;
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = 1;
	format.sampleRate = sample_rate;
	format.bitsPerSample = 16;

	const char * out_path = (outputFile != NULL) ? outputFile : "yN_output_file.wav";
	drwav pWav;
	if (drwav_init_file_write(&pWav, out_path, &format, NULL)) {
		drwav_write_pcm_frames(&pWav, total_pcm_frames, yN_audio);
		drwav_uninit(&pWav);
		printf("SUCCESS: %zu bits embedded into the cover file: %s !\n", bits_embedded, out_path);
	} else {
		printf("ERROR: Failed to write the output file: %s\n", out_path);
	}

	drwav_free(input_audio, NULL);
	free(yN_audio);
	free(payloadByteVal);
	free(final_stream);
	
	return 0;
}
