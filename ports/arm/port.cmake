# Developer libraries
include_directories(${PORT_DIR})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PORT_DIR}/libs/external/CMSIS-DSP/")
    message(STATUS "Using external CMSIS-DSP submodule")
    set(CMSIS_DSP_ROOT "${PORT_DIR}/libs/external/CMSIS-DSP")
else()
    message(STATUS "Using internal CMSIS-DSP")
    set(CMSIS_DSP_ROOT "${PORT_DIR}/libs/CMSIS-DSP")
endif()


include_directories(${CMSIS_DSP_ROOT}/Include)
include_directories(${CMSIS_DSP_ROOT}/PrivateInclude)

# ------------------------------------------------------------
# TensorFlow Lite integration
# ------------------------------------------------------------
if(USE_TFL)
    message(STATUS "Using TensorFlow Lite")

    # Set version macros (needed by release_version.h)
    add_definitions(
        -DTF_MAJOR_VERSION=2
        -DTF_MINOR_VERSION=20
        -DTF_PATCH_VERSION=0
        -DTF_VERSION_SUFFIX=""
        )

    add_subdirectory(${PORT_DIR}/libs/external/tensorflow/tensorflow/lite EXCLUDE_FROM_ALL)

    add_definitions(
        -DUSING_ACL_MATH_FUNCTIONS
        -DUSE_TFL
    )

    # Add TensorFlow Lite include paths
    include_directories(
        ${PORT_DIR}/libs/external/tensorflow
        ${PORT_DIR}/libs/external/tensorflow/tensorflow/lite
        ${CMAKE_BINARY_DIR}/flatbuffers/include/
    )

    list(APPEND EXTRA_LIBS
        tensorflow-lite
    )
else()
    message(STATUS "Using CMSIS-NN")
    include_directories(${PORT_DIR}/libs/CMSIS-NN/Include)
endif()

# Enbable Speex CMSIS DSP and custom optimizations
add_definitions(-DUSE_CMSIS_DSP)
#add_definitions(-DGENERIC_ARCH)

add_definitions(-DOVERRIDE_MDF_DC_NOTCH)
add_definitions(-DOVERRIDE_MDF_INNER_PROD)
add_definitions(-DOVERRIDE_MDF_POWER_SPECTRUM)
add_definitions(-DOVERRIDE_MDF_POWER_SPECTRUM_ACCUM)
add_definitions(-DOVERRIDE_MDF_SPECTRAL_MUL_ACCUM)
add_definitions(-DOVERRIDE_MDF_SPECTRAL_MUL_ACCUM16)
add_definitions(-DOVERRIDE_MDF_WEIGHT_SPECT_MUL_CONJ)
add_definitions(-DOVERRIDE_MDF_ADJUST_PROP)
add_definitions(-DOVERRIDE_MDF_PREEMPH_FLT)
add_definitions(-DOVERRIDE_MDF_STRIDED_PREEMPH_FLT)
add_definitions(-DOVERRIDE_MDF_VEC_SUB)
add_definitions(-DOVERRIDE_MDF_VEC_SUB16)
add_definitions(-DOVERRIDE_MDF_VEC_ADD)
add_definitions(-DOVERRIDE_MDF_VEC_MULT)
add_definitions(-DOVERRIDE_MDF_VEC_SCALE)
# add_definitions(-DOVERRIDE_MDF_VEC_CLEAR)
add_definitions(-DOVERRIDE_MDF_VEC_COPY)
add_definitions(-DOVERRIDE_MDF_SMOOTHED_ADD)
add_definitions(-DOVERRIDE_MDF_DEEMPH)
add_definitions(-DOVERRIDE_MDF_SMOOTH_FE_NRG)
add_definitions(-DOVERRIDE_MDF_FILTERED_SPEC_AD_XCORR)
add_definitions(-DOVERRIDE_MDF_NORM_LEARN_RATE_CALC)
add_definitions(-DOVERRIDE_MDF_CONVERG_LEARN_RATE_CALC)


# This is to make sure we include function decor for GCC/CLANG in CMSIS
# This might cause issues with arm-*-gcc toolchains though? If you see issues in
# arm_math_types.h it is probably related to this.
add_definitions(-D__GNUC_PYTHON__)



if(USE_CMSISDSP_NEON)
    message(STATUS "Using CMSIS-DSP + Neon")
    add_definitions(-DARM_MATH_NEON)

    include_directories(${CMSIS_DSP_ROOT}/ComputeLibrary/Include)
    include_directories(${CMSIS_DSP_ROOT}/Ne10)

    list(APPEND PORT_SOURCE
        ${CMSIS_DSP_ROOT}/ComputeLibrary/Source/arm_cl_tables.c

        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_generic_float16.neonintrisic.c
        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_generic_float32.neonintrisic.c
        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_generic_int32.c
        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_generic_int32.neonintrisic.c
        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_init.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_fft_float16.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_fft_float32.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_fft_int16.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_fft_int32.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_rfft_float16.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_rfft_float32.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Source/CommonTables/arm_neon_tables.c
        ${CMSIS_DSP_ROOT}/Source/CommonTables/arm_neon_tables_f16.c
    )
endif()



list(APPEND PORT_SOURCE
    ${PORT_DIR}/th_api.c

    ${CMSIS_DSP_ROOT}/Source/BasicMathFunctions/BasicMathFunctions.c
    ${CMSIS_DSP_ROOT}/Source/CommonTables/CommonTables.c
    ${CMSIS_DSP_ROOT}/Source/ComplexMathFunctions/ComplexMathFunctions.c
    ${CMSIS_DSP_ROOT}/Source/FastMathFunctions/FastMathFunctions.c
    ${CMSIS_DSP_ROOT}/Source/MatrixFunctions/MatrixFunctions.c
    ${CMSIS_DSP_ROOT}/Source/StatisticsFunctions/StatisticsFunctions.c
    ${CMSIS_DSP_ROOT}/Source/SupportFunctions/arm_float_to_q15.c
    ${CMSIS_DSP_ROOT}/Source/SupportFunctions/arm_q15_to_float.c
    ${CMSIS_DSP_ROOT}/Source/SupportFunctions/arm_copy_f32.c
    ${CMSIS_DSP_ROOT}/Source/TransformFunctions/TransformFunctions.c
)

if(USE_ARMNN)
	list(APPEND PORT_SOURCE ${PORT_DIR}/run_armnn.cpp)
elseif(USE_TFL)
	list(APPEND PORT_SOURCE ${PORT_DIR}/run_tfl_xnnpack.cpp)
else()
	list(APPEND PORT_SOURCE
        ${PORT_DIR}/libs/CMSIS-NN/Source/PoolingFunctions/arm_avgpool_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_wrapper_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1_x_n_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1x1_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_1x1_s8_fast.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_convolve_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_wrapper_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_3x3_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_depthwise_conv_s8_opt.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_mat_mult_kernel_s8_s16.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/ConvolutionFunctions/arm_nn_mat_mult_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/FullyConnectedFunctions/arm_fully_connected_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/SoftmaxFunctions/arm_softmax_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/SoftmaxFunctions/arm_nn_softmax_common_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mult_nt_t_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/NNSupportFunctions/arm_q7_to_q15_with_offset.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mul_core_1x_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/NNSupportFunctions/arm_nn_mat_mul_core_4x_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_s8.c
        ${PORT_DIR}/libs/CMSIS-NN/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_padded_s8.c
)
endif()
