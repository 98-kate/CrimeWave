
#include "dr_wav.h"
#include "stego_fun.h"

void helpMenu() {
    printf("\nUsage: [OPTION] [FLAG] [FILE] ...\n");
    printf("\nOptions: \n");
    printf("%-20s %-30s\n", " -hide", "embed a secret message file into a cover file");
    printf("%-20s %-30s\n", " -extract", "extract a hidden message from a stego file");
    printf("%-20s %-30s\n", " -h", "give this help list");
    printf("\n\nHiding flags: \n");
    printf("%-20s %-30s\n", " -m <file|random>", "path to the secret msg file to hide OR 'random' to hide random bits");
    printf("%-20s %-30s\n", " -c <cover file>",  "path to the cover file");
    printf("%-20s %-30s\n", " -o <stego file>",  "path for the output stego file [optional]");
    printf("\n\nExtraction flags: \n");
    printf("%-20s %-30s\n", " -s <stego file>", "path to the stego file containing hidden data");
    printf("%-20s %-30s\n", " -o <message file>", "path for the extracted message file [optional]");
	 printf("\n\nEcho Parameters [optional]: \n");
	 printf("%-20s %-30s\n", " -b <block size>", "number of audio samples per bit of data -- must be in powers of 2");
	 printf("%-20s %-30s\n", " -d0 <bit 0 delay>", "echo delay for bit 0");	
	 printf("%-20s %-30s\n", " -d1 <bit 1 delay>", "echo delay for bit 1");
	 printf("%-20s %-30s\n", " -a <alpha|amplitude>", "volume of the echo -- must be in range [-32768,32767]");

    printf("\nExamples: \n");
    printf("\t./crimeWave -hide -m <message.ext> -c <cover_file.wav> [-o <stego_file.wav>]\n");
	 printf("\t./crimeWave -hide -m <message.ext> -c <cover_file.wav> [-o <stego_file.wav>] [-b 1024 -d0 100 -d1 200 -a 0.15]\n");
    printf("\t./crimeWave -extract -s <stego_file.wav> [-o <message.ext>]\n");
    printf("\t./crimeWave -extract -s <stego_file.wav> [-o <message.ext>] [-b 1024 -d0 100 -d1 200 -a 0.15]\n");
}

int main(int argc, char * argv[]) {
	char * modified_audio = NULL, * outputFile = NULL;

   Parameters parameters = {
      .blockSize = default_blockSize,
      .delay0 = default_delay0,
      .delay1 = default_delay1,
      .alpha  = default_alpha
   };

	static struct option options[] = {
		{"hide", 	  required_argument, 0, 'm'},	
		{"extract",	  required_argument, 0, 's'},
		{"cover",	  required_argument, 0, 'c'},
		{"out",		  required_argument, 0, 'o'},
		{"blocksize", required_argument, 0, 'b'},
		{"d0",		  required_argument, 0, '0'},
		{"d1",	 	  required_argument, 0, '1'},
		{"alpha",     required_argument, 0, 'a'},
		{"help",      required_argument, 0, 'h'},
		{0, 0, 0, 0}};

	int opt, opt_idx = 0;
	optind = 2;
	if (argc < 2) {
		helpMenu();
		return -1;
	}

	if (strcmp(argv[1], "-hide") == 0) {
		char * hiddenFile = NULL, * coverFile = NULL, * outputFile = NULL;
		while ((opt = getopt_long_only(argc, argv, ":m:c:o:b:0:1:a:h:", options, &opt_idx)) != -1) {
			switch (opt) {
				case 'm': hiddenFile = optarg; break;
				case 'c': coverFile  = optarg; break;
				case 'o': outputFile = optarg; break;
				case 'b': parameters.blockSize = (size_t)atoi(optarg); break;
				case '0': parameters.delay0 = atoi(optarg); break;
				case '1': parameters.delay1 = atoi(optarg); break;
				case 'a': parameters.alpha  = (float)atof(optarg); break;
				case 'h': helpMenu(); return 0;
				case ':':
						if (optopt == 0 && opt_idx >= 0) {
							printf("\nERROR: Flag '-%s' requires a filename argument provided.\n", options[opt_idx].name);
                  } else { 
							printf("\nERROR: Flag '-%c' requires an argument provided.\n", optopt);
						}
						helpMenu();
                  return -1;
            case '?':
						if (optopt == 0) {
                 		printf("\nERROR: Unrecognized flag provided: %s\n", argv[optind - 1]);
						} else {
							printf("\nERROR: Unrecognized flag provided: -%c\n", optopt);
						}
                  helpMenu();
                  return -1;
            default: helpMenu(); return 0;	
		  }
		}
		if (hiddenFile == NULL) {
		 	printf("\nERROR: Missing -m flag for hidden file.\n");
         return -1;
      }
      if (coverFile == NULL) {
         printf("\nERROR: Missing -c flag for cover file.\n");
         return -1;
      }

		return hide_option(hiddenFile, coverFile, outputFile, parameters);
	 }  else if (strcmp(argv[1], "-extract") == 0) {
		while ((opt = getopt_long_only(argc, argv, ":s:o:", options, &opt_idx)) != -1) {
			switch (opt) {
				case 's': modified_audio = optarg; break;
				case 'o': outputFile     = optarg; break;
				case ':':
            	if (optopt == 0 && opt_idx >= 0) {  
						printf("\nERROR: Option '-%s' requires a filename argument provided.\n", options[opt_idx].name);
					} else {
						printf("\nERROR: Option '-%c' requires an argument provided.\n", optopt);
					}
               	helpMenu();
               	return -1;
         	case '?':
					if (optopt == 0) {
               	printf("\nERROR: Unrecognized flag provided: %s\n", argv[optind - 1]);
            	} else {
               	printf("\nERROR: Unrecognized flag provided: -%c\n", optopt);
            	}
               	helpMenu();
               	return -1;
				}
		}
	 	if (modified_audio == NULL) {
      	printf("\nERROR: Missing -s flag for the modified audio file.\n");
         return -1;
      }
	}	else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		helpMenu();	
		return 0;
	} else {
		printf("\nERROR: Invalid/missing mode option '%s'. Must specify -hide or -extract\n", argv[1]);
		helpMenu();
		return -1;
	}
	return extract_option(modified_audio, outputFile, parameters);
}
