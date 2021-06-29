# preset that turns on all existing packages off. can be used to reset
# an existing package selection without losing any other settings

set(ALL_PACKAGES ASPHERE BODY CLASS2 COLLOID COMPRESS CORESHELL DIPOLE GPU
  GRANULAR KIM KOKKOS KSPACE LATTE MANYBODY MC MESSAGE MISC ML-IAP MOLECULE
  MPIIO MSCG OPT PERI PLUGIN POEMS PYTHON QEQ REPLICA RIGID SHOCK ML-SNAP SPIN
  SRD VORONOI
  USER-ADIOS USER-ATC USER-AWPMD USER-BROWNIAN USER-BOCS CG-DNA CG-SDK
  USER-COLVARS USER-DIFFRACTION USER-DPD USER-DRUDE USER-EFF USER-FEP USER-H5MD
  ML-HDNNP USER-INTEL LATBOLTZ USER-MANIFOLD USER-MDI MEAM USER-MESODPD
  USER-MESONT USER-MGPT USER-MISC USER-MOFFF USER-MOLFILE USER-NETCDF OPENMP
  ML-PACE USER-PHONON USER-PLUMED USER-PTM USER-QMMM USER-QTB ML-QUIP ML-RANN
  USER-REACTION REAXFF USER-SCAFACOS USER-SDPD MACHDYN USER-SMTBQ USER-SPH
  USER-TALLY USER-UEF USER-VTK USER-YAFF USER-DIELECTRIC)

foreach(PKG ${ALL_PACKAGES})
  set(PKG_${PKG} OFF CACHE BOOL "" FORCE)
endforeach()
