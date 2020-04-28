set(GPU_SOURCES_DIR ${LAMMPS_SOURCE_DIR}/GPU)
set(GPU_SOURCES ${GPU_SOURCES_DIR}/gpu_extra.h
                ${GPU_SOURCES_DIR}/fix_gpu.h
                ${GPU_SOURCES_DIR}/fix_gpu.cpp)

set(GPU_API "opencl" CACHE STRING "API used by GPU package")
set(GPU_API_VALUES opencl cuda hip)
set_property(CACHE GPU_API PROPERTY STRINGS ${GPU_API_VALUES})
validate_option(GPU_API GPU_API_VALUES)
string(TOUPPER ${GPU_API} GPU_API)

set(GPU_PREC "mixed" CACHE STRING "LAMMPS GPU precision")
set(GPU_PREC_VALUES double mixed single)
set_property(CACHE GPU_PREC PROPERTY STRINGS ${GPU_PREC_VALUES})
validate_option(GPU_PREC GPU_PREC_VALUES)
string(TOUPPER ${GPU_PREC} GPU_PREC)

if(GPU_PREC STREQUAL "DOUBLE")
  set(GPU_PREC_SETTING "DOUBLE_DOUBLE")
elseif(GPU_PREC STREQUAL "MIXED")
  set(GPU_PREC_SETTING "SINGLE_DOUBLE")
elseif(GPU_PREC STREQUAL "SINGLE")
  set(GPU_PREC_SETTING "SINGLE_SINGLE")
endif()

file(GLOB GPU_LIB_SOURCES ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cpp)
file(MAKE_DIRECTORY ${LAMMPS_LIB_BINARY_DIR}/gpu)

if(GPU_API STREQUAL "CUDA")
  find_package(CUDA REQUIRED)
  find_program(BIN2C bin2c)
  if(NOT BIN2C)
    message(FATAL_ERROR "Could not find bin2c, use -DBIN2C=/path/to/bin2c to help cmake finding it.")
  endif()
  option(CUDPP_OPT "Enable CUDPP_OPT" ON)
  option(CUDA_MPS_SUPPORT "Enable tweaks to support CUDA Multi-process service (MPS)" OFF)
  if(CUDA_MPS_SUPPORT)
    set(GPU_CUDA_MPS_FLAGS "-DCUDA_PROXY")
  endif()

  set(GPU_ARCH "sm_50" CACHE STRING "LAMMPS GPU CUDA SM primary architecture (e.g. sm_60)")

  file(GLOB GPU_LIB_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cu ${CMAKE_CURRENT_SOURCE_DIR}/gpu/[^.]*.cu)
  list(REMOVE_ITEM GPU_LIB_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_pppm.cu)

  cuda_include_directories(${LAMMPS_LIB_SOURCE_DIR}/gpu ${LAMMPS_LIB_BINARY_DIR}/gpu)

  if(CUDPP_OPT)
    cuda_include_directories(${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini)
    file(GLOB GPU_LIB_CUDPP_SOURCES ${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini/[^.]*.cpp)
    file(GLOB GPU_LIB_CUDPP_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini/[^.]*.cu)
  endif()

  # build arch/gencode commands for nvcc based on CUDA toolkit version and use choice
  # --arch translates directly instead of JIT, so this should be for the preferred or most common architecture
  set(GPU_CUDA_GENCODE "-arch=${GPU_ARCH} ")
  # Fermi (GPU Arch 2.x) is supported by CUDA 3.2 to CUDA 8.0
  if((CUDA_VERSION VERSION_GREATER "3.1") AND (CUDA_VERSION VERSION_LESS "9.0"))
    string(APPEND GPU_CUDA_GENCODE "-gencode arch=compute_20,code=[sm_20,compute_20] ")
  endif()
  # Kepler (GPU Arch 3.x) is supported by CUDA 5 and later
  if(CUDA_VERSION VERSION_GREATER "4.9")
    string(APPEND GPU_CUDA_GENCODE "-gencode arch=compute_30,code=[sm_30,compute_30] -gencode arch=compute_35,code=[sm_35,compute_35] ")
  endif()
  # Maxwell (GPU Arch 5.x) is supported by CUDA 6 and later
  if(CUDA_VERSION VERSION_GREATER "5.9")
    string(APPEND GPU_CUDA_GENCODE "-gencode arch=compute_50,code=[sm_50,compute_50] -gencode arch=compute_52,code=[sm_52,compute_52] ")
  endif()
  # Pascal (GPU Arch 6.x) is supported by CUDA 8 and later
  if(CUDA_VERSION VERSION_GREATER "7.9")
    string(APPEND GPU_CUDA_GENCODE "-gencode arch=compute_60,code=[sm_60,compute_60] -gencode arch=compute_61,code=[sm_61,compute_61] ")
  endif()
  # Volta (GPU Arch 7.0) is supported by CUDA 9 and later
  if(CUDA_VERSION VERSION_GREATER "8.9")
    string(APPEND GPU_CUDA_GENCODE "-gencode arch=compute_70,code=[sm_70,compute_70] ")
  endif()
  # Turing (GPU Arch 7.5) is supported by CUDA 10 and later
  if(CUDA_VERSION VERSION_GREATER "9.9")
    string(APPEND GPU_CUDA_GENCODE "-gencode arch=compute_75,code=[sm_75,compute_75] ")
  endif()

  cuda_compile_fatbin(GPU_GEN_OBJS ${GPU_LIB_CU} OPTIONS
          -DUNIX -O3 --use_fast_math -Wno-deprecated-gpu-targets -DNV_KERNEL -DUCL_CUDADR ${GPU_CUDA_GENCODE} -D_${GPU_PREC_SETTING})

  cuda_compile(GPU_OBJS ${GPU_LIB_CUDPP_CU} OPTIONS ${CUDA_REQUEST_PIC}
          -DUNIX -O3 --use_fast_math -Wno-deprecated-gpu-targets -DUCL_CUDADR ${GPU_CUDA_GENCODE} -D_${GPU_PREC_SETTING})

  foreach(CU_OBJ ${GPU_GEN_OBJS})
    get_filename_component(CU_NAME ${CU_OBJ} NAME_WE)
    string(REGEX REPLACE "^.*_lal_" "" CU_NAME "${CU_NAME}")
    add_custom_command(OUTPUT ${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}_cubin.h
      COMMAND ${BIN2C} -c -n ${CU_NAME} ${CU_OBJ} > ${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}_cubin.h
      DEPENDS ${CU_OBJ}
      COMMENT "Generating ${CU_NAME}_cubin.h")
    list(APPEND GPU_LIB_SOURCES ${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}_cubin.h)
  endforeach()
  set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${LAMMPS_LIB_BINARY_DIR}/gpu/*_cubin.h")

  add_library(gpu STATIC ${GPU_LIB_SOURCES} ${GPU_LIB_CUDPP_SOURCES} ${GPU_OBJS})
  target_link_libraries(gpu PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY})
  target_include_directories(gpu PRIVATE ${LAMMPS_LIB_BINARY_DIR}/gpu ${CUDA_INCLUDE_DIRS})
  target_compile_definitions(gpu PRIVATE -D_${GPU_PREC_SETTING} -DMPI_GERYON -DUCL_NO_EXIT ${GPU_CUDA_MPS_FLAGS})
  if(CUDPP_OPT)
    target_include_directories(gpu PRIVATE ${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini)
    target_compile_definitions(gpu PRIVATE -DUSE_CUDPP)
  endif()

  target_link_libraries(lammps PRIVATE gpu)

  add_executable(nvc_get_devices ${LAMMPS_LIB_SOURCE_DIR}/gpu/geryon/ucl_get_devices.cpp)
  target_compile_definitions(nvc_get_devices PRIVATE -DUCL_CUDADR)
  target_link_libraries(nvc_get_devices PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY})
  target_include_directories(nvc_get_devices PRIVATE ${CUDA_INCLUDE_DIRS})

elseif(GPU_API STREQUAL "OPENCL")
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    # download and unpack support binaries for compilation of windows binaries.
    set(LAMMPS_THIRDPARTY_URL "http://download.lammps.org/thirdparty")
    file(DOWNLOAD "${LAMMPS_THIRDPARTY_URL}/opencl-win-devel.tar.gz" "${CMAKE_CURRENT_BINARY_DIR}/opencl-win-devel.tar.gz"
            EXPECTED_MD5 2c00364888d5671195598b44c2e0d44d)
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf opencl-win-devel.tar.gz WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_library(OpenCL::OpenCL UNKNOWN IMPORTED)
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86")
      set_target_properties(OpenCL::OpenCL PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/OpenCL/lib_win32/libOpenCL.dll")
    elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
      set_target_properties(OpenCL::OpenCL PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/OpenCL/lib_win64/libOpenCL.dll")
    endif()
    set_target_properties(OpenCL::OpenCL PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/OpenCL/include")
  else()
    find_package(OpenCL REQUIRED)
  endif()
  set(OCL_TUNE "generic" CACHE STRING "OpenCL Device Tuning")
  set(OCL_TUNE_VALUES intel fermi kepler cypress generic)
  set_property(CACHE OCL_TUNE PROPERTY STRINGS ${OCL_TUNE_VALUES})
  validate_option(OCL_TUNE OCL_TUNE_VALUES)
  string(TOUPPER ${OCL_TUNE} OCL_TUNE)

  include(OpenCLUtils)
  set(OCL_COMMON_HEADERS ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_preprocessor.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_aux_fun1.h)

  file(GLOB GPU_LIB_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cu)
  list(REMOVE_ITEM GPU_LIB_CU
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne_lj.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared_lj.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_zbl.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_mod.cu
  )

  foreach(GPU_KERNEL ${GPU_LIB_CU})
      get_filename_component(basename ${GPU_KERNEL} NAME_WE)
      string(SUBSTRING ${basename} 4 -1 KERNEL_NAME)
      GenerateOpenCLHeader(${KERNEL_NAME} ${CMAKE_CURRENT_BINARY_DIR}/gpu/${KERNEL_NAME}_cl.h ${OCL_COMMON_HEADERS} ${GPU_KERNEL})
      list(APPEND GPU_LIB_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/gpu/${KERNEL_NAME}_cl.h)
  endforeach()

  GenerateOpenCLHeader(gayberne ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne.cu)
  GenerateOpenCLHeader(gayberne_lj ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_lj_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne_lj.cu)
  GenerateOpenCLHeader(re_squared ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared.cu)
  GenerateOpenCLHeader(re_squared_lj ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_lj_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared_lj.cu)
  GenerateOpenCLHeader(tersoff ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff.cu)
  GenerateOpenCLHeader(tersoff_zbl ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_zbl_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_zbl_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_zbl.cu)
  GenerateOpenCLHeader(tersoff_mod ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_mod_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_mod_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_mod.cu)

  list(APPEND GPU_LIB_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_lj_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_lj_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_zbl_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_mod_cl.h
  )

  add_library(gpu STATIC ${GPU_LIB_SOURCES})
  target_link_libraries(gpu PRIVATE OpenCL::OpenCL)
  target_include_directories(gpu PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gpu)
  target_compile_definitions(gpu PRIVATE -D_${GPU_PREC_SETTING} -D${OCL_TUNE}_OCL -DMPI_GERYON -DUCL_NO_EXIT)
  target_compile_definitions(gpu PRIVATE -DUSE_OPENCL)

  target_link_libraries(lammps PRIVATE gpu)

  add_executable(ocl_get_devices ${LAMMPS_LIB_SOURCE_DIR}/gpu/geryon/ucl_get_devices.cpp)
  target_compile_definitions(ocl_get_devices PRIVATE -DUCL_OPENCL)
  target_link_libraries(ocl_get_devices PRIVATE OpenCL::OpenCL)
elseif(GPU_API STREQUAL "HIP")
  if(NOT DEFINED HIP_PATH)
      if(NOT DEFINED ENV{HIP_PATH})
          set(HIP_PATH "/opt/rocm/hip" CACHE PATH "Path to which HIP has been installed")
      else()
          set(HIP_PATH $ENV{HIP_PATH} CACHE PATH "Path to which HIP has been installed")
      endif()
  endif()
  set(CMAKE_MODULE_PATH "${HIP_PATH}/cmake" ${CMAKE_MODULE_PATH})
  find_package(HIP REQUIRED)
  option(HIP_USE_DEVICE_SORT "Use GPU sorting" ON)

  if(NOT DEFINED HIP_PLATFORM)
      if(NOT DEFINED ENV{HIP_PLATFORM})
          set(HIP_PLATFORM "hcc" CACHE PATH "HIP Platform to be used during compilation")
      else()
          set(HIP_PLATFORM $ENV{HIP_PLATFORM} CACHE PATH "HIP Platform used during compilation")
      endif()
  endif()

  set(ENV{HIP_PLATFORM} ${HIP_PLATFORM})

  if(HIP_PLATFORM STREQUAL "hcc")
    set(HIP_ARCH "gfx906" CACHE STRING "HIP target architecture")
  elseif(HIP_PLATFORM STREQUAL "nvcc")
    find_package(CUDA REQUIRED)
    set(HIP_ARCH "sm_50" CACHE STRING "HIP primary CUDA architecture (e.g. sm_60)")

    # build arch/gencode commands for nvcc based on CUDA toolkit version and use choice
    # --arch translates directly instead of JIT, so this should be for the preferred or most common architecture
    set(HIP_CUDA_GENCODE "-arch=${HIP_ARCH} ")
    # Fermi (GPU Arch 2.x) is supported by CUDA 3.2 to CUDA 8.0
    if((CUDA_VERSION VERSION_GREATER "3.1") AND (CUDA_VERSION VERSION_LESS "9.0"))
      string(APPEND HIP_CUDA_GENCODE "-gencode arch=compute_20,code=[sm_20,compute_20] ")
    endif()
    # Kepler (GPU Arch 3.x) is supported by CUDA 5 and later
    if(CUDA_VERSION VERSION_GREATER "4.9")
      string(APPEND HIP_CUDA_GENCODE "-gencode arch=compute_30,code=[sm_30,compute_30] -gencode arch=compute_35,code=[sm_35,compute_35] ")
    endif()
    # Maxwell (GPU Arch 5.x) is supported by CUDA 6 and later
    if(CUDA_VERSION VERSION_GREATER "5.9")
      string(APPEND HIP_CUDA_GENCODE "-gencode arch=compute_50,code=[sm_50,compute_50] -gencode arch=compute_52,code=[sm_52,compute_52] ")
    endif()
    # Pascal (GPU Arch 6.x) is supported by CUDA 8 and later
    if(CUDA_VERSION VERSION_GREATER "7.9")
      string(APPEND HIP_CUDA_GENCODE "-gencode arch=compute_60,code=[sm_60,compute_60] -gencode arch=compute_61,code=[sm_61,compute_61] ")
    endif()
    # Volta (GPU Arch 7.0) is supported by CUDA 9 and later
    if(CUDA_VERSION VERSION_GREATER "8.9")
      string(APPEND HIP_CUDA_GENCODE "-gencode arch=compute_70,code=[sm_70,compute_70] ")
    endif()
    # Turing (GPU Arch 7.5) is supported by CUDA 10 and later
    if(CUDA_VERSION VERSION_GREATER "9.9")
      string(APPEND HIP_CUDA_GENCODE "-gencode arch=compute_75,code=[sm_75,compute_75] ")
    endif()
  endif()

  file(GLOB GPU_LIB_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cu ${CMAKE_CURRENT_SOURCE_DIR}/gpu/[^.]*.cu)
  list(REMOVE_ITEM GPU_LIB_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_pppm.cu)

  set(GPU_LIB_CU_HIP "")
  foreach(CU_FILE ${GPU_LIB_CU})
    get_filename_component(CU_NAME ${CU_FILE} NAME_WE)
    string(REGEX REPLACE "^.*lal_" "" CU_NAME "${CU_NAME}")

    set(CU_CPP_FILE  "${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}.cu.cpp")
    set(CUBIN_FILE   "${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}.cubin")
    set(CUBIN_H_FILE "${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}_cubin.h")

    if(HIP_PLATFORM STREQUAL "hcc")
        configure_file(${CU_FILE} ${CU_CPP_FILE} COPYONLY)

        add_custom_command(OUTPUT ${CUBIN_FILE}
          VERBATIM COMMAND ${HIP_HIPCC_EXECUTABLE} --genco -t="${HIP_ARCH}" -f=\"-O3 -ffast-math -DUSE_HIP -D_${GPU_PREC_SETTING} -I${LAMMPS_LIB_SOURCE_DIR}/gpu\" -o ${CUBIN_FILE} ${CU_CPP_FILE}
          DEPENDS ${CU_CPP_FILE}
          COMMENT "Generating ${CU_NAME}.cubin")
    elseif(HIP_PLATFORM STREQUAL "nvcc")
        add_custom_command(OUTPUT ${CUBIN_FILE}
          VERBATIM COMMAND ${HIP_HIPCC_EXECUTABLE} --fatbin --use_fast_math -DUSE_HIP -D_${GPU_PREC_SETTING} ${HIP_CUDA_GENCODE} -I${LAMMPS_LIB_SOURCE_DIR}/gpu -o ${CUBIN_FILE} ${CU_FILE}
          DEPENDS ${CU_FILE}
          COMMENT "Generating ${CU_NAME}.cubin")
    endif()

    add_custom_command(OUTPUT ${CUBIN_H_FILE}
      COMMAND ${CMAKE_COMMAND} -D SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -D VARNAME=${CU_NAME} -D HEADER_FILE=${CUBIN_H_FILE} -D SOURCE_FILES=${CUBIN_FILE} -P ${CMAKE_CURRENT_SOURCE_DIR}/Modules/GenerateBinaryHeader.cmake
      DEPENDS ${CUBIN_FILE}
      COMMENT "Generating ${CU_NAME}_cubin.h")

    list(APPEND GPU_LIB_SOURCES ${CUBIN_H_FILE})
  endforeach()

  set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${LAMMPS_LIB_BINARY_DIR}/gpu/*_cubin.h ${LAMMPS_LIB_BINARY_DIR}/gpu/*.cu.cpp")

  hip_add_library(gpu STATIC ${GPU_LIB_SOURCES})
  target_include_directories(gpu PRIVATE ${LAMMPS_LIB_BINARY_DIR}/gpu)
  target_compile_definitions(gpu PRIVATE -D_${GPU_PREC_SETTING} -DMPI_GERYON -DUCL_NO_EXIT)
  target_compile_definitions(gpu PRIVATE -DUSE_HIP)

  if(HIP_USE_DEVICE_SORT)
    # add hipCUB
    target_include_directories(gpu PRIVATE ${HIP_ROOT_DIR}/../include)
    target_compile_definitions(gpu PRIVATE -DUSE_HIP_DEVICE_SORT)

    if(HIP_PLATFORM STREQUAL "nvcc")
      find_package(CUB)

      if(CUB_FOUND)
        set(DOWNLOAD_CUB_DEFAULT OFF)
      else()
        set(DOWNLOAD_CUB_DEFAULT ON)
      endif()

      option(DOWNLOAD_CUB "Download and compile the CUB library instead of using an already installed one" ${DOWNLOAD_CUB_DEFAULT})

      if(DOWNLOAD_CUB)
        message(STATUS "CUB download requested")
        include(ExternalProject)

        ExternalProject_Add(CUB
          GIT_REPOSITORY https://github.com/NVlabs/cub
          TIMEOUT 5
          PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
          CONFIGURE_COMMAND ""
          BUILD_COMMAND ""
          INSTALL_COMMAND ""
          UPDATE_COMMAND ""
        )
        ExternalProject_get_property(CUB SOURCE_DIR)
        set(CUB_INCLUDE_DIR ${SOURCE_DIR})
      else()
        find_package(CUB)
        if(NOT CUB_FOUND)
          message(FATAL_ERROR "CUB library not found. Help CMake to find it by setting CUB_INCLUDE_DIR, or set DOWNLOAD_VORO=ON to download it")
        endif()
      endif()

      target_include_directories(gpu PRIVATE ${CUB_INCLUDE_DIR})
    endif()
  endif()

  hip_add_executable(hip_get_devices ${LAMMPS_LIB_SOURCE_DIR}/gpu/geryon/ucl_get_devices.cpp)
  target_compile_definitions(hip_get_devices PRIVATE -DUCL_HIP)

  if(HIP_PLATFORM STREQUAL "nvcc")
    target_compile_definitions(gpu PRIVATE -D__HIP_PLATFORM_NVCC__)
    target_include_directories(gpu PRIVATE ${HIP_ROOT_DIR}/../include)
    target_include_directories(gpu PRIVATE ${CUDA_INCLUDE_DIRS})
    target_link_libraries(gpu PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY})

    target_compile_definitions(hip_get_devices PRIVATE -D__HIP_PLATFORM_NVCC__)
    target_include_directories(hip_get_devices PRIVATE ${HIP_ROOT_DIR}/include)
    target_include_directories(hip_get_devices PRIVATE ${CUDA_INCLUDE_DIRS})
    target_link_libraries(hip_get_devices PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY})
  elseif(HIP_PLATFORM STREQUAL "hcc")
    target_compile_definitions(gpu PRIVATE -D__HIP_PLATFORM_HCC__)
    target_include_directories(gpu PRIVATE ${HIP_ROOT_DIR}/../include)

    target_compile_definitions(hip_get_devices PRIVATE -D__HIP_PLATFORM_HCC__)
    target_include_directories(hip_get_devices PRIVATE ${HIP_ROOT_DIR}/../include)
  endif()

  target_link_libraries(lammps PRIVATE gpu)
endif()

# GPU package
FindStyleHeaders(${GPU_SOURCES_DIR} FIX_CLASS fix_ FIX)

set_property(GLOBAL PROPERTY "GPU_SOURCES" "${GPU_SOURCES}")

# detects styles which have GPU version
RegisterStylesExt(${GPU_SOURCES_DIR} gpu GPU_SOURCES)

get_property(GPU_SOURCES GLOBAL PROPERTY GPU_SOURCES)

if(NOT BUILD_MPI)
  # mpistubs is aliased to MPI::MPI_CXX, but older versions of cmake won't work forward the include path
  target_link_libraries(gpu PRIVATE mpi_stubs)
else()
  target_link_libraries(gpu PRIVATE MPI::MPI_CXX)
endif()
if(NOT BUILD_SHARED_LIBS)
  install(TARGETS gpu EXPORT LAMMPS_Targets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
target_compile_definitions(gpu PRIVATE -DLAMMPS_${LAMMPS_SIZES})
set_target_properties(gpu PROPERTIES OUTPUT_NAME lammps_gpu${LAMMPS_MACHINE})
target_sources(lammps PRIVATE ${GPU_SOURCES})
target_include_directories(lammps PRIVATE ${GPU_SOURCES_DIR})
