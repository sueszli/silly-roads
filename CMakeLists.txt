cmake_minimum_required(VERSION 3.27 FATAL_ERROR)
project(defer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0.0)
  message(FATAL_ERROR "GCC is outdated: ${CMAKE_CXX_COMPILER_VERSION}")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0.0)
  message(FATAL_ERROR "Clang is outdated: ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER MATCHES "/opt/homebrew")
  # default Apple Clang often has an older stdlib
  add_link_options(-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++)
endif()

#
# compiler hardening
#
# see: https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++
#

add_compile_options(
  -O2
  -Wall
  -Wextra
  -Wformat
  -Wformat=2
  -Wconversion
  -Wsign-conversion
  -Wimplicit-fallthrough
  -Werror=format-security
  # more portable and explicit than -fhardened
  -U_FORTIFY_SOURCE
  -D_FORTIFY_SOURCE=3
  -D_GLIBCXX_ASSERTIONS
  -fstrict-flex-arrays=3
  -fstack-protector-strong
  # deprecated c calls
  -Werror=implicit
  -Werror=incompatible-pointer-types
  -Werror=int-conversion
  # multithreading with pthreads
  -fexceptions
  # for shared libraries use `-fPIC`
  -fPIE
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -ftrivial-auto-var-init=zero)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_options(-fstack-clash-protection)
  add_link_options(
    -pie
    -Wl,-z,nodlopen
    -Wl,-z,noexecstack
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,--as-needed
    -Wl,--no-copy-dt-needed-entries
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # add `-fzero-init-padding-bits=all` from gcc 15
  add_compile_options(-Wtrampolines -Wbidi-chars=any)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # nop
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686|i386")
  add_compile_options(-fcf-protection=full)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
  add_compile_options(-mbranch-protection=standard)
endif()

#
# sanitizers
#

if(NOT DISABLE_ASAN)
  # apple silicon requires clang from homebrew with paths set accordingly: `brew info llvm`
  add_compile_options(-fsanitize=address -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address)
endif()

if(NOT DISABLE_UBSAN)
  add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=undefined)
endif()

#
# dependencies
#

include(FetchContent)

FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip)
# in windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE
)
FetchContent_MakeAvailable(googletest)
# disable warning for google test
foreach(t gtest gtest_main gmock gmock_main)
  if(TARGET ${t})
    target_compile_options(${t} PUBLIC -Wno-character-conversion)
  endif()
endforeach()

#
# sources
#

file(GLOB_RECURSE SRCS src/*.cpp)
list(FILTER SRCS EXCLUDE REGEX "src/main\\.cpp$")
add_library(lib ${SRCS})
target_include_directories(lib PUBLIC src)
add_executable(binary src/main.cpp)
target_link_libraries(binary PRIVATE lib)

#
# tests
#

file(GLOB_RECURSE TEST_FILES test/*.cpp)

if(BUILD_TESTS)
  enable_testing()

  foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(TEST_SOURCE "test/${TEST_NAME}.cpp")
    set(TEST_EXECUTABLE "${TEST_NAME}_binary")
    add_executable(${TEST_EXECUTABLE} ${TEST_SOURCE})
    target_link_libraries(${TEST_EXECUTABLE} PRIVATE gtest_main lib)
    target_include_directories(${TEST_EXECUTABLE} PRIVATE src)
    include(GoogleTest)
    gtest_discover_tests(${TEST_EXECUTABLE})
  endforeach()
endif()
