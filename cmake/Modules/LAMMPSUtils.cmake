# Utility functions
function(list_to_bulletpoints result)
    list(REMOVE_AT ARGV 0)
    set(temp "")
    foreach(item ${ARGV})
        set(temp "${temp}* ${item}\n")
    endforeach()
    set(${result} "${temp}" PARENT_SCOPE)
endfunction(list_to_bulletpoints)

function(validate_option name values)
    string(TOLOWER ${${name}} needle_lower)
    string(TOUPPER ${${name}} needle_upper)
    list(FIND ${values} ${needle_lower} IDX_LOWER)
    list(FIND ${values} ${needle_upper} IDX_UPPER)
    if(${IDX_LOWER} LESS 0 AND ${IDX_UPPER} LESS 0)
        list_to_bulletpoints(POSSIBLE_VALUE_LIST ${${values}})
        message(FATAL_ERROR "\n########################################################################\n"
                            "Invalid value '${${name}}' for option ${name}\n"
                            "\n"
                            "Possible values are:\n"
                            "${POSSIBLE_VALUE_LIST}"
                            "########################################################################")
    endif()
endfunction(validate_option)

function(get_lammps_version version_header variable)
    file(READ ${version_header} line)
    set(MONTHS x Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec)
    string(REGEX REPLACE "#define LAMMPS_VERSION \"([0-9]+) ([A-Za-z]+) ([0-9]+)\"" "\\1" day "${line}")
    string(REGEX REPLACE "#define LAMMPS_VERSION \"([0-9]+) ([A-Za-z]+) ([0-9]+)\"" "\\2" month "${line}")
    string(REGEX REPLACE "#define LAMMPS_VERSION \"([0-9]+) ([A-Za-z]+) ([0-9]+)\"" "\\3" year "${line}")
    string(STRIP ${day} day)
    string(STRIP ${month} month)
    string(STRIP ${year} year)
    list(FIND MONTHS "${month}" month)
    string(LENGTH ${day} day_length)
    string(LENGTH ${month} month_length)
    if(day_length EQUAL 1)
        set(day "0${day}")
    endif()
    if(month_length EQUAL 1)
        set(month "0${month}")
    endif()
    set(${variable} "${year}${month}${day}" PARENT_SCOPE)
endfunction()

function(check_for_autogen_files source_dir)
    message(STATUS "Running check for auto-generated files from make-based build system")
    file(GLOB SRC_AUTOGEN_FILES ${source_dir}/style_*.h)
    file(GLOB SRC_AUTOGEN_PACKAGES ${source_dir}/packages_*.h)
    list(APPEND SRC_AUTOGEN_FILES ${SRC_AUTOGEN_PACKAGES} ${source_dir}/lmpinstalledpkgs.h ${source_dir}/lmpgitversion.h)
    foreach(_SRC ${SRC_AUTOGEN_FILES})
      get_filename_component(FILENAME "${_SRC}" NAME)
      if(EXISTS ${source_dir}/${FILENAME})
        message(FATAL_ERROR "\n########################################################################\n"
                              "Found header file(s) generated by the make-based build system\n"
                              "\n"
                              "Please run\n"
                              "make -C ${source_dir} purge\n"
                              "to remove\n"
                              "########################################################################")
      endif()
    endforeach()
endfunction()

macro(pkg_depends PKG1 PKG2)
  if(PKG_${PKG1} AND NOT (PKG_${PKG2} OR BUILD_${PKG2}))
    message(FATAL_ERROR "${PKG1} package needs LAMMPS to be build with ${PKG2}")
  endif()
endmacro()

# CMake-only replacement for bin2c and xxd
function(GenerateBinaryHeader varname outfile infile)
    message("Creating ${outfile}...")
    file(WRITE ${outfile} "// CMake generated file\n")

    file(READ ${infile} content HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," content "${content}")
    string(REGEX REPLACE ",$" "" content "${content}")
    file(APPEND ${outfile} "const unsigned char ${varname}[] = { ${content} };\n")
    file(APPEND ${outfile} "const unsigned int ${varname}_size = sizeof(${varname});\n")
endfunction(GenerateBinaryHeader)

# fetch missing potential files
function(FetchPotentials pkgfolder potfolder)
  if (EXISTS "${pkgfolder}/potentials.txt")
    set(LAMMPS_POTENTIALS_URL "https://download.lammps.org/potentials")
    file(STRINGS "${pkgfolder}/potentials.txt" linelist REGEX "^[^#].")
    foreach(line ${linelist})
      string(FIND ${line} " " blank)
      math(EXPR plusone "${blank}+1")
      string(SUBSTRING ${line} 0 ${blank} pot)
      string(SUBSTRING ${line} ${plusone} -1 sum)
      if(EXISTS ${LAMMPS_POTENTIALS_DIR}/${pot})
        file(MD5 "${LAMMPS_POTENTIALS_DIR}/${pot}" oldsum)
      endif()
      if(NOT sum STREQUAL oldsum)
        message(STATUS "Checking external potential ${pot} from ${LAMMPS_POTENTIALS_URL}")
        file(DOWNLOAD "${LAMMPS_POTENTIALS_URL}/${pot}.${sum}" "${CMAKE_BINARY_DIR}/${pot}"
          EXPECTED_HASH MD5=${sum} SHOW_PROGRESS)
        file(COPY "${CMAKE_BINARY_DIR}/${pot}" DESTINATION ${LAMMPS_POTENTIALS_DIR})
      endif()
    endforeach()
  endif()
endfunction(FetchPotentials)
