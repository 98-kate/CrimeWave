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

	 printf("\nExamples: \n");
	 printf("\t./crimeWave -hide -m <message.ext> -c <cover_file.wav> [-o <stego_file.wav>]\n");
	 printf("\t./crimeWave -extract -s <stego_file.wav> [-o <message.ext>]\n");
}

int main(int argc, char * argv[]) {
	int opt;
	
	if (argc < 2) {
		helpMenu();
		return -1;
	}
	char * option = argv[1];

	if (strcmp(option, "-hide") == 0) {
		/** hiddenFile [-m], coverFile [-c], outputFile [-o] **/
		char * hiddenFile = NULL, * coverFile = NULL, * outputFile = NULL;
		optind = 2;
		while ((opt = getopt(argc, argv, "m:c:o:")) != -1) {
			switch (opt) {
				case 'm': hiddenFile = optarg; break;
				case 'c': coverFile = optarg; break;
				case 'o': outputFile = optarg; break;
				default: helpMenu(); return 0;
			}
		}		

		if (hiddenFile == NULL) {
			printf("\n\nERROR: Missing -m flag for hidden file!\n");
			helpMenu();
			return -1;
		}	 
		if (coverFile == NULL) {
		 	printf("\n\nERROR: Missing -c flag for cover file!\n");
			helpMenu();
			return -1;
		}	

	return hide_option(hiddenFile, coverFile, outputFile);
  } // END OF HIDE OPTION

	if (strcmp(option, "-extract") == 0) {
		char * modified_audio = NULL, * outputFile = NULL;
		optind = 2;
		while ((opt = getopt(argc, argv, "s:o:")) != -1) {
			switch (opt) {
				case 's': modified_audio = optarg; break;
				case 'o': outputFile = optarg; break;
			}
		}

		if (modified_audio == NULL) {
			printf("\n\nERROR: Missing -s flag for the modified audio file!\n");
			helpMenu();
			return -1;
		}
	return extract_option(modified_audio, outputFile);
} // END OF EXTRACT OPTION
	return 0;
}
