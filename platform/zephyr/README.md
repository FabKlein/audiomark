# AudioMark Zephyr Platform

This Zephyr platform builds AudioMark as a Zephyr application for:

- Arm Corstone-320 FVP, Cortex-M85 with Ethos-U85.
- Arm Corstone-1000 A320 FVP, Cortex-A320 with Ethos-U85.

It also builds the AudioMark unit-test applications:

- `CONFIG_AUDIOMARK_TEST_KWS=y`
- `CONFIG_AUDIOMARK_TEST_MFCC=y`
- `CONFIG_AUDIOMARK_TEST_AEC=y`
- `CONFIG_AUDIOMARK_TEST_ANR=y`
- `CONFIG_AUDIOMARK_TEST_ABF=y`

The default variant is the full AudioMark benchmark application.

## Prerequisites

Use an initialized Zephyr workspace containing this AudioMark tree and the
required Zephyr modules:

```shell
cd <zephyr-workspace>
source <venv>/bin/activate
west update
```

The examples below assume:

- Zephyr SDK is installed and discoverable by Zephyr, or exported with
  `ZEPHYR_SDK_INSTALL_DIR`.
- The Corstone-320 FVP binary is available as `FVP_Corstone_SSE-320`.
- The Corstone-1000 A320 FVP binary is available as `FVP_Corstone-1000-A320`.
- `sgdisk` from the `gdisk` package is installed for Corstone-1000 sysbuild
  firmware image generation.

Example environment:

```shell
export ZEPHYR_SDK_INSTALL_DIR=<path-to-zephyr-sdk>
export C320_FVP=<path-to>/FVP_Corstone_SSE-320
export C1000_FVP_DIR=<path-to-directory-containing-FVP_Corstone-1000-A320>
```

For the Corstone-1000 A320 FVP, `C1000_FVP_DIR` must be the directory
containing the executable, not the executable path itself.

## Corstone-320 Cortex-M85 + Ethos-U85

Build the benchmark application:

```shell
cd <zephyr-workspace>

west build -p always -d build-audiomark-c320-m85 \
    -b mps4/corstone320/fvp applications/audiomark/platform/zephyr \
    -- -DARMFVP="${C320_FVP}"
```

Run it:

```shell
west build -d build-audiomark-c320-m85 -t run
```

The Zephyr board configuration routes UART output to stdout and sets
`mps4_board.uart0.shutdown_on_eot=1`. Unit-test applications emit EOT when the
test completes, so the FVP exits automatically.

## Corstone-1000 Cortex-A320 + Ethos-U85

Corstone-1000 A320 uses Zephyr sysbuild to produce the firmware images loaded
by the FVP:

- `build-audiomark-c1000-a320/firmware/bl1.bin`
- `build-audiomark-c1000-a320/firmware/cs1000.bin`

Build the benchmark application:

```shell
cd <zephyr-workspace>
export ARMFVP_BIN_PATH="${C1000_FVP_DIR}"

west build -p always -d build-audiomark-c1000-a320 \
    -b fvp_corstone1000/a320 applications/audiomark/platform/zephyr \
    --sysbuild
```

Run it:

```shell
west build -d build-audiomark-c1000-a320 -t run
```

The Corstone-1000 board run target loads `bl1.bin` and `cs1000.bin` into the
FVP. It routes host UART output to stdout and sets
`host.uart0.shutdown_on_eot=1`.

## Unit Tests

Each unit test is selected with one Kconfig option. Use a separate build
directory per test to keep artifacts intact.

Corstone-320 MFCC example:

```shell
west build -p always -d build-audiomark-mfcc-c320-m85 \
    -b mps4/corstone320/fvp applications/audiomark/platform/zephyr \
    -- -DARMFVP="${C320_FVP}" \
       -DCONFIG_AUDIOMARK_TEST_MFCC=y

west build -d build-audiomark-mfcc-c320-m85 -t run
```

Corstone-320 AEC example:

```shell
west build -p always -d build-audiomark-aec-c320-m85 \
    -b mps4/corstone320/fvp applications/audiomark/platform/zephyr \
    -- -DARMFVP="${C320_FVP}" \
       -DCONFIG_AUDIOMARK_TEST_AEC=y

west build -d build-audiomark-aec-c320-m85 -t run
```

Corstone-1000 KWS example:

```shell
export ARMFVP_BIN_PATH="${C1000_FVP_DIR}"

west build -p always -d build-audiomark-kws-c1000-a320 \
    -b fvp_corstone1000/a320 applications/audiomark/platform/zephyr \
    --sysbuild \
    -- -DCONFIG_AUDIOMARK_TEST_KWS=y

west build -d build-audiomark-kws-c1000-a320 -t run
```

The same pattern works for:

```shell
-DCONFIG_AUDIOMARK_TEST_KWS=y
-DCONFIG_AUDIOMARK_TEST_MFCC=y
-DCONFIG_AUDIOMARK_TEST_AEC=y
-DCONFIG_AUDIOMARK_TEST_ANR=y
-DCONFIG_AUDIOMARK_TEST_ABF=y
```

KWS uses TFLite Micro and Ethos-U. MFCC, AEC, ANR, and ABF are DSP-oriented
unit tests and do not require the TFLite Micro runtime.

## Model Selection

By default, the Zephyr platform embeds the U85-256 Vela output for both
Corstone targets:

```text
ports/arm/ds_cnn_s_quantized_U85_256_vela.tflite.cpp
```

Override the embedded model source with `AUDIOMARK_MODEL_SOURCE`:

```shell
west build -p always -d build-audiomark-c320-custom-model \
    -b mps4/corstone320/fvp applications/audiomark/platform/zephyr \
    -- -DARMFVP="${C320_FVP}" \
       -DAUDIOMARK_MODEL_SOURCE=<path-to-model>.tflite.cpp
```

The model source may be absolute or relative to the AudioMark repository root.

## Useful Output Locations

Corstone-320 benchmark ELF:

```text
build-audiomark-c320-m85/zephyr/zephyr.elf
```

Corstone-1000 A320 application ELF:

```text
build-audiomark-c1000-a320/audiomark_zephyr/zephyr/zephyr.elf
```

Corstone-1000 A320 FVP firmware images:

```text
build-audiomark-c1000-a320/firmware/bl1.bin
build-audiomark-c1000-a320/firmware/cs1000.bin
```

## Troubleshooting

If `west build -t run` fails with `ARMFVP-NOTFOUND`, check the FVP path:

- For Corstone-320, pass the executable path with `-DARMFVP=...`.
- For Corstone-1000 A320, export `ARMFVP_BIN_PATH` to the directory containing
  `FVP_Corstone-1000-A320`.

If a non-KWS unit test prints Kconfig warnings about disabled TFLite Micro or
Ethos-U symbols, the build can still be valid. The pure DSP unit tests do not
select TFLite Micro or Ethos-U.

If a previous build directory was configured for another board or variant, use
`-p always` or choose a new `-d <build-dir>`.
