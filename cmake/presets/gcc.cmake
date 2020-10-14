# preset that will restore gcc/g++ with support for MPI and OpenMP (on Linux boxes)

set(CMAKE_CXX_COMPILER "g++" CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER "gcc" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -g" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Wall -Wextra -g -O2 -DNDEBUG" CACHE STRING "" FORCE)
set(MPI_CXX "g++" CACHE STRING "" FORCE)
set(MPI_CXX_COMPILER "mpicxx" CACHE STRING "" FORCE)
unset(HAVE_OMP_H_INCLUDE CACHE)

set(OpenMP_C "gcc" CACHE STRING "" FORCE)
set(OpenMP_C_FLAGS "-fopenmp" CACHE STRING "" FORCE)
set(OpenMP_C_LIB_NAMES "gomp" CACHE STRING "" FORCE)
set(OpenMP_CXX "g++" CACHE STRING "" FORCE)
set(OpenMP_CXX_FLAGS "-fopenmp" CACHE STRING "" FORCE)
set(OpenMP_CXX_LIB_NAMES "gomp" CACHE STRING "" FORCE)
set(OpenMP_omp_LIBRARY "libgomp.so" CACHE PATH "" FORCE)
