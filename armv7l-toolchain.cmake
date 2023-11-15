# Set system name and processor
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)
set(CMAKE_SYSROOT "/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc")
set(CMAKE_LINKER /opt/miyoomini-toolchain/bin/arm-linux-gnueabihf-ld)
set(CMAKE_INSTALL_RPATH "/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/lib")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/lib/ -L/lib/arm-linux-gnueabihf -L/usr/lib/arm-linux-gnueabihf/ -L/usr/lib/arm-linux-gnueabihf/neon/vfp/ -L/lib -L/root/workspace/ffmpegex/lib -L/root/workspace/ffmpegex/lib -L/mnt/SDCARD/App/ffmpeg/lib")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/lib/ -L/lib/arm-linux-gnueabihf -L/usr/lib/arm-linux-gnueabihf/ -L/usr/lib/arm-linux-gnueabihf/neon/vfp/ -L/lib -L/root/workspace/ffmpegex/lib -L/root/workspace/ffmpegex/lib -L/mnt/SDCARD/App/ffmpeg/lib")
# Additional Include Directories
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include -I/usr -I/usr/include -I/usr/include/arm-linux-gnueabihf/ -I/root/workspace/ffmpegex/include -I/mnt/SDCARD/App/ffmpeg/include")


# Compiler settings
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

# Libraries and include settings
set(CURL_LIBRARY /usr/lib/arm-linux-gnueabihf/libcurl.so)
set(CURL_INCLUDE_DIR /usr/include)
set(OPENSSL_INCLUDE_DIR /usr/include)
set(OPENSSL_ROOT_DIR /usr/include)

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "-L/lib/arm-linux-gnueabihf")

# Verbose makefile generation
set(CMAKE_VERBOSE_MAKEFILE ON)
