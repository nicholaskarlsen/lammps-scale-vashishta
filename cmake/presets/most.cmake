# preset that turns on a wide range of packages, some of which require
# external libraries. Compared to all_on.cmake some more unusual packages
# are removed. The resulting binary should be able to run most inputs.

set(ALL_PACKAGES ASPHERE BODY CLASS2 COLLOID COMPRESS CORESHELL DIPOLE
        GRANULAR KSPACE MANYBODY MC MISC ML-IAP MOLECULE OPT PERI
        PLUGIN POEMS PYTHON QEQ REPLICA RIGID SHOCK ML-SNAP SPIN SRD VORONOI
        USER-BROWNIAN USER-BOCS CG-DNA CG-SDK USER-COLVARS
        USER-DIFFRACTION USER-DPD USER-DRUDE USER-EFF USER-FEP MEAM
        USER-MESODPD USER-MISC USER-MOFFF USER-OMP USER-PHONON USER-REACTION
        USER-REAXC USER-SDPD USER-SPH USER-SMD USER-UEF USER-YAFF USER-DIELECTRIC)

foreach(PKG ${ALL_PACKAGES})
  set(PKG_${PKG} ON CACHE BOOL "" FORCE)
endforeach()

set(BUILD_TOOLS ON CACHE BOOL "" FORCE)
