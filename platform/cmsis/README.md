# README

- How to build and run EEMBC Audiomark applications on Arm Corstone-300/310 MPS3 FPGA, Corstone-315/320 FVP, Corstone-320 MPS4 FPGA, IoT kit (CM33 SSE-200), Cortex-M CMSDK, or Arm Virtual Hardware.
  - The applications are intended to run on Cortex-M55/Cortex-M85 MCUs supporting Helium™ and Arm V7M-E/Arm V8.0M cores. FPU is required.
  - There are dedicated projects for running the KWS on Ethos-U NPUs.
  - ARM FPGA images and documentation can be found at https://developer.arm.com/downloads/-/download-fpga-images.
    - `AN552`: Arm® Corstone™ SSE-300 with Cortex®-M55 and Ethos™-U55 Example Subsystem for MPS3 (Partial Reconfiguration Design)
    - `AN555`: Arm® Corstone™ SSE-310 with Cortex®-M85 and Ethos™-U55 Example Subsystem for MPS3
    - `AN109762`: Arm® Corstone™ SSE-320 with Cortex®-M85 and Ethos™-U85 Example Subsystem for MPS4
    - `AN505`: Arm® Cortex™-M33 with IoT kit FPGA for MPS2+
    - `AN386`, `AN500`: Arm® Cortex™-M4 / Arm® Cortex™-M7 Prototyping System version 3.1 (VEM31)
  - Arm® Corstone™ SSE-315 FVP is supported. No public FPGA image is available for now.
  - Arm® Corstone™ SSE-320 FVP is supported, and the Ethos-U85 Corstone-320 FPGA AudioMark application is supported using the MPS4 FPGA image.

## CMSIS Build tools option

If only considering building the benchmarks with Keil MDK without CMSIS Toolbox, following paragraph can be skipped and move directly [here](#keil-mdk-builds).

See description about CMSIS Toolbox here: https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/installation.md


If not installed, decompress cmsis-toolbox in your workspace and setup environment variables as described in the link above.
CMSIS Toolbox **v2.3.0** or above is required. The current CMSIS build flow has been checked with CMSIS Toolbox **v2.13.0**.

```shell
tar -zxvf cmsis-toolbox-linux64.tar.gz
PATH=$PATH:<your_cmsis_tool_path>/cmsis-toolbox-linux64/bin/

# these are optional, overriding default variables
export CMSIS_COMPILER_ROOT=<your_cmsis_tool_path>/cmsis-toolbox-linux64/etc/
export CMSIS_PACK_ROOT=<your_cmsis_pack_storage_path>cmsis-pack
```


If not already installed, download **Arm Compiler 6.18** or later. It is recommended to use up to date Arm Compiler 6 releases.
Support for other toolchains like CLANG-17 or GCC-13 and above are experimental. IAR support will be added later.

**GCC 15 warning:** avoid GCC 15 for Helium/MVE AudioMark builds for now. GCC 15.2 miscompiles the CMSIS-DSP MVE CFFT kernel `_arm_radix4_butterfly_f32_mve()` by using a stale gather-load base after a writeback gather load. This corrupts the FFT path at runtime. Use Arm Compiler 6 or LLVM/Clang for MVE targets until the GCC code generation issue is fixed. Non-MVE GCC builds are not affected by this specific issue.

Following the CMSIS Toolbox installation steps, compiler and version must be registered  by defining an according environment variable e.g.

```shell
export AC6_TOOLCHAIN_6_22_0=<path_to_ac6_22_compiler>/bin/
export GCC_TOOLCHAIN_13_2_1=<path_to_gcc_13_compiler>/bin/
export CLANG_TOOLCHAIN_17_0_1=<path_to_llvm17_compiler>/bin/
```

### Initialize the new pack repository

If you did not have a local CMSIS-PACK repository, create a directory and then execute the following command in that directory:
```
cpackget init https://www.keil.com/pack/index.pidx
```
The CMSIS_PACK_ROOT variable should also point to this directory.

### Build the project

#### Generate list of needed packs

From this `audiomark/platform/cmsis` folder, type the command:

```
csolution list packs -s audiomark.csolution.yml -m > required_packs.txt
```



#### Install the packs

```
cpackget add -f required_packs.txt
```


#### Projects generation (optional)

```
csolution convert -s audiomark.csolution.yml -t AC6
```

This  will generate several project files for each audiomark application:
 * *audiomark_app* : the main audiomark application
 * *testabf* : the beamformer application
 * *testaec* : the echo canceller application
 * *testanr* : the noise reductor application
 * *testkws* : the key word spotting application
 * *testmfcc* : the MFCC unit test application

For each target: Corstone-300/310 MPS3, Corstone-315/320 FVP, Corstone-320 MPS4 FPGA, CM4, CM7, and CM33 MPS2.

Expected output:

```
audiomark/platform/cmsis/testanr/testanr.Release+MPS3-Corstone-300.cprj - info csolution: file generated successfully
audiomark/platform/cmsis/testanr/testanr.Release+MPS3-Corstone-310.cprj - info csolution: file generated successfully
audiomark/platform/cmsis/testabf/testabf.Release+MPS3-Corstone-300.cprj - info csolution: file generated successfully
audiomark/platform/cmsis/testabf/testabf.Release+MPS3-Corstone-310.cprj - info csolution: file generated successfully
...
<full list elided>
...
```

This also generates projects for running the AudioMark application with Arm Ethos-U acceleration.

```
audiomark/platform/cmsis/audiomark_app/audiomark_app.Release+Ethos-MPS3-Corstone-300.cprj
audiomark/platform/cmsis/audiomark_app/audiomark_app.Release+Ethos-MPS3-Corstone-310.cprj
audiomark/platform/cmsis/audiomark_app/audiomark_app.Release+Ethos-Corstone-315.cprj
audiomark/platform/cmsis/audiomark_app/audiomark_app.Release+Ethos-MPS4-Corstone-320-FVP.cprj
audiomark/platform/cmsis/audiomark_app/audiomark_app.Release+Ethos-MPS4-Corstone-320-FPGA.cprj
```


#### Build

To build the different projects, use the **cbuild** command:
e.g. for the C300 audiomark application:

```
cbuild --context audiomark_app.Release+MPS3-Corstone-300 audiomark.csolution.yml --update-rte -v --toolchain AC6
```

This will generate an object in `audiomark/platform/cmsis/out/audiomark_app/MPS3-Corstone-300/Release/audiomark_app.axf`

Similarly GCC and CLANG generated binaries can be produced by replacing the toolchain parameter with GCC@x.y.z or CLANG@x.y.z where x.y.z are compiler versions.

Expected output:
```
>> cbuild --context audiomark_app.Release+MPS3-Corstone-300 audiomark.csolution.yml --update-rte -v --toolchain AC6
info cbuild: Build Invocation 2.13.0 (C) 2022-2026 Arm Ltd. and Contributors
info csolution: config files for each component:
  ARM::Device:Definition@1.2.0:
    - git/forks/audiomark/platform/cmsis/audiomark_app/../RTE/Device/SSE-300-MPS3/platform_base_address.h (base@1.1.2)
  ARM::Device:Startup&Baremetal@1.2.0:

...
-- Configuring done
-- Generating done
-- Build files have been written to: git/forks/audiomark/platform/cmsis/tmp/audiomark_app/MPS3-Corstone-300/Release
[1/1] Linking C executable git/forks/audiomark/platform/cmsis/out/audiomark_app/MPS3-Corstone-300/Release/audiomark_app.axf
info cbuild: build finished successfully!

```

### Running on Virtual Hardware

MPS3 Corstone-300/310 FPGA images can be run with Arm Virtual Hardware. Corstone-315 and Corstone-320 are also available as FVP targets. For more information, please refer to https://www.arm.com/products/development-tools/simulation/virtual-hardware.

With this configuration, UART output goes through a telnet terminal.

Older project files used `VHT`-prefixed target names. Current CMSIS contexts use the board/FVP names directly, for example `MPS3-Corstone-300`, `Ethos-MPS3-Corstone-300`, `Corstone-315`, and `Ethos-MPS4-Corstone-320-FVP`.


To start simulation, issue following command from audiomark/platform/cmsis folder

```
>> VHT_MPS3_Corstone_SSE-300 out/audiomark_app/MPS3-Corstone-300/Release/audiomark_app.axf -f model_config_sse300.txt  --stat
telnetterminal1: Listening for serial connection on port 5000
telnetterminal2: Listening for serial connection on port 5001

```

**Note**
- Virtual Hardware simulation _does not provide cycle accurate measurements_ hence final audiomark score will differ from the one measured on device.


## Keil MDK Builds

The Audiomark Corstone-300 FPGA main application can be built by importing the **audiomark_app/audiomark_app.Release+MPS3-Corstone-300.cprj** in uVision (Project=>Import)

- The printf messages are output to MPS3 FPGA board 2nd UART port (settings `115200, 8,N,1`) by project default setting.
- To change printf messages to Debug (printf) Viewer window using the tracing method through JTAG, please select ITM for STDOUT under Compiler => I/O in Manage Run-Time Environment menu.

Various individual audiomark components unit-tests project can imported using the different cprojects files provided in this folder (e.g. *testanr.Release+MPS3-Corstone-300.cprj* for Noise suppressor Corstone-300 Unit test)

For Corstone-310 FPGA, similar steps can be followed by importing **audiomark_app/audiomark_app.Release+MPS3-Corstone-310.cprj** and / or different unit tests

For Arm V7M-E/ Arm V8.0M MPS2+ FPGA, similar steps can be followed by importing **audiomark_app/audiomark_app.Release+MPS2-IOTKit-CM33.cprj**, **audiomark_app/audiomark_app.Release+MPS2-CMSDK_CM7_SP.cprj**, **audiomark_app/audiomark_app.Release+MPS2-CMSDK_CM4_FP.cprj** and / or different unit tests

For Virtual Hardware AudioMark components, import the generated `.cprj` matching the target context you want to run.

## Arm Ethos-U KWS acceleration

Audiomark KWS can be significantly accelerated by offloading the neural network processing to the Arm Ethos-U NPU engine.
For the DS-CNN model reference is converted back to TensorFlow Lite (TFL) format and processed by VELA which is the tool used to compile a TFL neural network model into an optimized version that can run on an embedded system containing an Arm Ethos-U NPU.
Converted binary models are packed in cpp files that are located in `ports/arm/` folder. There is one file for each Ethos-U variant based on number of MACs.
 - ds_cnn_s_quantized_U55_32_vela.tflite.cpp
 - ds_cnn_s_quantized_U55_64_vela.tflite.cpp
 - ds_cnn_s_quantized_U55_128_vela.tflite.cpp
 - ds_cnn_s_quantized_U55_256_vela.tflite.cpp
 - ds_cnn_s_quantized_U65_256_vela.tflite.cpp
 - ds_cnn_s_quantized_U85_256_sram_only_vela.tflite.cpp
 - ds_cnn_s_quantized_U85_1024_sram_only_vela.tflite.cpp

ARM Corstone-300 FPGA implements Ethos-U55-128 while Corstone-310 implements Ethos-U55-256.
ARM Corstone-315 FVP implements Ethos-U65-256 or Ethos-U65-512.
ARM Corstone-320 FVP uses Ethos-U85-256 while the MPS4 Corstone-320 FPGA image uses Ethos-U85-1024.
Bit exactness is not affected by Ethos-U offloading.

- *Note* : Corstone-320 FPGA support currently reuses the SSE-320 FVP CMSIS pack and applies local RTE header overrides for FPGA-specific addresses and interrupts. Running `cbuild --update-rte` may refresh these generated RTE files, so check the local overrides afterwards. This should be removed once a proper SSE-320 FPGA CMSIS pack is available.

- *Note* : `ds_cnn_s_quantized.tflite.cpp` is the MCU only variant that can be substituted to original Audiomark model.

The Corstone-320 U85 models were generated with Vela using the `Sram_Only` memory mode so constants and activation/scratch tensors are placed in SRAM. Example commands:

```shell
# Corstone-320 FVP: Ethos-U85-256
vela ds_cnn_s_quantized.tflite \
  --accelerator-config=ethos-u85-256 \
  --optimise Performance \
  --memory-mode=Sram_Only \
  --system-config=Ethos_U85_SYS_DRAM_Low \
  --output-dir vela_u85_256_sram_only
cp vela_u85_256_sram_only/ds_cnn_s_quantized_vela.tflite \
  ds_cnn_s_quantized_U85_256_sram_only_vela.tflite

# Corstone-320 FPGA: Ethos-U85-1024
vela ds_cnn_s_quantized.tflite \
  --accelerator-config=ethos-u85-1024 \
  --optimise Performance \
  --memory-mode=Sram_Only \
  --system-config=Ethos_U85_SYS_DRAM_Mid_1024 \
  --output-dir vela_u85_1024_sram_only
cp vela_u85_1024_sram_only/ds_cnn_s_quantized_vela.tflite \
  ds_cnn_s_quantized_U85_1024_sram_only_vela.tflite
```

Building process is similar to ARM Cortex-M only builds.

```shell
# Corstone-300 FPGA build
cbuild --context audiomark_app.Release+Ethos-MPS3-Corstone-300 audiomark.csolution.yml --update-rte --toolchain AC6

# Corstone-310 FPGA build
cbuild --context audiomark_app.Release+Ethos-MPS3-Corstone-310 audiomark.csolution.yml --update-rte --toolchain AC6

# Corstone-320 FVP build
cbuild --context audiomark_app.Release+Ethos-MPS4-Corstone-320-FVP audiomark.csolution.yml --update-rte --toolchain AC6

# Corstone-320 FPGA build
cbuild --context audiomark_app.Release+Ethos-MPS4-Corstone-320-FPGA audiomark.csolution.yml --update-rte --toolchain AC6
```

Binaries can be run on AVH, but as for Cortex-M only variants, results will be different from FPGA or physical target because not cycle-accurate.


```
>> VHT_Corstone_SSE-300_Ethos-U55 audiomark_app.axf -f model_config_sse300.txt -C mps3_board.uart0.out_file=-
telnetterminal0: Listening for serial connection on port 5000
telnetterminal1: Listening for serial connection on port 5001
telnetterminal2: Listening for serial connection on port 5002
telnetterminal5: Listening for serial connection on port 5003

    Ethos-U rev 136b7d75 --- Apr 12 2023 13:44:01
    (C) COPYRIGHT 2019-2023 Arm Limited
    ALL RIGHTS RESERVED

Warning: Failed to write bytes at address range [0x00080000..0x002B2B07] when loading image "audiomark/platform/cmsis/out/audiomark_app/Ethos-MPS3-Corstone-300/Release/audiomark_app.axf".
Initializing
Memory alloc summary:
 bmf = 14948
 aec = 68100
 anr = 45250
 kws = 8308
INFO - Ethos-U device initialised
INFO - Ethos-U version info:
INFO - 	Arch:       v1.1.0
INFO - 	Driver:     v0.16.0
INFO - 	MACs/cc:    128
INFO - 	Cmd stream: v0
Added ethos-u support to op resolver
INFO - Creating allocator using tensor arena at 0x31000000
INFO - Allocating tensors
INFO - Model INPUT tensors:
INFO - 	tensor type is INT8
INFO - 	tensor occupies 490 bytes with dimensions
INFO - 		0:   1
INFO - 		1: 490
INFO - Quant dimension: 0
INFO - Scale[0] = 1.084193
INFO - ZeroPoint[0] = 100
INFO - Model OUTPUT tensors:
INFO - 	tensor type is INT8
INFO - 	tensor occupies 12 bytes with dimensions
INFO - 		0:   1
INFO - 		1:  12
INFO - Quant dimension: 0
INFO - Scale[0] = 0.003906
INFO - ZeroPoint[0] = -128
INFO - Activation buffer (a.k.a tensor arena) size used: 22548
INFO - Number of operators: 1
INFO - 	Operator 0: ethos-u
Computing run speed
[warning ][main@0][2463 ns] TA0 is not enabled!
[warning ][main@0][2463 ns] TA1 is not enabled!
Measuring
Total runtime    : 10.974 seconds
Total iterations : 10 iterations
Score            : 607.506470 AudioMarks
```


## Important Notes

 - For Corstone-300, Audiomark Code and Data fit entirely in I/D TCM. MPS3 FPGA system clock frequency runs at `32Mhz`
 - For Corstone-310, small TCMs prevent Code and Data to fit in these. Internal SRAM are used and benchmarks will run with caches enabled. MPS3 FPGA system clock frequency runs at `25Mhz`
 - For Corstone-320 FVP and MPS4 FPGA, the Cortex-M85 CPU and Ethos-U85 NPU run at `50Mhz` in the current configuration. The FPGA target uses the SSE-320 FVP CMSIS pack with local address and interrupt overrides until a dedicated FPGA pack is available.
 - For MPS2+ Cortex-M33 IoTKit, default system clock frequency runs at `20Mhz`
 - For MPS2+ Cortex-M4/Cortex-M7, CMSDK default system clock frequency runs at `25Mhz`
 - For platforms utilizing the Ethos-U NPU and running the AudioMark DS-CNN network, it is assumed that, aside from the storage for Neural Network MFCC input and Soft Decisions output - which may be situated in cacheable memories necessitating cache maintenance operations - the rest of the shared Cortex-M and Ethos-U data are **Read-Only**. Consequently, these do not require cache clean and invalidation operations, allowing saving processing cycles. However, in the standard Ethos-U handling procedures, this approach may not be recommended. The TensorFlow Lite Micro runtime could organize and manage memory allocation in a different manner, involving write-accesses by both CPU and NPU, necessitating the use of appropriate cache operations to ensure data coherency between the CPU and NPU. For reference, typical cache and invalidation routines employed in general scenarios can be found in the ML Evaluation Kit CPU cache handling source code.
    - https://review.mlplatform.org/plugins/gitiles/ml/ethos-u/ml-embedded-evaluation-kit/+/refs/heads/main/source/hal/source/components/npu/ethosu_cpu_cache.c
