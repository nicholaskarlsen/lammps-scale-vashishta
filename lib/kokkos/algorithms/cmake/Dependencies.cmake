TRIBITS_PACKAGE_DEFINE_DEPENDENCIES(
  LIB_REQUIRED_PACKAGES KokkosCore KokkosContainers
  LIB_OPTIONAL_TPLS Pthread CUDA HWLOC
  TEST_OPTIONAL_TPLS CUSPARSE
  )
