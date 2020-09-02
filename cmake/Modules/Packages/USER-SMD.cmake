find_package(Eigen3 NO_MODULE)
if(EIGEN3_FOUND)
  set(DOWNLOAD_EIGEN3_DEFAULT OFF)
else()
  set(DOWNLOAD_EIGEN3_DEFAULT ON)
endif()
option(DOWNLOAD_EIGEN3 "Download Eigen3 instead of using an already installed one)" ${DOWNLOAD_EIGEN3_DEFAULT})
if(DOWNLOAD_EIGEN3)
  message(STATUS "Eigen3 download requested - we will build our own")
  include(ExternalProject)
  ExternalProject_Add(Eigen3_build
    URL https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.tar.gz
    URL_MD5 9e30f67e8531477de4117506fe44669b
    CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND ""
  )
  ExternalProject_get_property(Eigen3_build SOURCE_DIR)
  add_library(LAMMPS::EIGEN3 INTERFACE IMPORTED)
  set_target_properties(LAMMPS::EIGEN3 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SOURCE_DIR}")
  target_link_libraries(lammps PRIVATE LAMMPS::EIGEN3)
  add_dependencies(LAMMPS::EIGEN3 Eigen3_build)
else()
  find_package(Eigen3 NO_MODULE)
  mark_as_advanced(Eigen3_DIR)
  if(NOT EIGEN3_FOUND)
    message(FATAL_ERROR "Eigen3 not found, help CMake to find it by setting EIGEN3_INCLUDE_DIR, or set DOWNLOAD_EIGEN3=ON to download it")
  endif()
  target_link_libraries(lammps PRIVATE Eigen3::Eigen)
endif()
