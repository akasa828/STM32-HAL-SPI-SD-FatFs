cmake_minimum_required(VERSION 3.20)
project(sd_driver_host_tests C)

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

function(enable_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

# Keep test translation units inside the build tree.  Some Windows toolchains
# cannot create Ninja object directories for sources reached through a parent
# path when the repository path contains non-ASCII characters.
set(HOST_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/driver_sources")
file(MAKE_DIRECTORY "${HOST_SOURCE_DIR}")
configure_file(../Core/Micro_SD/SD_reader.c "${HOST_SOURCE_DIR}/SD_reader.c" COPYONLY)
configure_file(../Core/fatfs/diskio.c "${HOST_SOURCE_DIR}/diskio.c" COPYONLY)
configure_file(../Core/Port/sd_stm32_hal.c "${HOST_SOURCE_DIR}/sd_stm32_hal.c" COPYONLY)

add_executable(test_sd_core
    test_sd_core.c
    "${HOST_SOURCE_DIR}/SD_reader.c"
    "${HOST_SOURCE_DIR}/diskio.c"
)
target_include_directories(test_sd_core PRIVATE
    ../Core/Micro_SD
    ../Core/fatfs
)
target_compile_definitions(test_sd_core PRIVATE SD_ENABLE_SELF_TEST=0)
enable_strict_warnings(test_sd_core)
add_test(NAME sd_core COMMAND test_sd_core)

add_executable(test_sd_core_command_crc
    test_sd_core.c
    "${HOST_SOURCE_DIR}/SD_reader.c"
    "${HOST_SOURCE_DIR}/diskio.c"
)
target_include_directories(test_sd_core_command_crc PRIVATE
    ../Core/Micro_SD
    ../Core/fatfs
)
target_compile_definitions(test_sd_core_command_crc PRIVATE
    SD_ENABLE_SELF_TEST=0
    SD_ENABLE_CMD_CRC=1
)
enable_strict_warnings(test_sd_core_command_crc)
add_test(NAME sd_core_command_crc COMMAND test_sd_core_command_crc)

add_executable(test_sd_self_test
    test_sd_core.c
    "${HOST_SOURCE_DIR}/SD_reader.c"
    "${HOST_SOURCE_DIR}/diskio.c"
)
target_include_directories(test_sd_self_test PRIVATE
    ../Core/Micro_SD
    ../Core/fatfs
)
target_compile_definitions(test_sd_self_test PRIVATE SD_ENABLE_SELF_TEST=1)
enable_strict_warnings(test_sd_self_test)
add_test(NAME sd_self_test COMMAND test_sd_self_test)

add_executable(test_sd_hal_adapter
    test_sd_hal_adapter.c
    "${HOST_SOURCE_DIR}/sd_stm32_hal.c"
)
target_include_directories(test_sd_hal_adapter PRIVATE
    fakes
    ../Core/Micro_SD
    ../Core/Port
)
enable_strict_warnings(test_sd_hal_adapter)
add_test(NAME sd_hal_adapter COMMAND test_sd_hal_adapter)
