/**
 * dsps_fft2r_platform.h — stub for Arduino framework builds on ESP32-S3.
 *
 * JPEGDEC's s3_simd_420.S includes this header to check whether the
 * ESP32-S3 AES3/SIMD instructions are available via the ESP-DSP component.
 * When building with the Arduino framework the real esp-dsp headers are not
 * on the include path, causing a fatal "No such file or directory" error.
 *
 * Defining the flag as 0 tells the assembler to skip the SIMD code path;
 * JPEGDEC falls back to its portable C implementation.  On a 240×240 display
 * refreshing album art the performance difference is imperceptible.
 */
#pragma once
#define dsps_fft2r_sc16_aes3_enabled 0
