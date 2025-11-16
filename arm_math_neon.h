/*
 *   Copyright (C) 2024 by MMDVM-SDR Contributors
 *
 *   NEON-optimized DSP functions for ARM Cortex-A processors
 *   Based on ARM CMSIS DSP library architecture
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#if !defined(ARM_MATH_NEON_H)
#define ARM_MATH_NEON_H

#if defined(USE_NEON)

#include <stdint.h>

// Type definitions (compatible with ARM CMSIS DSP)
typedef int16_t q15_t;
typedef int32_t q31_t;
typedef int64_t q63_t;

// Saturation macro
#define __SSAT(ARG1, ARG2) \
  ({ \
    int32_t __RES, __ARG1 = (ARG1); \
    if (__ARG1 > ((1 << (ARG2 - 1)) - 1)) \
      __RES = ((1 << (ARG2 - 1)) - 1); \
    else if (__ARG1 < -(1 << (ARG2 - 1))) \
      __RES = -(1 << (ARG2 - 1)); \
    else \
      __RES = __ARG1; \
    __RES; \
  })

/**
 * FIR filter instance structure
 */
typedef struct {
  uint16_t numTaps;      // Number of filter coefficients
  q15_t *pState;         // State buffer [numTaps + blockSize - 1]
  q15_t *pCoeffs;        // Filter coefficients [numTaps]
} arm_fir_instance_q15;

/**
 * FIR interpolator instance structure
 */
typedef struct {
  uint8_t L;             // Upsample factor
  uint16_t phaseLength;  // Length of each polyphase filter
  q15_t *pCoeffs;        // Filter coefficients [L * phaseLength]
  q15_t *pState;         // State buffer [phaseLength + blockSize - 1]
} arm_fir_interpolate_instance_q15;

/**
 * Biquad cascade DF1 instance structure
 */
typedef struct {
  uint32_t numStages;    // Number of 2nd order stages
  q31_t *pState;         // State buffer [4 * numStages]
  q31_t *pCoeffs;        // Coefficients [6 * numStages] {b0,0,b1,b2,-a1,-a2}
  int8_t postShift;      // Post-shift for output
} arm_biquad_casd_df1_inst_q31;

// ==================== NEON-Optimized Functions ====================

/**
 * NEON-optimized FIR filter for Q15 data
 * Processes 8 output samples at a time using NEON intrinsics
 *
 * @param S Pointer to FIR instance
 * @param pSrc Pointer to input samples
 * @param pDst Pointer to output samples
 * @param blockSize Number of samples to process
 */
void arm_fir_fast_q15_neon(
  const arm_fir_instance_q15 *S,
  q15_t *pSrc,
  q15_t *pDst,
  uint32_t blockSize);

/**
 * NEON-optimized FIR interpolator for Q15 data
 * Used for TX sample upsampling
 *
 * @param S Pointer to interpolator instance
 * @param pSrc Pointer to input samples
 * @param pDst Pointer to output samples
 * @param blockSize Number of input samples
 */
void arm_fir_interpolate_q15_neon(
  const arm_fir_interpolate_instance_q15 *S,
  q15_t *pSrc,
  q15_t *pDst,
  uint32_t blockSize);

/**
 * NEON-optimized biquad IIR cascade for Q31 data
 * Used for DC blocking and other IIR filtering
 *
 * @param S Pointer to biquad instance
 * @param pSrc Pointer to input samples
 * @param pDst Pointer to output samples
 * @param blockSize Number of samples to process
 */
void arm_biquad_cascade_df1_q31_neon(
  const arm_biquad_casd_df1_inst_q31 *S,
  q31_t *pSrc,
  q31_t *pDst,
  uint32_t blockSize);

/**
 * Convert Q15 to Q31
 *
 * @param pSrc Pointer to Q15 input
 * @param pDst Pointer to Q31 output
 * @param blockSize Number of samples
 */
void arm_q15_to_q31_neon(
  q15_t *pSrc,
  q31_t *pDst,
  uint32_t blockSize);

/**
 * Convert Q31 to Q15
 *
 * @param pSrc Pointer to Q31 input
 * @param pDst Pointer to Q15 output
 * @param blockSize Number of samples
 */
void arm_q31_to_q15_neon(
  q31_t *pSrc,
  q15_t *pDst,
  uint32_t blockSize);

// ==================== Correlation Functions ====================

/**
 * NEON-optimized correlation (sum of squared differences)
 * Used for sync pattern matching
 *
 * @param pSrc1 Pointer to first sequence
 * @param pSrc2 Pointer to second sequence
 * @param length Number of samples to correlate
 * @return Sum of squared differences
 */
uint32_t arm_correlate_ssd_neon(
  const q15_t *pSrc1,
  const q15_t *pSrc2,
  uint32_t length);

/**
 * NEON-optimized dot product
 *
 * @param pSrc1 Pointer to first sequence
 * @param pSrc2 Pointer to second sequence
 * @param length Number of samples
 * @return Dot product result
 */
int32_t arm_dot_prod_q15_neon(
  const q15_t *pSrc1,
  const q15_t *pSrc2,
  uint32_t length);

// ==================== Utility Functions ====================

/**
 * NEON-optimized absolute value
 *
 * @param pSrc Pointer to input
 * @param pDst Pointer to output
 * @param blockSize Number of samples
 */
void arm_abs_q15_neon(
  q15_t *pSrc,
  q15_t *pDst,
  uint32_t blockSize);

/**
 * NEON-optimized multiply by constant
 *
 * @param pSrc Pointer to input
 * @param scale Scale factor (Q15)
 * @param pDst Pointer to output
 * @param blockSize Number of samples
 */
void arm_scale_q15_neon(
  q15_t *pSrc,
  q15_t scale,
  q15_t *pDst,
  uint32_t blockSize);

/**
 * NEON-optimized vector addition
 *
 * @param pSrcA Pointer to first input
 * @param pSrcB Pointer to second input
 * @param pDst Pointer to output
 * @param blockSize Number of samples
 */
void arm_add_q15_neon(
  q15_t *pSrcA,
  q15_t *pSrcB,
  q15_t *pDst,
  uint32_t blockSize);

/**
 * NEON-optimized vector subtraction
 *
 * @param pSrcA Pointer to first input
 * @param pSrcB Pointer to second input
 * @param pDst Pointer to output (A - B)
 * @param blockSize Number of samples
 */
void arm_sub_q15_neon(
  q15_t *pSrcA,
  q15_t *pSrcB,
  q15_t *pDst,
  uint32_t blockSize);

#endif // USE_NEON

#endif // ARM_MATH_NEON_H
