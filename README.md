# CrimeWave
project for steg

CrimeWave is a C program that will use echo hiding to encode a message of any data type (text, image, etc ..) into 16-bit audio files (wav, mono). 

As of 07/13/2026: Uploaded the diagram I made-- Only the encoding and decoding parts which may be subject to further changes.

07/25/2026: Uploaded main & header files. Using dr_wav instead of the audio parser provided by the professor. 

## Acknowledgements

This project uses the following third-party libraries:
* [dr_wav](https://github.com/mackron/dr_libs) (v0.14.6) - A single-file WAV audio loader and writer by David Reid, released into the public domain / MIT-0.
