
# Required for cross-compiling
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Compilers
#set(CMAKE_C_COMPILER /arm/tools/arm/arm-gnu-toolchain-aarch64-none-linux-gnu/13.3.rel1/linux64/bin/aarch64-none-linux-gnu-gcc)
#set(CMAKE_CXX_COMPILER /arm/tools/arm/arm-gnu-toolchain-aarch64-none-linux-gnu/13.3.rel1/linux64/bin/aarch64-none-linux-gnu-g++)

# Optional: Add sysroot if needed
# set(CMAKE_SYSROOT /path/to/sysroot)

# These must be set *before* project() is called
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)

# Ensure detection is correct in XNNPACK / CPUINFO
set(XNNPACK_TARGET_PROCESSOR aarch64)
set(CPUINFO_TARGET_PROCESSOR aarch64)

# Optional: Strip unused features for safety
set(BUILD_SHARED_LIBS OFF)

