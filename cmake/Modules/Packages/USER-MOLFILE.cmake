set(MOLFILE_INCLUDE_DIRS "${LAMMPS_LIB_SOURCE_DIR}/molfile" CACHE STRING "Path to VMD molfile plugin headers")
add_library(molfile INTERFACE)
if(NOT BUILD_SHARED_LIBS)
  install(TARGETS molfile EXPORT LAMMPS_Targets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
target_include_directories(molfile INTERFACE ${MOLFILE_INCLUDE_DIRS})
# no need to link with -ldl on windows
if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  target_link_libraries(molfile INTERFACE ${CMAKE_DL_LIBS})
endif()
target_link_libraries(lammps PRIVATE molfile)
