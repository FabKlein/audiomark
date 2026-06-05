cmake_minimum_required(VERSION 3.10.2)

# Use fresh extraction timestamps for downloaded dependencies when supported.
if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
endif()

project(audiomark
	DESCRIPTION "Firmware for the AudoMark Self-Hosted Benchmark"
	LANGUAGES C CXX
	VERSION 1.0.0)

# Keep baseline flags configurable so ports/toolchains can add their own tuning
# without editing this top-level file.
set(AUDIOMARK_C_FLAGS "-O3" CACHE STRING "Base C compiler flags")
set(AUDIOMARK_CXX_FLAGS "-O3" CACHE STRING "Base C++ compiler flags")
set(AUDIOMARK_EXTRA_COMPILE_FLAGS "" CACHE STRING "Extra compiler flags applied to C and C++ sources")
set(AUDIOMARK_EXTRA_C_FLAGS "" CACHE STRING "Extra compiler flags applied only to C sources")
set(AUDIOMARK_EXTRA_CXX_FLAGS "" CACHE STRING "Extra compiler flags applied only to C++ sources")
option(AUDIOMARK_STATIC_LINK "Link generated executables statically" OFF)

# TensorFlow Lite/Abseil need a consistent C++ standard across their targets.
set(CMAKE_C_FLAGS "${AUDIOMARK_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${AUDIOMARK_CXX_FLAGS}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Propagate C++ standard requirements through Abseil targets" FORCE)

# Ask dependencies that honor BUILD_SHARED_LIBS to prefer static libraries.
if(AUDIOMARK_STATIC_LINK)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
endif()

# enable custom CPU tuning
if(CPU)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=${CPU}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=${CPU}")
endif()

if(AUDIOMARK_EXTRA_COMPILE_FLAGS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${AUDIOMARK_EXTRA_COMPILE_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AUDIOMARK_EXTRA_COMPILE_FLAGS}")
endif()

if(AUDIOMARK_EXTRA_C_FLAGS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${AUDIOMARK_EXTRA_C_FLAGS}")
endif()

if(AUDIOMARK_EXTRA_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AUDIOMARK_EXTRA_CXX_FLAGS}")
endif()

include_directories(lib/speexdsp)
include_directories(lib/speexdsp/include)
include_directories(lib/speexdsp/include/speex)
include_directories(lib/speexdsp/libspeexdsp)
include_directories(src)

if(DEFINED PORT_DIR)
	include(${PORT_DIR}/port.cmake)
else()
	message(FATAL_ERROR "Please define PORT_DIR, e.g. -DPORT_DIR=<path>")
endif()

# external libraries
list(APPEND EXTRA_LIBS
	m)

# For Xiph we are using Float, never Fixed
add_definitions(-DFLOATING_POINT)
# CMSIS DSP FFT tables size optimization
add_definitions(-DREDUCED_FFT_TABSZ)
# This macro is used extensively in LibSpeeX for function visibility.
add_definitions(-DEXPORT=)
# Benchmark system integrator provides heap
add_definitions(-DOVERRIDE_SPEEX_ALLOC)
add_definitions(-DOS_SUPPORT_CUSTOM)
add_definitions(-D__RESTRICT=)

set(EE_SOURCE
	# Note: main.c is just for self-hosted testing
	main.c
	src/ee_audiomark.c

	src/ee_aec_f32.c
	src/ee_anr_f32.c
	src/ee_abf_f32.c
	src/ee_abf_f32_tables.c
	src/ee_kws.c
	src/ee_mfcc_f32.c
	src/ee_mfcc_f32_tables.c

	lib/speexdsp/libspeexdsp/buffer.c
	lib/speexdsp/libspeexdsp/fftwrap.c
	lib/speexdsp/libspeexdsp/filterbank.c
	lib/speexdsp/libspeexdsp/jitter.c
	lib/speexdsp/libspeexdsp/kiss_fft.c
	lib/speexdsp/libspeexdsp/kiss_fftr.c
	lib/speexdsp/libspeexdsp/mdf.c
	lib/speexdsp/libspeexdsp/preprocess.c
	lib/speexdsp/libspeexdsp/resample.c
	lib/speexdsp/libspeexdsp/scal.c
	lib/speexdsp/libspeexdsp/smallft.c
)

set(TH_SOURCE
	# This is essentially an include needed in th_api.c
	src/ee_nn_tables.c
	src/ee_abf_f32_tables.c

	${PORT_SOURCE}
)

set(TEST_AEC_F32_SOURCE
	src/ee_aec_f32.c
	lib/speexdsp/libspeexdsp/buffer.c
	lib/speexdsp/libspeexdsp/fftwrap.c
	lib/speexdsp/libspeexdsp/filterbank.c
	lib/speexdsp/libspeexdsp/jitter.c
	lib/speexdsp/libspeexdsp/kiss_fft.c
	lib/speexdsp/libspeexdsp/kiss_fftr.c
	lib/speexdsp/libspeexdsp/mdf.c
	lib/speexdsp/libspeexdsp/preprocess.c
	lib/speexdsp/libspeexdsp/resample.c
	lib/speexdsp/libspeexdsp/scal.c
	lib/speexdsp/libspeexdsp/smallft.c
	${TH_SOURCE}
	tests/data/aec_f32_input_source.c
	tests/data/aec_f32_input_echo.c
	tests/data/aec_f32_expected.c
	tests/test_aec_f32.c
)
add_executable(test_aec_f32 ${TEST_AEC_F32_SOURCE})

set(TEST_ANR_F32_SOURCE
	src/ee_anr_f32.c
	lib/speexdsp/libspeexdsp/buffer.c
	lib/speexdsp/libspeexdsp/fftwrap.c
	lib/speexdsp/libspeexdsp/filterbank.c
	lib/speexdsp/libspeexdsp/jitter.c
	lib/speexdsp/libspeexdsp/kiss_fft.c
	lib/speexdsp/libspeexdsp/kiss_fftr.c
	lib/speexdsp/libspeexdsp/mdf.c
	lib/speexdsp/libspeexdsp/preprocess.c
	lib/speexdsp/libspeexdsp/resample.c
	lib/speexdsp/libspeexdsp/scal.c
	lib/speexdsp/libspeexdsp/smallft.c
	${TH_SOURCE}
	tests/data/anr_f32_input.c
	tests/data/anr_f32_expected.c
	tests/test_anr_f32.c
)
add_executable(test_anr_f32 ${TEST_ANR_F32_SOURCE})


set(TEST_ABF_F32_SOURCE
	src/ee_abf_f32.c
	${TH_SOURCE}
	tests/data/abf_f32_input_ch1.c
	tests/data/abf_f32_input_ch2.c
	tests/data/abf_f32_expected.c
	tests/test_abf_f32.c
)
add_executable(test_abf_f32 ${TEST_ABF_F32_SOURCE})

set(TEST_MFCC_F32_SOURCE
	src/ee_mfcc_f32.c
	src/ee_mfcc_f32_tables.c
	${TH_SOURCE}
	tests/data/mfcc_f32_all.c
	tests/test_mfcc_f32.c
)
add_executable(test_mfcc_f32 ${TEST_MFCC_F32_SOURCE})

set(TEST_KWS_SOURCE
	src/ee_kws.c
	src/ee_mfcc_f32.c
	src/ee_mfcc_f32_tables.c
	${TH_SOURCE}
	tests/test_kws.c
	tests/data/kws_input.c
	tests/data/kws_expected.c
)
add_executable(test_kws ${TEST_KWS_SOURCE})

add_executable(audiomark ${EE_SOURCE} ${TH_SOURCE})

# Ports can attach target-specific sources and options without hardcoding their
# implementation details here.
if(PORT_AUDIOMARK_SOURCE)
	target_sources(audiomark PRIVATE ${PORT_AUDIOMARK_SOURCE})
endif()

if(PORT_AUDIOMARK_LINK_OPTIONS)
	target_link_options(audiomark PRIVATE ${PORT_AUDIOMARK_LINK_OPTIONS})
endif()

if(PORT_AUDIOMARK_COMPILE_DEFINITIONS)
	target_compile_definitions(audiomark PRIVATE ${PORT_AUDIOMARK_COMPILE_DEFINITIONS})
endif()

set(ALL_TARGETS
	audiomark
	test_kws
	test_abf_f32
	test_aec_f32
	test_anr_f32
	test_mfcc_f32
)

if(USE_ARMNN)
	foreach(tgt IN LISTS ALL_TARGETS)
		if(TARGET ${tgt})
			add_dependencies(${tgt} armnn_external)
		endif()
	endforeach()
endif()

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
else()
	foreach(tgt IN LISTS ALL_TARGETS)
		if(TARGET ${tgt})
			# Keep static linking opt-in because not every platform/toolchain can
			# provide fully static system and C++ runtime libraries.
			if(AUDIOMARK_STATIC_LINK)
				set_target_properties(${tgt} PROPERTIES
					LINK_SEARCH_START_STATIC ON
					LINK_SEARCH_END_STATIC ON
				)
				target_link_options(${tgt} PRIVATE
					-static
					-static-libgcc
				)
			endif()
			target_link_libraries(${tgt} ${EXTRA_LIBS})
		endif()
	endforeach()
endif()

add_custom_target(test
	COMMENT "Running unit tests..."
	COMMAND ./test_abf_f32
	COMMAND ./test_aec_f32
	COMMAND ./test_anr_f32
	COMMAND ./test_kws
	COMMAND ./test_mfcc_f32
	DEPENDS test_mfcc_f32 test_kws test_abf_f32 test_aec_f32 test_anr_f32
)
