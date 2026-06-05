# Arm Cortex-A AArch64 Build

This port is intended for self-hosted Cortex-A builds using the Arm port layer in
`ports/arm`.

## Fetch Submodules

Run this once from the repository root:

```sh
git submodule update --init --recursive
```

## Configure

Example Cortex-A320 TensorFlow Lite build with Arm Clang, static linking, and
the Omax configuration:

```sh
cmake -S . -B build_a320 -G "Unix Makefiles" \
  -DCPU=cortex-a320 \
  -DPORT_DIR=ports/arm \
  -DUSE_TFL=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/mnt/data/fabkle01/toolchains/ATfE-22.1.0-Linux-x86_64/bin/clang \
  -DCMAKE_CXX_COMPILER=/mnt/data/fabkle01/toolchains/ATfE-22.1.0-Linux-x86_64/bin/clang \
  -DAUDIOMARK_EXTRA_COMPILE_FLAGS=--config=/mnt/data/fabkle01/toolchains/ATfE-22.1.0-Linux-x86_64/bin/Omax.cfg \
  -DAUDIOMARK_STATIC_LINK=ON
```

Build the benchmark:

```sh
cmake --build build_a320 --target audiomark --parallel 2
```

Run:

```sh
./build_a320/audiomark
```

## Private Component Profiling

The Arm port can build a private profiling variant without modifying
`src/ee_audiomark.c`. It uses linker wrapping around the component entry points:

- `ee_abf_f32`
- `ee_aec_f32`
- `ee_anr_f32`
- `ee_kws_f32`

Enable profiling with architectural counter ticks:

```sh
cmake -S . -B build_a320 \
  -DAUDIOMARK_ARM_PROFILE=ON \
  -DAUDIOMARK_ARM_PROFILE_COUNTER=arch \
  -DAUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES=ON
cmake --build build_a320 --target audiomark --parallel 2
```

Enable profiling with Linux nanosecond timing and convert to cycles using a
2 GHz core frequency:

```sh
cmake -S . -B build_a320 \
  -DAUDIOMARK_ARM_PROFILE=ON \
  -DAUDIOMARK_ARM_PROFILE_COUNTER=linux_ns \
  -DAUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ=2000000000 \
  -DAUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES=ON
cmake --build build_a320 --target audiomark --parallel 2
```

With profiling enabled, the benchmark prints:

- Per-iteration timing table for `ABF`, `AEC`, `ANR`, `KWS`, and `TFLite Invoke`.
- Summary table with calls, totals, and averages.
- `AudioMark/MHz` estimate.

For `linux_ns`, the raw table is printed in nanoseconds. `AudioMark/MHz` is only
computed when `AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ` is set, because the formula
requires cycle-like values.

When `AUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES=ON`, the KWS wrapper exits after
`AUDIO_COUNTER_MAX_ITERATIONS` samples. This is for private platform testing,
not official benchmark scoring.

## Clean

Rebuild from scratch:

```sh
rm -rf build_a320
```

Clean generated files inside an existing build directory:

```sh
cmake --build build_a320 --target clean
```

## CMake Options

| Option | Default | Description |
| --- | --- | --- |
| `PORT_DIR` | required | Must be set to `ports/arm`. |
| `CPU` | empty | Appended as `-mcpu=<CPU>` to C and C++ flags. Example: `cortex-a320`. |
| `USE_TFL` | `OFF` | Enable TensorFlow Lite backend. Recommended for Cortex-A Linux testing. |
| `USE_ARMNN` | `OFF` | Enable Arm NN backend. Mutually exclusive with `USE_TFL` and `USE_IMX93`. |
| `USE_IMX93` | `OFF` | Enable NXP i.MX93 TFLite/Ethos-U path. Mutually exclusive with other NN backends. |
| `USE_CMSISDSP_NEON` | `OFF` | Enable CMSIS-DSP Neon support on Cortex-A builds. |
| `AUDIOMARK_C_FLAGS` | `-O3` | Base C compiler flags. |
| `AUDIOMARK_CXX_FLAGS` | `-O3` | Base C++ compiler flags. |
| `AUDIOMARK_EXTRA_COMPILE_FLAGS` | empty | Extra flags added to both C and C++ compilation. Useful for `Omax.cfg`. |
| `AUDIOMARK_EXTRA_C_FLAGS` | empty | Extra flags added only to C compilation. |
| `AUDIOMARK_EXTRA_CXX_FLAGS` | empty | Extra flags added only to C++ compilation. |
| `AUDIOMARK_STATIC_LINK` | `OFF` | Request static executable linking and static dependency builds where supported. |
| `AUDIOMARK_ARM_PROFILE` | `OFF` | Enable private Arm-port component timing wrappers. |
| `AUDIOMARK_ARM_PROFILE_EXIT_AFTER_SAMPLES` | `ON` | Exit after collecting the profiling sample window. Only used when profiling is enabled. |
| `AUDIOMARK_ARM_PROFILE_COUNTER` | `arch` | Profiling counter backend. Valid values: `arch`, `linux_ns`. |
| `AUDIOMARK_ARM_PROFILE_CORE_FREQ_HZ` | empty | Core frequency used to convert time-based samples to cycles for `AudioMark/MHz`. Example: `2000000000`. |

## Notes

- The `arch` counter uses `CNTVCT_EL0` on AArch64.
- The `linux_ns` counter uses `clock_gettime(CLOCK_MONOTONIC_RAW)`.
- The TensorFlow Lite invoke timing uses the same counter backend as the wrapper
  profiler, so table columns stay in the same unit.
- Linker wrapping uses `-Wl,--wrap=<symbol>`, which is supported by GNU ld, gold,
  and lld. Disable LTO if wrapped calls are ever optimized away.
