# Copyright (C) 2026 Arm Limited
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(audiomark_zephyr C CXX)

set(AUDIOMARK_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
set(AUDIOMARK_PORT_DIR ${AUDIOMARK_ROOT}/ports/arm)

set(AUDIOMARK_DEFAULT_MODEL_SOURCE
    ${AUDIOMARK_PORT_DIR}/ds_cnn_s_quantized.tflite.cpp)

if(CONFIG_BOARD_FVP_CORSTONE1000)
  set(AUDIOMARK_DEFAULT_MODEL_SOURCE
      ${AUDIOMARK_PORT_DIR}/ds_cnn_s_quantized_U85_256_vela.tflite.cpp)
elseif(CONFIG_BOARD_MPS4_CORSTONE320_FVP)
  set(AUDIOMARK_DEFAULT_MODEL_SOURCE
      ${AUDIOMARK_PORT_DIR}/ds_cnn_s_quantized_U85_256_vela.tflite.cpp)
endif()

set(AUDIOMARK_MODEL_SOURCE
    ${AUDIOMARK_DEFAULT_MODEL_SOURCE}
    CACHE FILEPATH "AudioMark DS-CNN TFLite model source to embed")

if(NOT IS_ABSOLUTE "${AUDIOMARK_MODEL_SOURCE}")
  get_filename_component(AUDIOMARK_MODEL_SOURCE "${AUDIOMARK_MODEL_SOURCE}"
                         ABSOLUTE BASE_DIR ${AUDIOMARK_ROOT})
endif()

if(NOT EXISTS "${AUDIOMARK_MODEL_SOURCE}")
  message(FATAL_ERROR "AUDIOMARK_MODEL_SOURCE does not exist: ${AUDIOMARK_MODEL_SOURCE}")
endif()

# Zephyr's CMSIS-DSP module can still be built with Neon through
# CONFIG_CMSIS_DSP_NEON. Keep AudioMark/Speex source-level overrides on the
# generic path: this Speex tree does not provide a complete Neon preprocessing
# path, and its fftwrap Neon calls expect a CMSIS-DSP RFFT scratch-argument API
# that Zephyr's CMSIS-DSP headers do not expose.
set(AUDIOMARK_DSP_ARCH_DEFS GENERIC_ARCH)

target_include_directories(app PRIVATE
  ${AUDIOMARK_ROOT}/src
  ${AUDIOMARK_ROOT}/lib/speexdsp
  ${AUDIOMARK_ROOT}/lib/speexdsp/include
  ${AUDIOMARK_ROOT}/lib/speexdsp/include/speex
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp
  ${AUDIOMARK_PORT_DIR}
  ${AUDIOMARK_PORT_DIR}/include
)

target_compile_definitions(app PRIVATE
  FLOATING_POINT
  EXPORT=
  ${AUDIOMARK_DSP_ARCH_DEFS}
  ARM_MATH_LOOPUNROLL
  REDUCED_FFT_TABSZ
  OVERRIDE_SPEEX_ALLOC
  OVERRIDE_FB_COMPUTE_BANK32
  OVERRIDE_FB_COMPUTE_PSD16
  OVERRIDE_ANR_VEC_MUL
  OVERRIDE_ANR_UPDATE_NOISE_ESTIMATE
  OVERRIDE_ANR_APOSTERIORI_SNR
  OVERRIDE_ANR_POWER_SPECTRUM
  OVERRIDE_ANR_COMPUTE_GAIN_FLOOR
  OVERRIDE_ANR_UPDATE_ZETA
  OVERRIDE_ANR_UPDATE_GAINS_CRITICAL_BANDS
  OVERRIDE_ANR_UPDATE_GAINS_LINEAR
  OVERRIDE_ANR_APPLY_SPEC_GAIN
  OVERRIDE_ANR_OLA
  OVERRIDE_ANR_UPDATE_NOISE_PROB
  OVERRIDE_MDF_DC_NOTCH
  OVERRIDE_MDF_INNER_PROD
  OVERRIDE_MDF_POWER_SPECTRUM
  OVERRIDE_MDF_POWER_SPECTRUM_ACCUM
  OVERRIDE_MDF_SPECTRAL_MUL_ACCUM
  OVERRIDE_MDF_WEIGHT_SPECT_MUL_CONJ
  OVERRIDE_MDF_ADJUST_PROP
  OVERRIDE_MDF_PREEMPH_FLT
  OVERRIDE_MDF_STRIDED_PREEMPH_FLT
  OVERRIDE_MDF_VEC_SUB
  OVERRIDE_MDF_VEC_SUB16
  OVERRIDE_MDF_VEC_ADD
  OVERRIDE_MDF_SMOOTHED_ADD
  OVERRIDE_MDF_DEEMPH
  OVERRIDE_MDF_SMOOTH_FE_NRG
  OVERRIDE_MDF_FILTERED_SPEC_AD_XCORR
  OVERRIDE_MDF_NORM_LEARN_RATE_CALC
  OVERRIDE_MDF_CONVERG_LEARN_RATE_CALC
  OVERRIDE_MDF_VEC_SCALE
  OS_SUPPORT_CUSTOM
  __RESTRICT=
  USE_CMSIS_DSP
  TF_INTERPRETER=1
  ACTIVATION_BUF_SZ=${CONFIG_AUDIOMARK_TENSOR_ARENA_SIZE}
)

set(AUDIOMARK_COMMON_KWS_SOURCES
  ${AUDIOMARK_ROOT}/src/ee_kws.c
  ${AUDIOMARK_ROOT}/src/ee_mfcc_f32.c
  ${AUDIOMARK_ROOT}/src/ee_mfcc_f32_tables.c
  ${AUDIOMARK_PORT_DIR}/th_api.c
  ${AUDIOMARK_PORT_DIR}/run_tflm_zephyr.cpp
  ${AUDIOMARK_MODEL_SOURCE}
)

set(AUDIOMARK_SPEEX_SOURCES
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/buffer.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/fftwrap.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/filterbank.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/jitter.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/kiss_fft.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/kiss_fftr.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/mdf.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/preprocess.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/resample.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/scal.c
  ${AUDIOMARK_ROOT}/lib/speexdsp/libspeexdsp/smallft.c
)

set(AUDIOMARK_COMMON_TEST_SOURCES
  src/test_main.c
  ${AUDIOMARK_PORT_DIR}/th_api.c
)

function(audiomark_add_unit_test TEST_SOURCE)
  set_source_files_properties(${TEST_SOURCE}
    PROPERTIES COMPILE_DEFINITIONS main=audiomark_test_main)
  target_sources(app PRIVATE
    ${AUDIOMARK_COMMON_TEST_SOURCES}
    ${TEST_SOURCE}
    ${ARGN}
  )
endfunction()

if(CONFIG_AUDIOMARK_TEST_KWS)
  audiomark_add_unit_test(${AUDIOMARK_ROOT}/tests/test_kws.c
    ${AUDIOMARK_ROOT}/src/ee_kws.c
    ${AUDIOMARK_ROOT}/src/ee_mfcc_f32.c
    ${AUDIOMARK_ROOT}/src/ee_mfcc_f32_tables.c
    ${AUDIOMARK_PORT_DIR}/run_tflm_zephyr.cpp
    ${AUDIOMARK_MODEL_SOURCE}
    ${AUDIOMARK_ROOT}/tests/data/kws_input.c
    ${AUDIOMARK_ROOT}/tests/data/kws_expected.c
  )
elseif(CONFIG_AUDIOMARK_TEST_MFCC)
  audiomark_add_unit_test(${AUDIOMARK_ROOT}/tests/test_mfcc_f32.c
    ${AUDIOMARK_ROOT}/src/ee_mfcc_f32.c
    ${AUDIOMARK_ROOT}/src/ee_mfcc_f32_tables.c
    ${AUDIOMARK_ROOT}/tests/data/mfcc_f32_all.c
  )
elseif(CONFIG_AUDIOMARK_TEST_AEC)
  audiomark_add_unit_test(${AUDIOMARK_ROOT}/tests/test_aec_f32.c
    ${AUDIOMARK_ROOT}/src/ee_aec_f32.c
    ${AUDIOMARK_SPEEX_SOURCES}
    ${AUDIOMARK_ROOT}/tests/data/aec_f32_input_source.c
    ${AUDIOMARK_ROOT}/tests/data/aec_f32_input_echo.c
    ${AUDIOMARK_ROOT}/tests/data/aec_f32_expected.c
  )
elseif(CONFIG_AUDIOMARK_TEST_ANR)
  audiomark_add_unit_test(${AUDIOMARK_ROOT}/tests/test_anr_f32.c
    ${AUDIOMARK_ROOT}/src/ee_anr_f32.c
    ${AUDIOMARK_SPEEX_SOURCES}
    ${AUDIOMARK_ROOT}/tests/data/anr_f32_input.c
    ${AUDIOMARK_ROOT}/tests/data/anr_f32_expected.c
  )
elseif(CONFIG_AUDIOMARK_TEST_ABF)
  audiomark_add_unit_test(${AUDIOMARK_ROOT}/tests/test_abf_f32.c
    ${AUDIOMARK_ROOT}/src/ee_abf_f32.c
    ${AUDIOMARK_ROOT}/src/ee_abf_f32_tables.c
    ${AUDIOMARK_ROOT}/tests/data/abf_f32_input_ch1.c
    ${AUDIOMARK_ROOT}/tests/data/abf_f32_input_ch2.c
    ${AUDIOMARK_ROOT}/tests/data/abf_f32_expected.c
  )
else()
  target_sources(app PRIVATE
    src/main.c

    ${AUDIOMARK_ROOT}/src/ee_audiomark.c
    ${AUDIOMARK_ROOT}/src/ee_aec_f32.c
    ${AUDIOMARK_ROOT}/src/ee_anr_f32.c
    ${AUDIOMARK_ROOT}/src/ee_abf_f32.c
    ${AUDIOMARK_ROOT}/src/ee_abf_f32_tables.c
    ${AUDIOMARK_COMMON_KWS_SOURCES}
    ${AUDIOMARK_SPEEX_SOURCES}
  )
endif()

target_link_libraries(app PRIVATE m)

zephyr_linker_sources_ifdef(CONFIG_SOC_SERIES_MPS4 SECTIONS linker.ld)
zephyr_linker_sources_ifdef(CONFIG_BOARD_FVP_CORSTONE1000 SECTIONS linker.ld)
