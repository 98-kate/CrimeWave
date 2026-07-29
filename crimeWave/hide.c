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

int hide_option(const char * hiddenFile, const char * coverFile, const char * outputFile, Parameters p) {
	unsigned char * payloadByteVal = NULL;
	size_t payloadByteCount = 64;
	FILE * check_file;	

	if ((check_file = fopen(hiddenFile, "rb")) == NULL) {
		printf("ERROR: The file \"%s\" you are trying to hide does not exist.\n", hiddenFile);
		return -1;
	}
	fclose(check_file);

	if (strcmp(hiddenFile, "random") == 0) {
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
			for (size_t i = 0; i < payloadByteCount; i++) { 
				payloadByteVal[i] = rand() % 256; 
			} 
		}
	} else { 
		
    FILE * fptr;
	 if ((fptr = fopen(hiddenFile, "rb")) == NULL) {
     printf("ERROR: Couldn't open file: %s\n", hiddenFile);
	  return -1;
	 }

    fseek(fptr, 0, SEEK_END);
    payloadByteCount = ftell(fptr);
    fseek(fptr,0,SEEK_SET);
	 unsigned char * payloadByteVal = malloc(payloadByteCount);
	 fread(payloadByteVal, 1, payloadByteCount, fptr);
    fclose(fptr);
	 // total bits in payload to be hidden (ex: 64 * 8 = 512 bits)
	 size_t payloadBitCount = (payloadByteCount * 8);   
	 // converts payload bytes to 1 bit per array
	 char * payload_bits = malloc(payloadBitCount);   
	 converter(payloadByteVal, payloadByteCount, payload_bits);	

	/** Used to find extension in hiddenFile for extraction **/
	char ext_header[12] = {0}, ext_bits[96] = {0};
	const char * file_ext = strrchr(hiddenFile, '.');
	strncpy(ext_header, file_ext ? file_ext + 1 : "bin", sizeof(ext_header) - 1);
	converter((unsigned char *)ext_header, sizeof(ext_header), ext_bits);
	
	/** converts payloadByteCount into a standard 32-bit unsigned int for easier bit-shifting**/
	char header_bits[32]; 
	unsigned int header_val = (unsigned int)payloadByteCount; 
	for (size_t i = 0; i < 32; i++) {
		header_bits[i] = (header_val >> (31 - i)) & 1;
	}

	char p_bits[PARAMETER] = {0};
	uint32_t blkSz = (uint32_t)p.blockSize, d0 = (uint32_t)p.delay0, d1 = (uint32_t)p.delay1, alphabits;
	memcpy(&alphabits, &p.alpha, sizeof(alphabits));
	for (size_t i = 0; i < 32; i++) {
		p_bits[i] = (blkSz >> (31 - i)) & 1;
		p_bits[32 + i] = (d0 >> (31 - i)) & 1;
		p_bits[64 + i] = (d1 >> (31 - i)) & 1;
		p_bits[96 + i] = (alphabits >> (31 - i)) & 1;
	} 	
	size_t bitstreamLength = PARAMETER + EXT + SIZE + payloadBitCount;
	char * final_stream = malloc(bitstreamLength);
	memcpy(final_stream, p_bits, PARAMETER); 
	memcpy(final_stream + PARAMETER, ext_bits, EXT);
	memcpy(final_stream + PARAMETER + EXT, header_bits, SIZE);
	memcpy(final_stream + PARAMETER + EXT + SIZE, payload_bits, payloadBitCount);
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
	size_t truncate_check = ((size_t)PARAMETER * BLOCKSIZE) + (bitstreamLength - PARAMETER) * p.blockSize;
	if (truncate_check > total_pcm_frames) {
		printf("\nWARNING! Not all of the data was hidden and will be truncated!\n");
	}

   /** BEGIN ECHO HIDING! HOW EXCITING! **/
	size_t bits_embedded = 0, cursor = 0;
	for (size_t z = 0; z < PARAMETER; z++) {
		if (cursor >= total_pcm_frames) break;
		char bit = final_stream[z];
		/** With Gruhl & other's papers, a one bit is represented by a single 
			 delay [d1|d0]. This method is implementing a differential echo scheme **/
	   int dpos = (bit == 1) ? DELAY1 : DELAY0;
	   int dneg = (bit == 1) ? DELAY0 : DELAY1;
		for (size_t y = cursor; y < (cursor + BLOCKSIZE); y++) {
		 if (y >= total_pcm_frames) break;
	      int16_t echo_pos = 0;
	      int16_t echo_neg = 0;
   	   if ((int64_t)y - dpos >= 0) echo_pos = input_audio[y - dpos];
	      if ((int64_t)y - dneg >= 0) echo_neg = input_audio[y - dneg];
	      float yN = (float)input_audio[y] + (ALPHA * (float)echo_pos) - (ALPHA * (float)echo_neg);
	      if (yN > 32767.0f)  yN = 32767.0f;
	      if (yN < -32768.0f) yN = -32768.0f;
   	   yN_audio[y] = (int16_t)yN;
    	}
	  cursor += BLOCKSIZE;
     bits_embedded++;
   }

  for (size_t z = PARAMETER; z < bitstreamLength; z++) {
     if (cursor >= total_pcm_frames) break;
     char bit = final_stream[z];
     int dpos = (bit == 1) ? p.delay1 : p.delay0;
     int dneg = (bit == 1) ? p.delay0 : p.delay1;
     for (size_t y = cursor; y < (cursor + p.blockSize); y++) {
       if (y >= total_pcm_frames) break;
        int16_t echo_pos = 0;
        int16_t echo_neg = 0;
        if ((int64_t)y - dpos >= 0) echo_pos = input_audio[y - dpos];
        if ((int64_t)y - dneg >= 0) echo_neg = input_audio[y - dneg];
        float yN = (float)input_audio[y] + (p.alpha * (float)echo_pos) - (p.alpha * (float)echo_neg);
        if (yN > 32767.0f)  yN = 32767.0f;
        if (yN < -32768.0f) yN = -32768.0f;
        yN_audio[y] = (int16_t)yN;
     }
      cursor += p.blockSize;
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

	const char * out_path = (outputFile != NULL) ? outputFile : "output_file.wav";
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
}
