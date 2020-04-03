if(PKG_USER-INTEL)
  check_include_file_cxx(immintrin.h FOUND_IMMINTRIN)
  if(NOT FOUND_IMMINTRIN)
    message(FATAL_ERROR "immintrin.h header not found, Intel package won't work without it")
  endif()

  target_compile_definitions(lammps PRIVATE -DLMP_USER_INTEL)

  set(INTEL_ARCH "cpu" CACHE STRING "Architectures used by USER-INTEL (cpu or knl)")
  set(INTEL_ARCH_VALUES cpu knl)
  set_property(CACHE INTEL_ARCH PROPERTY STRINGS ${INTEL_ARCH_VALUES})
  validate_option(INTEL_ARCH INTEL_ARCH_VALUES)
  string(TOUPPER ${INTEL_ARCH} INTEL_ARCH)

  find_package(Threads QUIET)
  if(Threads_FOUND)
    set(INTEL_LRT_MODE "threads" CACHE STRING "Long-range threads mode (none, threads, or c++11)")
  else()
    set(INTEL_LRT_MODE "none" CACHE STRING "Long-range threads mode (none, threads, or c++11)")
  endif()
  set(INTEL_LRT_VALUES none threads c++11)
  set_property(CACHE INTEL_LRT_MODE PROPERTY STRINGS ${INTEL_LRT_VALUES})
  validate_option(INTEL_LRT_MODE INTEL_LRT_VALUES)
  string(TOUPPER ${INTEL_LRT_MODE} INTEL_LRT_MODE)
  if(INTEL_LRT_MODE STREQUAL "THREADS")
    if(Threads_FOUND)
      target_compile_definitions(lammps PRIVATE -DLMP_INTEL_USELRT)
      target_link_libraries(lammps PRIVATE Threads::Threads)
    else()
      message(FATAL_ERROR "Must have working threads library for Long-range thread support")
    endif()
  endif()
  if(INTEL_LRT_MODE STREQUAL "C++11")
    target_compile_definitions(lammps PRIVATE -DLMP_INTEL_USELRT -DLMP_INTEL_LRT11)
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
      message(FATAL_ERROR "USER-INTEL needs at least a 2016 Intel compiler, found ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
  else()
    message(WARNING "USER-INTEL gives best performance with Intel compilers")
  endif()

  find_package(TBB_MALLOC QUIET)
  if(TBB_MALLOC_FOUND)
    target_link_libraries(lammps PRIVATE TBB::TBB_MALLOC)
  else()
    target_compile_definitions(lammps PRIVATE -DLMP_INTEL_NO_TBB)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      message(WARNING "USER-INTEL with Intel compilers should use TBB malloc libraries")
    endif()
  endif()

  find_package(MKL QUIET)
  if(MKL_FOUND)
    target_compile_definitions(lammps PRIVATE -DLMP_USE_MKL_RNG)
    target_link_libraries(lammps PRIVATE MKL::MKL)
  else()
    message(STATUS "Pair style dpd/intel will be faster with MKL libraries")
  endif()

  if((NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows") AND (NOT ${LAMMPS_MEMALIGN} STREQUAL "64") AND (NOT ${LAMMPS_MEMALIGN} STREQUAL "128") AND (NOT ${LAMMPS_MEMALIGN} STREQUAL "256"))
    message(FATAL_ERROR "USER-INTEL only supports memory alignment of 64, 128 or 256 on this platform")
  endif()

  if(INTEL_ARCH STREQUAL "KNL")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      message(FATAL_ERROR "Must use Intel compiler with USER-INTEL for KNL architecture")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -xHost -qopenmp -qoffload")
    set(MIC_OPTIONS "-qoffload-option,mic,compiler,\"-fp-model fast=2 -mGLOB_default_function_attrs=\\\"gather_scatter_loop_unroll=4\\\"\"")
    target_compile_options(lammps PRIVATE -xMIC-AVX512 -qoffload -fno-alias -ansi-alias -restrict -qoverride-limits ${MIC_OPTIONS})
    target_compile_definitions(lammps PRIVATE -DLMP_INTEL_OFFLOAD)
  else()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
      include(CheckCXXCompilerFlag)
      foreach(_FLAG -O2 -fp-model fast=2 -no-prec-div -qoverride-limits -qopt-zmm-usage=high -qno-offload -fno-alias -ansi-alias -restrict)
        check_cxx_compiler_flag("${__FLAG}" COMPILER_SUPPORTS${_FLAG})
        if(COMPILER_SUPPORTS${_FLAG})
	  target_compile_options(lammps PRIVATE ${_FLAG})
        endif()
      endforeach()
    endif()
  endif()

  # collect sources
  set(USER-INTEL_SOURCES_DIR ${LAMMPS_SOURCE_DIR}/USER-INTEL)
  set(USER-INTEL_SOURCES ${USER-INTEL_SOURCES_DIR}/fix_intel.cpp
                         ${USER-INTEL_SOURCES_DIR}/fix_nh_intel.cpp
                         ${USER-INTEL_SOURCES_DIR}/intel_buffers.cpp
                         ${USER-INTEL_SOURCES_DIR}/nbin_intel.cpp
                         ${USER-INTEL_SOURCES_DIR}/npair_intel.cpp)

  set_property(GLOBAL PROPERTY "USER-INTEL_SOURCES" "${USER-INTEL_SOURCES}")

  # detect styles which have a USER-INTEL version
  RegisterStylesExt(${USER-INTEL_SOURCES_DIR} intel USER-INTEL_SOURCES)
  RegisterNBinStyle(${USER-INTEL_SOURCES_DIR}/nbin_intel.h)
  RegisterNPairStyle(${USER-INTEL_SOURCES_DIR}/npair_intel.h)
  RegisterFixStyle(${USER-INTEL_SOURCES_DIR}/fix_intel.h)

  get_property(USER-INTEL_SOURCES GLOBAL PROPERTY USER-INTEL_SOURCES)
  if(PKG_KSPACE)
    list(APPEND USER-INTEL_SOURCES ${USER-INTEL_SOURCES_DIR}/verlet_lrt_intel.cpp)
    RegisterIntegrateStyle(${USER-INTEL_SOURCES_DIR}/verlet_lrt_intel.h)
  endif()

  target_sources(lammps PRIVATE ${USER-INTEL_SOURCES})
  target_include_directories(lammps PRIVATE ${USER-INTEL_SOURCES_DIR})
endif()
