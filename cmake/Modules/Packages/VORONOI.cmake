if(PKG_VORONOI)
  find_package(VORO)
  if(VORO_FOUND)
    set(DOWNLOAD_VORO_DEFAULT OFF)
  else()
    set(DOWNLOAD_VORO_DEFAULT ON)
  endif()
  option(DOWNLOAD_VORO "Download and compile the Voro++ library instead of using an already installed one" ${DOWNLOAD_VORO_DEFAULT})
  if(DOWNLOAD_VORO)
    if(CMAKE_GENERATOR STREQUAL "Ninja")
      message(FATAL_ERROR "Cannot build downloaded Voro++ library with Ninja build tool")
    endif()
    message(STATUS "Voro++ download requested - we will build our own")
    include(ExternalProject)

    if(BUILD_SHARED_LIBS)
      set(VORO_BUILD_CFLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS} ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BTYPE}}")
    else()
      set(VORO_BUILD_CFLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BTYPE}}")
    endif()
    string(APPEND VORO_BUILD_CFLAGS ${CMAKE_CXX_FLAGS})
    set(VORO_BUILD_OPTIONS CXX=${CMAKE_CXX_COMPILER} CFLAGS=${VORO_BUILD_CFLAGS})

    ExternalProject_Add(voro_build
      URL https://download.lammps.org/thirdparty/voro++-0.4.6.tar.gz
      URL_MD5 2338b824c3b7b25590e18e8df5d68af9
      CONFIGURE_COMMAND "" BUILD_COMMAND make ${VORO_BUILD_OPTIONS} BUILD_IN_SOURCE 1 INSTALL_COMMAND ""
      )
    ExternalProject_get_property(voro_build SOURCE_DIR)
    set(VORO_LIBRARIES ${SOURCE_DIR}/src/libvoro++.a)
    set(VORO_INCLUDE_DIRS ${SOURCE_DIR}/src)
    list(APPEND LAMMPS_DEPS voro_build)
  else()
    find_package(VORO)
    if(NOT VORO_FOUND)
      message(FATAL_ERROR "Voro++ library not found. Help CMake to find it by setting VORO_LIBRARY and VORO_INCLUDE_DIR, or set DOWNLOAD_VORO=ON to download it")
    endif()
  endif()
  include_directories(${VORO_INCLUDE_DIRS})
  list(APPEND LAMMPS_LINK_LIBS ${VORO_LIBRARIES})
endif()
