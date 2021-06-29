set(WIN_PACKAGES ASPHERE BODY CLASS2 COLLOID COMPRESS CORESHELL DIPOLE GPU
                 GRANULAR KSPACE LATTE MANYBODY MC MISC ML-IAP MOLECULE OPT
                 PERI POEMS QEQ REPLICA RIGID SHOCK ML-SNAP SPIN SRD VORONOI
                 USER-ATC USER-AWPMD USER-BOCS USER-BROWNIAN CG-DNA CG-SDK
                 USER-COLVARS USER-DIFFRACTION USER-DPD USER-DRUDE USER-EFF USER-FEP
                 ML-HDNNP USER-INTEL USER-MANIFOLD USER-MDI MEAM USER-MESODPD
                 USER-MESONT USER-MISC USER-MGPT USER-MOFFF USER-MOLFILE OPENMP
                 USER-PHONON USER-PTM USER-QTB USER-REACTION USER-REAXC
                 USER-SDPD MACHDYN USER-SMTBQ USER-SPH USER-TALLY USER-UEF
                 USER-YAFF USER-DIELECTRIC)

foreach(PKG ${WIN_PACKAGES})
  set(PKG_${PKG} ON CACHE BOOL "" FORCE)
endforeach()

# these two packages require a full MPI implementation
if(BUILD_MPI)
  set(PKG_MPIIO ON CACHE BOOL "" FORCE)
  set(PKG_LATBOLTZ ON CACHE BOOL "" FORCE)
endif()

set(DOWNLOAD_VORO ON CACHE BOOL "" FORCE)
set(DOWNLOAD_EIGEN3 ON CACHE BOOL "" FORCE)
set(LAMMPS_MEMALIGN "0" CACHE STRING "" FORCE)
set(CMAKE_TUNE_FLAGS "-Wno-missing-include-dirs" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--enable-stdcall-fixup,--as-needed,-lssp" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--enable-stdcall-fixup,--as-needed,-lssp" CACHE STRING "" FORCE)
set(BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/lammps-installer")
