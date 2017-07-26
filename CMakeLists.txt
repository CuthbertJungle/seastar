# Still missing:
#
# - Documentation targets
# - Link pools (for Ninja only)
# - All apps
# - OSv support
# - Symbol-stripping
# - Swagger
# - Per-module archive files (core, net, etc). "libseastar_core.a" (only for internal use).

cmake_minimum_required (VERSION 3.5)

project (seastar)

#
# Prologue.
#

set (CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set (CMAKE_CXX_FLAGS_RELEASE "-O2")
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
set (CMAKE_CXX_FLAGS_MINSIZEREL "-Os")

# Set an option defined in a sub-directory.
# Courtesy of http://edsiper.linuxchile.cl/blog/2016/01/08/cmake-override-subdirectory-options/
macro (set_option option value)
  set (${option} ${value} CACHE INTERNAL "" FORCE)
endmacro ()

#
# Configuration options.
#

option (SEASTAR_USER_CXXFLAGS
  "Extra CXXFLAGS."
  "")

option (SEASTAR_USER_LDFLAGS
  "Extra LDFLAGS."
  "")

option (SEASTAR_PIE
  "Enable position-independent executables (PIE)."
  OFF)

option (SEASTAR_ENABLE_HWLOC
  "Enable hwloc support."
  ON)

option (SEASTAR_ENABLE_DPDK
  "Enable DPDK (from bundled sources)."
  OFF)

option (SEASTAR_ENABLE_EXCEPTION_SCALABILITY_WORKAROUND
  "Override the dl_iterate_phdr symbol to workaround C++ exception scalability issues."
  OFF)

option (SEASTAR_ENABLE_GCC6_CONCEPTS
  "Enable experimental support for C++ concepts as implemented in GCC 6."
  OFF)

option (SEASTAR_LINK_STATIC
  "Statically link Seastar (useful for running on hosts outside the build environment)."
  OFF)

option (SEASTAR_LINK_STATIC_STDCXX
  "Link with libgcc and libstdc++ statically."
  OFF)

option (SEASTAR_LINK_STATIC_BOOST
  "Link with the Boost library statically."
  OFF)

option (SEASTAR_SUMMARIZE_SETTINGS
  "Display a summary of the configured compilation settings."
  ON)

##
## Check incompatible options.
##

if (${SEASTAR_ENABLE_EXCEPTION_SCALABILITY_WORKAROUND} AND ${SEASTAR_LINK_STATIC_STDCXX} AND ${SEASTAR_LINK_STATIC})
  message (FATAL_ERROR "Cannot enable exception scalability workaround with static linkage of libgcc and libstdc++.")
endif ()

#
# Primary target. Everything is configured later.
#

##
## Must be defined prior to the target being created.
##

if (${SEASTAR_PIE} OR NOT ${SEASTAR_LINK_STATIC})
  message (STATUS "Enabling position-independent code by request or because of dynamic linking.")
  set (CMAKE_POSITION_INDEPENDENT_CODE ON)
else ()
  set (CMAKE_POSITION_INDEPENDENT_CODE OFF)
endif ()

add_library (seastar stub.cc)
add_library (Seastar::seastar ALIAS seastar)

#
# External projects.
#

set_option (CARES_STATIC YES)
set_option (CARES_SHARED NO)
set_option (CARES_INSTALL NO)
set_option (CARES_STATIC_PIC YES)
add_subdirectory (c-ares)

add_subdirectory (fmt)

##
## DPDK.
##

if (${SEASTAR_ENABLE_DPDK})
  set (dpdk_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/dpdk")
  set (dpdk_binary_dir "${CMAKE_CURRENT_BINARY_DIR}/dpdk")

  #
  # Oh, CMake...
  #

  set (arch "")
  foreach (flag ${SEASTAR_USER_CXXFLAGS})
    if (${flag} MATCHES "-march=(.+)")
      set (arch ${CMAKE_MATCH_0})
    endif ()
  endforeach ()

  if (${arch} STREQUAL "nehalem")
    set (dpdk_machine "nhm")
  elseif (${arch} STREQUAL "westmere")
    set (dpdk_machine "wsm")
  elseif (${arch} STREQUAL "sandybridge")
    set (dpdk_machine "snb")
  elseif (${arch} STREQUAL "ivybridge")
    set (dpdk_machine "ivb")
  else ()
    set (dpdk_machine "native")
  endif ()

  if (EXISTS "${dpdk_binary_dir}/.config")
    file (REMOVE "${dpdk_binary_dir}/.config")
  endif ()

  exec_program (make
    ARGS -C ${dpdk_source_dir} RTE_OUTPUT=${dpdk_binary_dir} config "T=x86_64-${dpdk_machine}-linuxapp-gcc")

  exec_program ("${CMAKE_CURRENT_SOURCE_DIR}/dpdk_adjust_variables.py"
    ARGS "${dpdk_binary_dir}/.config" native)

  exec_program ("${CMAKE_CURRENT_SOURCE_DIR}/dpdk_query_cflags.py"
    ARGS ${dpdk_binary_dir} ${dpdk_machine}
    OUTPUT_VARIABLE dpdk_cflags)

  add_custom_target (seastar-dpdk
    COMMAND make -C ${dpdk_binary_dir} CC=\"${CMAKE_C_COMPILER} -Wno-implicit-fallthrough -Wno-format-truncation -Wno-bool-operation -Wno-maybe-uninitialized\")

  add_dependencies (seastar seastar-dpdk)

  target_include_directories (seastar SYSTEM PUBLIC "${dpdk_binary_dir}/include")
  target_compile_options (seastar PUBLIC "${dpdk_cflags}")
endif ()

#
# System dependencies.
#

set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

##
## Unconditional dependencies.
##

if (${SEASTAR_LINK_STATIC_BOOST})
  set (Boost_USE_STATIC_LIBS ON)
endif ()

find_package (Boost COMPONENTS program_options filesystem unit_test_framework thread REQUIRED)

if (${Boost_VERSION} VERSION_LESS "1.55")
  message(error "Seastar requires Boost >= 1.55")
endif ()

find_package (GnuTLS REQUIRED)

find_package (Protobuf REQUIRED)

find_package (Cryptopp REQUIRED)

find_package (Lz4 REQUIRED)

find_library (RT_LIBRARY rt DOC "The Posix Realtime extension library.")

find_library (AIO_LIBRARY aio DOC "The Posix asynchronous I/O library.")

find_program (Ragel_EXECUTABLE NAMES ragel DOC "Path to the ragel executable.")

##
## Conditional dependencies.
##

if (${SEASTAR_ENABLE_HWLOC})
  find_package (HWLoc REQUIRED)

  find_package (LibXml2 REQUIRED)

  find_package (ZLIB REQUIRED)

  find_library (NUMA_LIBRARY numa DOC "The NUMA support library.")

  find_library (PCIAccess_LIBRARY pciaccess DOC "The pciaccess library.")
endif ()

#
# Compilation flags and definitions.
#

target_compile_options (seastar
  PUBLIC
    -std=gnu++1y
    -Wall
    -Werror
    -Wno-error=deprecated-declarations
    -fvisibility=hidden
    -U_FORTIFY_SOURCE)

target_link_libraries (seastar PUBLIC
  -Wl,--no-as-needed
  -fvisibility=hidden)

if (${SEASTAR_LINK_STATIC})
  target_link_libraries (seastar PUBLIC -static)
endif ()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  target_compile_options (seastar PUBLIC
    -fsanitize=address -fsanitize=leak -fsanitize=undefined)

  target_compile_definitions (seastar PUBLIC
    DEBUG DEBUG_SHARED_PTR DEFAULT_ALLOCATOR SEASTAR_THREAD_STACK_GUARDS ASAN_ENABLED)
endif ()

if (${SEASTAR_ENABLE_GCC6_CONCEPTS})
  target_compile_options (seastar PUBLIC -fconcepts)
  target_compile_definitions (seastar PUBLIC HAVE_GCC6_CONCEPTS)
endif ()

if (NOT ${SEASTAR_ENABLE_EXCEPTION_SCALABILITY_WORKAROUND})
  target_compile_definitions (seastar PUBLIC NO_EXCEPTION_HACK)
endif ()

if (${SEASTAR_LINK_STATIC_STDCXX} OR ${SEASTAR_LINK_STATIC})
  target_compile_definitions (seastar PUBLIC NO_EXCEPTION_INTERCEPT)
endif ()

if (${SEASTAR_LINK_STATIC_STDCXX})
  target_link_libraries (seastar PUBLIC
    -static-libgcc
    -static-libstdc++)
endif ()

if (${SEASTAR_ENABLE_HWLOC})
  target_compile_definitions (seastar PUBLIC HAVE_HWLOC HAVE_NUMA)
endif ()

if (${SEASTAR_ENABLE_DPDK})
  target_compile_options (seastar PUBLIC
    -Wno-error=literal-suffix
    -Wno-literal-suffix
    -Wno-invalid-offsetof)

  target_compile_definitions (seastar PUBLIC HAVE_DPDK)
endif ()

# This doesn't work as you'd expect without the empty-check.
if (${SEASTAR_USER_CXXFLAGS})
  target_compile_options (seastar PUBLIC "${SEASTAR_USER_CXXFLAGS}")
endif ()

##
## Code tests.
##

set (CMAKE_REQUIRED_INCLUDES ${Lz4_INCLUDE_DIRS})
set (CMAKE_REQUIRED_LIBRARIES ${Lz4_LIBRARIES})
check_symbol_exists (LZ4_compress_default lz4.h have_lz4_compress_default)

if (${have_lz4_compress_default})
  target_compile_definitions (seastar PUBLIC HAVE_LZ4_COMPRESS_DEFAULT)
endif ()

try_compile (have_asan_fiber_support
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake-tests/asan_fiber.cc
  LINK_LIBRARIES -fsanitize=address)

if (${have_asan_fiber_support})
  target_compile_definitions (seastar PUBLIC HAVE_ASAN_FIBER_SUPPORT)
endif ()

# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67258
check_cxx_compiler_flag (-fsanitize=vptr have_sanitize_vptr_flag)

set (ENV{UBSAN_OPTIONS} "exitcode=1")
try_run (sanitize_vptr_run sanitize_vptr_compile
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake-tests/sanitize_vptr.cc
  CMAKE_FLAGS -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover")

# TODO: -fsanitize=vptr is broken even when the test passes.
if ((NOT "${have_sanitize_vptr_flag}") OR "${sanitize_vptr_run}" AND FALSE)
else ()
  if ("${CMAKE_BUILD_TYPE}" STREQUAL Debug)
    message (STATUS "-fsanitize=vptr is broken; disabling. Some debug-mode tests are bypassed.")
    target_compile_options (seastar PUBLIC -fno-sanitize=vptr)
  endif ()
endif ()

# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80947
try_compile (visiblity_flags_compile
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake-tests/visibility_flags.cc
  CMAKE_FLAGS -DCMAKE_CXX_FLAGS="-fvisibility=hidden -std=gnu++1y -Werror=attributes")

if (NOT "${visibility_flags_compile}")
  message (STATUS "Disabling -Wattributes due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80947")
  target_compile_options (seastar PUBLIC -Wno-attributes)
endif ()

##
## Warnings.
##

function (seastar_add_warning_if_supported warning)
  # GCC ignores the `-Wno` prefix even when the warning is not recognized, so strip it.
  string (REGEX REPLACE "^-Wno-" "-W" adjusted ${warning})
  string (REGEX REPLACE "^-W" "warning_" human_name_prefix ${adjusted})
  set (result "${human_name_prefix}_supported")
  check_cxx_compiler_flag (${adjusted} ${result})

  if ("${${result}}")
    target_compile_options (seastar PUBLIC ${warning})
  endif ()
endfunction ()

# Clang-only.
seastar_add_warning_if_supported (-Wno-mismatched-tags)

# Clang-only: moving a temporary object prevents copy elision.
seastar_add_warning_if_supported (-Wno-pessimizing-move)

# Clang-only: redundant move in return statement.
seastar_add_warning_if_supported (-Wno-redundant-move)

# Clang-only: 'x' overrides a member function but is not marked 'override'.
seastar_add_warning_if_supported (-Wno-inconsistent-missing-override)

# Clang-only: private field 'x' is not used.
seastar_add_warning_if_supported (-Wno-unused-private-field)

# Clang-only: unknown attribute 'x' ignored (x in this case is gnu::externally_visible).
seastar_add_warning_if_supported (-Wno-unknown-attributes)

# Clang-only: 'x' function 'y' declared in header file should be declared 'z'.
seastar_add_warning_if_supported (-Wno-unneeded-internal-declaration)

# Clang-only: inline function 'x' is not defined.
seastar_add_warning_if_supported (-Wno-undefined-inline)

# Clang-only: 'x' hides overloaded virtual functions.
seastar_add_warning_if_supported (-Wno-overloaded-virtual)

seastar_add_warning_if_supported (-Wno-maybe-uninitialized)

#
# Protobuf code generation.
#

add_subdirectory (proto)

function (seastar_ragel_generate file output_var)
  set (abs_file "${CMAKE_CURRENT_SOURCE_DIR}/${file}")

  get_filename_component (base_name ${file} NAME_WE)
  set (output_name "${base_name}.hh")

  add_custom_command (OUTPUT "${output_name}"
    COMMAND ${Ragel_EXECUTABLE} -G2 -o ${output_name} ${abs_file}
    # sed away a bug in ragel 7 that emits some extraneous _nfa* variables.
    COMMAND sed -i -e '1h\;2,$$H\;$$!d\;g' -re 's/static const char _nfa[^\;]*\;//g' ${output_name})

  set ("${output_var}" "${CMAKE_CURRENT_BINARY_DIR}/${output_name}" PARENT_SCOPE)
endfunction ()

#
# Library.
#

set (SEASTAR_SOURCES "")

function (seastar_add_sources)
  set (abs_files "")

  foreach (file ${ARGN})
    list (APPEND abs_files "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
  endforeach ()

  target_sources (seastar PRIVATE "${abs_files}")
endfunction ()

add_subdirectory (core)
add_subdirectory (http)
add_subdirectory (json)
add_subdirectory (net)
add_subdirectory (rpc)
add_subdirectory (util)

target_include_directories (seastar SYSTEM
  PUBLIC
    ${GNUTLS_INCLUDE_DIR}
    ${Protobuf_INCLUDE_DIRS})

target_include_directories (seastar PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR})

if (${SEASTAR_ENABLE_HWLOC})
  target_include_directories (seastar SYSTEM
    PUBLIC
      ${LIBXML2_INCLUDE_DIRS})
endif ()

# Include -lgcc_s before -lunwind to work around for
# https://savannah.nongnu.org/bugs/?48486. See
# https://github.com/scylladb/scylla/issues/1725.

if (${SEASTAR_USER_LDFLAGS})
  target_link_libraries (seastar PUBLIC "${SEASTAR_USER_LDFLAGS}")
endif ()

target_link_libraries (seastar PUBLIC
  debug -fsanitize=address
  debug -fsanitize=leak
  debug -fsanitize=undefined
  -ldl
  -lgcc_s
  -lunwind
  ${CMAKE_THREAD_LIBS_INIT}
  ${GNUTLS_LIBRARIES}
  ${Protobuf_LIBRARIES}
  ${RT_LIBRARY}
  ${AIO_LIBRARY}
  Boost::boost # Headers only.
  Cryptopp::cryptopp
  Lz4::lz4
  fmt::fmt-header-only
  c-ares::cares)

set (boost_targets Boost::program_options Boost::filesystem Boost::thread)

if ("${SEASTAR_LINK_STATIC_BOOST}" AND (NOT "${SEASTAR_LINK_STATIC}"))
  target_link_libraries (seastar PUBLIC "-Wl,-Bstatic ${boost_targets} -Wl,-Bdynamic")
else ()
  target_link_libraries (seastar PUBLIC "${boost_targets}")
endif ()

if (${SEASTAR_ENABLE_HWLOC})
  target_link_libraries (seastar PUBLIC
    ${NUMA_LIBRARY}
    ${PCIAccess_LIBRARY}
    ${LIBXML2_LIBRARIES}
    HWLoc::hwloc
    ZLIB::ZLIB)
endif ()

if (${SEASTAR_ENABLE_DPDK})
  target_link_libraries (seastar PUBLIC
    -L"${dpdk_binary_dir}/lib"
    -lrte_pmd_vmxnet3_uio
    -lrte_pmd_i40e
    -lrte_pmd_ixgbe
    -lrte_pmd_e1000
    -lrte_pmd_ring
    -lrte_pmd_bnxt
    -lrte_pmd_cxgbe
    -lrte_pmd_ena
    -lrte_pmd_enic
    -lrte_pmd_fm10k
    -lrte_pmd_nfp
    -lrte_pmd_qede
    -lrte_pmd_sfc_efx
    -lrte_hash
    -lrte_kvargs
    -lrte_mbuf
    -lrte_ethdev
    -lrte_eal
    -lrte_mempool
    -lrte_ring
    -lrte_cmdline
    -lrte_cfgfile)
endif ()

#
# Applications.
#

add_subdirectory (apps)

#
# Tests.
#

enable_testing ()

add_subdirectory (tests)

#
# Conclusion.
#

if (${SEASTAR_SUMMARIZE_SETTINGS})
  get_target_property (flags seastar COMPILE_OPTIONS)
  get_target_property (definitions seastar COMPILE_DEFINITIONS)

  message ("flags: ${flags}")
  message ("definitions: ${definitions}")
endif ()
