/**
 * dsps_fft2r_platform.h — stub to disable JPEGDEC's ESP32-S3 SIMD path.
 *
 * JPEGDEC's s3_simd_420.S includes this header to check whether the
 * ESP32-S3 AES3/SIMD instructions are available via the ESP-DSP component.
 * Defining the flag as 0 tells the assembler to skip the SIMD code path;
 * JPEGDEC falls back to its portable C implementation.  On a 240×240 display
 * refreshing album art the performance difference is imperceptible.
 *
 * This stub is placed in the project source directory so it shadows any
 * real esp-dsp header regardless of framework (Arduino or IDF).
 */
#pragma once
#define dsps_fft2r_sc16_aes3_enabled 0
