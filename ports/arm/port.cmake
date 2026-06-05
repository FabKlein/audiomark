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



option(USE_ARMNN "Enable Arm NN backend (linux)" OFF)
option(USE_TFL "Enable TensorFlow Lite backend (linux)" OFF)
option(USE_IMX93 "Use NXP i.MX93 TFLite fork and Ethos-U delegate (linux)" OFF)
option(AUDIOMARK_ARM_PROFILE "Enable private Arm-port AudioMark component timing" OFF)
option(AUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES "Exit after collecting Arm-port timing samples" ON)
set(AUDIOMARK_ARM_PROFILE_COUNTER "arch" CACHE STRING "Arm-port profile counter backend: arch or linux_ns")
set(AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ "" CACHE STRING "Core frequency in Hz used to convert time-based profile samples to cycles")
set_property(CACHE AUDIOMARK_ARM_PROFILE_COUNTER PROPERTY STRINGS arch linux_ns)

set(_USE_CMSISDSP_NEON_DEFAULT OFF)
if(CPU MATCHES "^cortex-a")
    set(_USE_CMSISDSP_NEON_DEFAULT ON)
endif()
option(USE_CMSISDSP_NEON "Enable CMSIS-DSP Neon support on Cortex-A builds" ${_USE_CMSISDSP_NEON_DEFAULT})

# only one can be ON
set(BACKEND_COUNT 0)
foreach(BACKEND USE_ARMNN USE_TFL USE_IMX93)
    if(${BACKEND})
        math(EXPR BACKEND_COUNT "${BACKEND_COUNT} + 1")
    endif()
endforeach()
math(EXPR BACKEND_COUNT
    "${BACKEND_COUNT}"
)

if(BACKEND_COUNT GREATER 1)
    message(FATAL_ERROR
        "Only one backend can be enabled. "
        "Currently set: USE_ARMNN=${USE_ARMNN}, USE_IMX93=${USE_IMX93}, USE_TFL=${USE_TFL}"
    )
endif()

if(AUDIOMARK_ARM_PROFILE)
    list(APPEND PORT_AUDIOMARK_SOURCE
        ${PORT_DIR}/ee_audiomark_profile.c
    )

    list(APPEND PORT_AUDIOMARK_LINK_OPTIONS
        -Wl,--wrap=ee_abf_f32
        -Wl,--wrap=ee_aec_f32
        -Wl,--wrap=ee_anr_f32
        -Wl,--wrap=ee_kws_f32
    )

    if(AUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES)
        list(APPEND PORT_AUDIOMARK_COMPILE_DEFINITIONS
            AUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES
        )
    endif()

    if(AUDIOMARK_ARM_PROFILE_COUNTER STREQUAL "linux_ns")
        list(APPEND PORT_AUDIOMARK_COMPILE_DEFINITIONS
            AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS
        )
    elseif(NOT AUDIOMARK_ARM_PROFILE_COUNTER STREQUAL "arch")
        message(FATAL_ERROR
            "AUDIOMARK_ARM_PROFILE_COUNTER must be one of: arch, linux_ns")
    endif()

    if(AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ)
        list(APPEND PORT_AUDIOMARK_COMPILE_DEFINITIONS
            AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ=${AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ}
        )
    endif()
endif()

# ------------------------------------------------------------
# ARMNN integration
# ------------------------------------------------------------

if(USE_ARMNN)
    message(STATUS "Using ARMNN")

    include(ExternalProject)

    get_filename_component(PORT_ABS ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
    set(ARMNN_ROOT ${PORT_ABS}/libs/external/armnn)

    get_filename_component(PORT_ABS ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
    set(ACL_ROOT ${PORT_ABS}/libs/external/acl)

    set(ARMCOMPUTE_ROOT ${PORT_DIR}/libs/external/ComputeLibrary)
    set(ACL_BUILD_DIR ${CMAKE_BINARY_DIR}/acl-build)

    #set(ML_TARGETS audiomark test_kws )

    include_directories(
        ${PORT_DIR}/libs/external/armnn/include/
        ${PORT_DIR}/libs/external/tensorflow
        ${PORT_DIR}/libs/external/tensorflow/tensorflow/lite
        ${CMAKE_BINARY_DIR}/flatbuffers/include/
    )

    add_definitions(
        -DUSING_ACL_MATH_FUNCTIONS
        -DUSE_ARMNN
    )

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_link_options(-lstdc++)
        set(CMAKE_EXE_LINKER_FLAGS "-static -static-libstdc++ -static-libgcc")
    endif()

    set(FLATBUFFERS_INCLUDE_PATH ${PORT_DIR}/libs/external/flatbuffers/include)
    set(ARMCOMPUTE_BUILD_DIR ${CMAKE_BINARY_DIR}/acl-build)
    set(ARMNN_BUILD_DIR ${CMAKE_BINARY_DIR}/armnn-build)
    set(HALF_INCLUDE_DIR ${PORT_DIR}/libs/external/armnn/third-party/half)

    # build ACL
    ExternalProject_Add(acl_external
    SOURCE_DIR ${ACL_ROOT}
    BINARY_DIR ${ACL_BUILD_DIR}
    INSTALL_DIR ${ACL_ROOT}/install
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${ACL_ROOT}/install
        -DCMAKE_BUILD_TYPE=Release
        -DARM_COMPUTE_ENABLE_OPENMP=OFF
        -DARM_COMPUTE_BUILD_SHARED_LIB=OFF
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
         INSTALL_COMMAND ""
        )

        # build Flatbuffers
    set(FLATBUFFERS_ROOT ${PORT_ABS}/libs/external/flatbuffers)
    set(FLATBUFFERS_BUILD_DIR ${CMAKE_BINARY_DIR}/flatbuffers-build)

    ExternalProject_Add(flatbuffers_external
    SOURCE_DIR ${FLATBUFFERS_ROOT}
    BINARY_DIR ${FLATBUFFERS_BUILD_DIR}
    INSTALL_DIR ${FLATBUFFERS_ROOT}/install
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${ACL_ROOT}/install
        -DCMAKE_BUILD_TYPE=Release
        -DFLATBUFFERS_BUILD_SHAREDLIB=OFF
        -DFLATBUFFERS_BUILD_TESTS=OFF
        -DFLATBUFFERS_BUILD_FLATC=OFF
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        INSTALL_COMMAND ""
        )

    set(FLATBUFFERS_LIBRARY_PATH ${FLATBUFFERS_BUILD_DIR}/libflatbuffers.a)


    get_filename_component(PORT_ABS ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)
    set(TF_LITE_SCHEMA_INCLUDE_PATH ${PORT_ABS}/libs/external/tensorflow/tensorflow/lite/schema)


    ExternalProject_Add(armnn_external
    # dependencies
    DEPENDS acl_external flatbuffers_external
    SOURCE_DIR ${ARMNN_ROOT}
    BINARY_DIR ${ARMNN_BUILD_DIR}
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/armnn-install
        -DARMCOMPUTENEON=1
        -DARMNNREF=1
        -DBUILD_TF_LITE_PARSER=ON
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_UNIT_TESTS=OFF
        -DFLATBUFFERS_INCLUDE_PATH=${FLATBUFFERS_ROOT}/include/
        -DARMCOMPUTE_ROOT=${ARMCOMPUTE_ROOT}
        -DARMCOMPUTE_BUILD_DIR=${ARMCOMPUTE_BUILD_DIR}
        -DFLATBUFFERS_LIBRARY=${FLATBUFFERS_BUILD_DIR}/libflatbuffers.a
        -DFLATBUFFERS_LIBRARY_RELEASE=${FLATBUFFERS_BUILD_DIR}/libflatbuffers.a
        -DFLATBUFFERS_LIBRARY_DEBUG=${FLATBUFFERS_BUILD_DIR}/libflatbuffers.a
        -DARMCOMPUTE_BUILD_DIR=${ACL_BUILD_DIR}
        -DARMCOMPUTE_INCLUDE=${ACL_ROOT}
        -DHALF_INCLUDE=${HALF_INCLUDE_DIR}
        -DARMCOMPUTE_LIBRARY_RELEASE=${ACL_BUILD_DIR}/libarm_compute.a
        -DARMCOMPUTE_LIBRARY_DEBUG=${ACL_BUILD_DIR}/libarm_compute.a
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS=-DTF_MAJOR_VERSION=2\ -DTF_MINOR_VERSION=20\ -DTF_PATCH_VERSION=0\ -DTF_VERSION_SUFFIX=\\\"\\\"
        -DTF_LITE_SCHEMA_INCLUDE_PATH=${TF_LITE_SCHEMA_INCLUDE_PATH}
        INSTALL_COMMAND ""
  )

  include_directories(${CMAKE_BINARY_DIR}/armnn-install/include)


    list(APPEND EXTRA_LIBS
        -Wl,--whole-archive
        ${ARMNN_BUILD_DIR}/libarmnn.a
        ${ARMNN_BUILD_DIR}/libarmnnUtils.a
        ${ARMNN_BUILD_DIR}/libarmnnTfLiteParser.a
        ${ARMNN_BUILD_DIR}/third-party/fmt/libfmt.a
        ${ARMNN_BUILD_DIR}/profiling/common/src/libpipeCommon.a
        ${ARMNN_BUILD_DIR}/profiling/client/src/libpipeClient.a
        -Wl,--no-whole-archive
        ${FLATBUFFERS_BUILD_DIR}/libflatbuffers.a
        ${ACL_BUILD_DIR}/libarm_compute.a
        pthread
        dl
    )




# ------------------------------------------------------------
# TensorFlow Lite integration
# ------------------------------------------------------------
elseif(USE_TFL)
    message(STATUS "Using TensorFlow Lite")

    if(CPU MATCHES "^cortex-a")
        set(CMAKE_SYSTEM_PROCESSOR aarch64)
        set(XNNPACK_TARGET_PROCESSOR aarch64)
        set(CPUINFO_TARGET_PROCESSOR aarch64)
        set(XNNPACK_TARGET_PROCESSOR aarch64 CACHE STRING "XNNPACK target processor" FORCE)
        set(CPUINFO_TARGET_PROCESSOR aarch64 CACHE STRING "cpuinfo target processor" FORCE)
    endif()

    # Set version macros (needed by release_version.h)
    add_definitions(
        -DTF_MAJOR_VERSION=2
        -DTF_MINOR_VERSION=20
        -DTF_PATCH_VERSION=0
        -DTF_VERSION_SUFFIX=""
        )

    set(_AUDIOMARK_CMAKE_WARN_DEPRECATED ${CMAKE_WARN_DEPRECATED})
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress deprecation warnings from vendored CMake projects" FORCE)
    add_subdirectory(${PORT_DIR}/libs/external/tensorflow/tensorflow/lite EXCLUDE_FROM_ALL)
    set(CMAKE_WARN_DEPRECATED ${_AUDIOMARK_CMAKE_WARN_DEPRECATED} CACHE BOOL "Show CMake deprecation warnings" FORCE)

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
        stdc++
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
        ${CMSIS_DSP_ROOT}/Ne10/NE10_fft_float32.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_fft_int32.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_fft_int16.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/NE10_rfft_float32.neonintrinsic.c
        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_init.c
        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_generic_float32.neonintrisic.c
        ${CMSIS_DSP_ROOT}/Ne10/CMSIS_NE10_fft_generic_int32.neonintrisic.c
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
