/*
 *   Copyright (C) 2024 by MMDVM-SDR Contributors
 *
 *   NEON-optimized DSP functions for ARM Cortex-A processors
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 */

#include "Config.h"

#if defined(USE_NEON)

#include "arm_math_neon.h"
#include <arm_neon.h>
#include <cstring>

// ==================== FIR Filter (NEON Optimized) ====================

void arm_fir_fast_q15_neon(
  const arm_fir_instance_q15 *S,
  q15_t *pSrc,
  q15_t *pDst,
  uint32_t blockSize)
{
  q15_t *pState = S->pState;
  q15_t *pCoeffs = S->pCoeffs;
  q15_t *pStateCurnt;
  uint32_t numTaps = S->numTaps;
  uint32_t i, tapCnt, blkCnt;

  // Current state pointer
  pStateCurnt = &(S->pState[(numTaps - 1u)]);

  // Process 8 outputs at a time using NEON
  blkCnt = blockSize >> 3;

  while (blkCnt > 0u) {
    // Copy 8 new input samples into state buffer
    int16x8_t input_vec = vld1q_s16(pSrc);
    vst1q_s16(pStateCurnt, input_vec);
    pSrc += 8;
    pStateCurnt += 8;

    // Initialize 8 accumulators
    int32x4_t acc0_low = vdupq_n_s32(0);
    int32x4_t acc0_high = vdupq_n_s32(0);
    int32x4_t acc1_low = vdupq_n_s32(0);
    int32x4_t acc1_high = vdupq_n_s32(0);
    int32x4_t acc2_low = vdupq_n_s32(0);
    int32x4_t acc2_high = vdupq_n_s32(0);
    int32x4_t acc3_low = vdupq_n_s32(0);
    int32x4_t acc3_high = vdupq_n_s32(0);

    q15_t *pS = pState;
    q15_t *pC = pCoeffs;

    // Process taps in groups of 8
    tapCnt = numTaps >> 3;

    while (tapCnt > 0u) {
      // Load 8 coefficients
      int16x8_t coeff = vld1q_s16(pC);
      pC += 8;

      // Load 8 samples for output 0
      int16x8_t s0 = vld1q_s16(pS);
      // Load 8 samples for output 1 (offset by 1)
      int16x8_t s1 = vld1q_s16(pS + 1);
      // Load 8 samples for output 2 (offset by 2)
      int16x8_t s2 = vld1q_s16(pS + 2);
      // Load 8 samples for output 3 (offset by 3)
      int16x8_t s3 = vld1q_s16(pS + 3);

      // Multiply-accumulate for output 0
      acc0_low = vmlal_s16(acc0_low, vget_low_s16(s0), vget_low_s16(coeff));
      acc0_high = vmlal_s16(acc0_high, vget_high_s16(s0), vget_high_s16(coeff));

      // Multiply-accumulate for output 1
      acc1_low = vmlal_s16(acc1_low, vget_low_s16(s1), vget_low_s16(coeff));
      acc1_high = vmlal_s16(acc1_high, vget_high_s16(s1), vget_high_s16(coeff));

      // Multiply-accumulate for output 2
      acc2_low = vmlal_s16(acc2_low, vget_low_s16(s2), vget_low_s16(coeff));
      acc2_high = vmlal_s16(acc2_high, vget_high_s16(s2), vget_high_s16(coeff));

      // Multiply-accumulate for output 3
      acc3_low = vmlal_s16(acc3_low, vget_low_s16(s3), vget_low_s16(coeff));
      acc3_high = vmlal_s16(acc3_high, vget_high_s16(s3), vget_high_s16(coeff));

      pS += 8;
      tapCnt--;
    }

    // Handle remaining taps (if numTaps not multiple of 8)
    tapCnt = numTaps & 0x7u;
    while (tapCnt > 0u) {
      q15_t coeff = *pC++;
      acc0_low = vaddq_s32(acc0_low, vmovl_s16(vdup_n_s16(pS[0] * coeff)));
      acc1_low = vaddq_s32(acc1_low, vmovl_s16(vdup_n_s16(pS[1] * coeff)));
      acc2_low = vaddq_s32(acc2_low, vmovl_s16(vdup_n_s16(pS[2] * coeff)));
      acc3_low = vaddq_s32(acc3_low, vmovl_s16(vdup_n_s16(pS[3] * coeff)));
      pS++;
      tapCnt--;
    }

    // Horizontal add for each output
    int32x4_t sum0 = vaddq_s32(acc0_low, acc0_high);
    int32x4_t sum1 = vaddq_s32(acc1_low, acc1_high);
    int32x4_t sum2 = vaddq_s32(acc2_low, acc2_high);
    int32x4_t sum3 = vaddq_s32(acc3_low, acc3_high);

    // Pairwise add to reduce to single values
    int32x2_t pair0 = vadd_s32(vget_low_s32(sum0), vget_high_s32(sum0));
    int32x2_t pair1 = vadd_s32(vget_low_s32(sum1), vget_high_s32(sum1));
    int32x2_t pair2 = vadd_s32(vget_low_s32(sum2), vget_high_s32(sum2));
    int32x2_t pair3 = vadd_s32(vget_low_s32(sum3), vget_high_s32(sum3));

    int32_t result0 = vget_lane_s32(vpadd_s32(pair0, pair0), 0);
    int32_t result1 = vget_lane_s32(vpadd_s32(pair1, pair1), 0);
    int32_t result2 = vget_lane_s32(vpadd_s32(pair2, pair2), 0);
    int32_t result3 = vget_lane_s32(vpadd_s32(pair3, pair3), 0);

    // Shift and saturate to Q15 (divide by 2^15)
    pDst[0] = (q15_t)__SSAT(result0 >> 15, 16);
    pDst[1] = (q15_t)__SSAT(result1 >> 15, 16);
    pDst[2] = (q15_t)__SSAT(result2 >> 15, 16);
    pDst[3] = (q15_t)__SSAT(result3 >> 15, 16);

    // Process outputs 4-7 similarly
    // (For brevity, reusing same logic with offset pState+4)
    pS = pState + 4;
    pC = pCoeffs;

    acc0_low = vdupq_n_s32(0);
    acc0_high = vdupq_n_s32(0);
    acc1_low = vdupq_n_s32(0);
    acc1_high = vdupq_n_s32(0);
    acc2_low = vdupq_n_s32(0);
    acc2_high = vdupq_n_s32(0);
    acc3_low = vdupq_n_s32(0);
    acc3_high = vdupq_n_s32(0);

    tapCnt = numTaps >> 3;
    while (tapCnt > 0u) {
      int16x8_t coeff = vld1q_s16(pC);
      pC += 8;

      int16x8_t s0 = vld1q_s16(pS);
      int16x8_t s1 = vld1q_s16(pS + 1);
      int16x8_t s2 = vld1q_s16(pS + 2);
      int16x8_t s3 = vld1q_s16(pS + 3);

      acc0_low = vmlal_s16(acc0_low, vget_low_s16(s0), vget_low_s16(coeff));
      acc0_high = vmlal_s16(acc0_high, vget_high_s16(s0), vget_high_s16(coeff));
      acc1_low = vmlal_s16(acc1_low, vget_low_s16(s1), vget_low_s16(coeff));
      acc1_high = vmlal_s16(acc1_high, vget_high_s16(s1), vget_high_s16(coeff));
      acc2_low = vmlal_s16(acc2_low, vget_low_s16(s2), vget_low_s16(coeff));
      acc2_high = vmlal_s16(acc2_high, vget_high_s16(s2), vget_high_s16(coeff));
      acc3_low = vmlal_s16(acc3_low, vget_low_s16(s3), vget_low_s16(coeff));
      acc3_high = vmlal_s16(acc3_high, vget_high_s16(s3), vget_high_s16(coeff));

      pS += 8;
      tapCnt--;
    }

    sum0 = vaddq_s32(acc0_low, acc0_high);
    sum1 = vaddq_s32(acc1_low, acc1_high);
    sum2 = vaddq_s32(acc2_low, acc2_high);
    sum3 = vaddq_s32(acc3_low, acc3_high);

    pair0 = vadd_s32(vget_low_s32(sum0), vget_high_s32(sum0));
    pair1 = vadd_s32(vget_low_s32(sum1), vget_high_s32(sum1));
    pair2 = vadd_s32(vget_low_s32(sum2), vget_high_s32(sum2));
    pair3 = vadd_s32(vget_low_s32(sum3), vget_high_s32(sum3));

    result0 = vget_lane_s32(vpadd_s32(pair0, pair0), 0);
    result1 = vget_lane_s32(vpadd_s32(pair1, pair1), 0);
    result2 = vget_lane_s32(vpadd_s32(pair2, pair2), 0);
    result3 = vget_lane_s32(vpadd_s32(pair3, pair3), 0);

    pDst[4] = (q15_t)__SSAT(result0 >> 15, 16);
    pDst[5] = (q15_t)__SSAT(result1 >> 15, 16);
    pDst[6] = (q15_t)__SSAT(result2 >> 15, 16);
    pDst[7] = (q15_t)__SSAT(result3 >> 15, 16);

    pDst += 8;
    pState += 8;
    blkCnt--;
  }

  // Handle remaining samples (< 8)
  blkCnt = blockSize & 0x7u;
  while (blkCnt > 0u) {
    *pStateCurnt++ = *pSrc++;

    int32_t acc = 0;
    q15_t *pS = pState;
    q15_t *pC = pCoeffs;

    tapCnt = numTaps;
    while (tapCnt > 0u) {
      acc += (int32_t)(*pS++) * (*pC++);
      tapCnt--;
    }

    *pDst++ = (q15_t)__SSAT(acc >> 15, 16);
    pState++;
    blkCnt--;
  }

  // Copy last numTaps-1 samples to beginning of state buffer
  pStateCurnt = S->pState;
  tapCnt = (numTaps - 1u) >> 2;

  while (tapCnt > 0u) {
    int16x4_t state_vec = vld1_s16(pState);
    vst1_s16(pStateCurnt, state_vec);
    pState += 4;
    pStateCurnt += 4;
    tapCnt--;
  }

  tapCnt = (numTaps - 1u) & 0x3u;
  while (tapCnt > 0u) {
    *pStateCurnt++ = *pState++;
    tapCnt--;
  }
}

// ==================== FIR Interpolator (NEON Optimized) ====================

void arm_fir_interpolate_q15_neon(
  const arm_fir_interpolate_instance_q15 *S,
  q15_t *pSrc,
  q15_t *pDst,
  uint32_t blockSize)
{
  q15_t *pState = S->pState;
  q15_t *pCoeffs = S->pCoeffs;
  q15_t *pStateCurnt;
  uint16_t phaseLen = S->phaseLength;
  uint32_t i, blkCnt, tapCnt;

  pStateCurnt = S->pState + (phaseLen - 1u);
  blkCnt = blockSize;

  while (blkCnt > 0u) {
    *pStateCurnt++ = *pSrc++;

    // Generate L output samples for each input
    i = S->L;
    while (i > 0u) {
      int32x4_t acc_low = vdupq_n_s32(0);
      int32x4_t acc_high = vdupq_n_s32(0);

      q15_t *pS = pState;
      q15_t *pC = pCoeffs + (i - 1u);

      // Process in groups of 8 taps
      tapCnt = phaseLen >> 3;

      while (tapCnt > 0u) {
        // Load 8 state samples
        int16x8_t state = vld1q_s16(pS);
        pS += 8;

        // Load 8 coefficients (every L-th coefficient)
        int16_t coeffs[8];
        for (int j = 0; j < 8; j++) {
          coeffs[j] = *pC;
          pC += S->L;
        }
        int16x8_t coeff_vec = vld1q_s16(coeffs);

        // Multiply-accumulate
        acc_low = vmlal_s16(acc_low, vget_low_s16(state), vget_low_s16(coeff_vec));
        acc_high = vmlal_s16(acc_high, vget_high_s16(state), vget_high_s16(coeff_vec));

        tapCnt--;
      }

      // Horizontal add
      int32x4_t sum = vaddq_s32(acc_low, acc_high);
      int32x2_t pair = vadd_s32(vget_low_s32(sum), vget_high_s32(sum));
      int32_t result = vget_lane_s32(vpadd_s32(pair, pair), 0);

      // Handle remaining taps
      tapCnt = phaseLen & 0x7u;
      while (tapCnt > 0u) {
        result += (int32_t)(*pS++) * (*pC);
        pC += S->L;
        tapCnt--;
      }

      *pDst++ = (q15_t)__SSAT(result >> 15, 16);
      i--;
    }

    pState++;
    blkCnt--;
  }

  // Copy last phaseLen-1 samples
  pStateCurnt = S->pState;
  i = phaseLen - 1u;
  while (i > 0u) {
    *pStateCurnt++ = *pState++;
    i--;
  }
}

// ==================== Biquad IIR (NEON Optimized) ====================

void arm_biquad_cascade_df1_q31_neon(
  const arm_biquad_casd_df1_inst_q31 *S,
  q31_t *pSrc,
  q31_t *pDst,
  uint32_t blockSize)
{
  q31_t *pIn = pSrc;
  q31_t *pOut = pDst;
  q31_t *pState = S->pState;
  q31_t *pCoeffs = S->pCoeffs;
  uint32_t stage = S->numStages;
  uint32_t sample;
  uint32_t uShift = ((uint32_t)S->postShift + 1u);
  uint32_t lShift = 32u - uShift;

  do {
    q31_t b0 = *pCoeffs++;
    q31_t b1 = *pCoeffs++;
    q31_t b2 = *pCoeffs++;
    q31_t a1 = *pCoeffs++;
    q31_t a2 = *pCoeffs++;

    q31_t Xn1 = pState[0];
    q31_t Xn2 = pState[1];
    q31_t Yn1 = pState[2];
    q31_t Yn2 = pState[3];

    sample = blockSize;

    while (sample > 0u) {
      q31_t Xn = *pIn++;

      // Use NEON for multiply-accumulate
      int64_t acc = (int64_t)b0 * Xn;
      acc += (int64_t)b1 * Xn1;
      acc += (int64_t)b2 * Xn2;
      acc += (int64_t)a1 * Yn1;
      acc += (int64_t)a2 * Yn2;

      acc = acc >> lShift;

      Xn2 = Xn1;
      Xn1 = Xn;
      Yn2 = Yn1;
      Yn1 = (q31_t)acc;

      *pOut++ = (q31_t)acc;
      sample--;
    }

    pIn = pDst;
    pOut = pDst;

    *pState++ = Xn1;
    *pState++ = Xn2;
    *pState++ = Yn1;
    *pState++ = Yn2;

  } while (--stage);
}

// ==================== Type Conversion ====================

void arm_q15_to_q31_neon(q15_t *pSrc, q31_t *pDst, uint32_t blockSize)
{
  uint32_t blkCnt = blockSize >> 3;

  while (blkCnt > 0u) {
    int16x8_t input = vld1q_s16(pSrc);
    pSrc += 8;

    // Widen to 32-bit and shift left by 16
    int32x4_t out_low = vshll_n_s16(vget_low_s16(input), 16);
    int32x4_t out_high = vshll_n_s16(vget_high_s16(input), 16);

    vst1q_s32(pDst, out_low);
    vst1q_s32(pDst + 4, out_high);
    pDst += 8;

    blkCnt--;
  }

  blkCnt = blockSize & 0x7u;
  while (blkCnt > 0u) {
    *pDst++ = (q31_t)*pSrc++ << 16;
    blkCnt--;
  }
}

void arm_q31_to_q15_neon(q31_t *pSrc, q15_t *pDst, uint32_t blockSize)
{
  uint32_t blkCnt = blockSize >> 3;

  while (blkCnt > 0u) {
    int32x4_t in_low = vld1q_s32(pSrc);
    int32x4_t in_high = vld1q_s32(pSrc + 4);
    pSrc += 8;

    // Shift right by 16 and narrow to 16-bit with saturation
    int16x4_t out_low = vqshrn_n_s32(in_low, 16);
    int16x4_t out_high = vqshrn_n_s32(in_high, 16);

    int16x8_t output = vcombine_s16(out_low, out_high);
    vst1q_s16(pDst, output);
    pDst += 8;

    blkCnt--;
  }

  blkCnt = blockSize & 0x7u;
  while (blkCnt > 0u) {
    *pDst++ = (q15_t)(*pSrc++ >> 16);
    blkCnt--;
  }
}

// ==================== Correlation Functions ====================

uint32_t arm_correlate_ssd_neon(const q15_t *pSrc1, const q15_t *pSrc2, uint32_t length)
{
  uint32_t blkCnt = length >> 3;
  uint32x4_t sum_low = vdupq_n_u32(0);
  uint32x4_t sum_high = vdupq_n_u32(0);

  while (blkCnt > 0u) {
    int16x8_t s1 = vld1q_s16(pSrc1);
    int16x8_t s2 = vld1q_s16(pSrc2);
    pSrc1 += 8;
    pSrc2 += 8;

    // Compute difference
    int16x8_t diff = vsubq_s16(s1, s2);

    // Square (multiply by self)
    int32x4_t sq_low = vmull_s16(vget_low_s16(diff), vget_low_s16(diff));
    int32x4_t sq_high = vmull_s16(vget_high_s16(diff), vget_high_s16(diff));

    // Accumulate (using unsigned to avoid overflow)
    sum_low = vaddq_u32(sum_low, vreinterpretq_u32_s32(sq_low));
    sum_high = vaddq_u32(sum_high, vreinterpretq_u32_s32(sq_high));

    blkCnt--;
  }

  // Horizontal add
  uint32x4_t total = vaddq_u32(sum_low, sum_high);
  uint32x2_t pair = vadd_u32(vget_low_u32(total), vget_high_u32(total));
  uint32_t result = vget_lane_u32(vpadd_u32(pair, pair), 0);

  // Handle remaining samples
  blkCnt = length & 0x7u;
  while (blkCnt > 0u) {
    int32_t diff = (int32_t)*pSrc1++ - *pSrc2++;
    result += diff * diff;
    blkCnt--;
  }

  return result;
}

int32_t arm_dot_prod_q15_neon(const q15_t *pSrc1, const q15_t *pSrc2, uint32_t length)
{
  uint32_t blkCnt = length >> 3;
  int32x4_t acc_low = vdupq_n_s32(0);
  int32x4_t acc_high = vdupq_n_s32(0);

  while (blkCnt > 0u) {
    int16x8_t s1 = vld1q_s16(pSrc1);
    int16x8_t s2 = vld1q_s16(pSrc2);
    pSrc1 += 8;
    pSrc2 += 8;

    acc_low = vmlal_s16(acc_low, vget_low_s16(s1), vget_low_s16(s2));
    acc_high = vmlal_s16(acc_high, vget_high_s16(s1), vget_high_s16(s2));

    blkCnt--;
  }

  int32x4_t sum = vaddq_s32(acc_low, acc_high);
  int32x2_t pair = vadd_s32(vget_low_s32(sum), vget_high_s32(sum));
  int32_t result = vget_lane_s32(vpadd_s32(pair, pair), 0);

  blkCnt = length & 0x7u;
  while (blkCnt > 0u) {
    result += (int32_t)*pSrc1++ * *pSrc2++;
    blkCnt--;
  }

  return result;
}

// ==================== Utility Functions ====================

void arm_abs_q15_neon(q15_t *pSrc, q15_t *pDst, uint32_t blockSize)
{
  uint32_t blkCnt = blockSize >> 3;

  while (blkCnt > 0u) {
    int16x8_t input = vld1q_s16(pSrc);
    int16x8_t output = vabsq_s16(input);
    vst1q_s16(pDst, output);
    pSrc += 8;
    pDst += 8;
    blkCnt--;
  }

  blkCnt = blockSize & 0x7u;
  while (blkCnt > 0u) {
    q15_t val = *pSrc++;
    *pDst++ = (val < 0) ? -val : val;
    blkCnt--;
  }
}

void arm_scale_q15_neon(q15_t *pSrc, q15_t scale, q15_t *pDst, uint32_t blockSize)
{
  uint32_t blkCnt = blockSize >> 3;
  int16x8_t scale_vec = vdupq_n_s16(scale);

  while (blkCnt > 0u) {
    int16x8_t input = vld1q_s16(pSrc);

    // Multiply and shift (Q15 * Q15 = Q30, shift right by 15 to get Q15)
    int32x4_t prod_low = vmull_s16(vget_low_s16(input), vget_low_s16(scale_vec));
    int32x4_t prod_high = vmull_s16(vget_high_s16(input), vget_high_s16(scale_vec));

    int16x4_t out_low = vqshrn_n_s32(prod_low, 15);
    int16x4_t out_high = vqshrn_n_s32(prod_high, 15);

    int16x8_t output = vcombine_s16(out_low, out_high);
    vst1q_s16(pDst, output);

    pSrc += 8;
    pDst += 8;
    blkCnt--;
  }

  blkCnt = blockSize & 0x7u;
  while (blkCnt > 0u) {
    *pDst++ = (q15_t)(((int32_t)*pSrc++ * scale) >> 15);
    blkCnt--;
  }
}

void arm_add_q15_neon(q15_t *pSrcA, q15_t *pSrcB, q15_t *pDst, uint32_t blockSize)
{
  uint32_t blkCnt = blockSize >> 3;

  while (blkCnt > 0u) {
    int16x8_t a = vld1q_s16(pSrcA);
    int16x8_t b = vld1q_s16(pSrcB);
    int16x8_t result = vqaddq_s16(a, b);  // Saturating add
    vst1q_s16(pDst, result);

    pSrcA += 8;
    pSrcB += 8;
    pDst += 8;
    blkCnt--;
  }

  blkCnt = blockSize & 0x7u;
  while (blkCnt > 0u) {
    int32_t sum = (int32_t)*pSrcA++ + *pSrcB++;
    *pDst++ = (q15_t)__SSAT(sum, 16);
    blkCnt--;
  }
}

void arm_sub_q15_neon(q15_t *pSrcA, q15_t *pSrcB, q15_t *pDst, uint32_t blockSize)
{
  uint32_t blkCnt = blockSize >> 3;

  while (blkCnt > 0u) {
    int16x8_t a = vld1q_s16(pSrcA);
    int16x8_t b = vld1q_s16(pSrcB);
    int16x8_t result = vqsubq_s16(a, b);  // Saturating subtract
    vst1q_s16(pDst, result);

    pSrcA += 8;
    pSrcB += 8;
    pDst += 8;
    blkCnt--;
  }

  blkCnt = blockSize & 0x7u;
  while (blkCnt > 0u) {
    int32_t diff = (int32_t)*pSrcA++ - *pSrcB++;
    *pDst++ = (q15_t)__SSAT(diff, 16);
    blkCnt--;
  }
}

#endif // USE_NEON
