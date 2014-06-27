/***************************************************************************
                                    gauss.h
                             -------------------
                            Trung Dac Nguyen (ORNL)

  Class for acceleration of the gauss pair style.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                : 
    email                : nguyentd@ornl.gov
 ***************************************************************************/

#ifndef LAL_GAUSS_H
#define LAL_GAUSS_H

#include "lal_base_atomic.h"

namespace LAMMPS_AL {

template <class numtyp, class acctyp>
class Gauss : public BaseAtomic<numtyp, acctyp> {
 public:
  Gauss();
  ~Gauss(); 

  /// Clear any previous data and set up for a new LAMMPS run
  /** \param max_nbors initial number of rows in the neighbor matrix
    * \param cell_size cutoff + skin
    * \param gpu_split fraction of particles handled by device
    * 
    * Returns:
    * -  0 if successfull
    * - -1 if fix gpu not found
    * - -3 if there is an out of memory error
    * - -4 if the GPU library was not compiled for GPU
    * - -5 Double precision is not supported on card **/
  int init(const int ntypes, double **host_cutsq,
           double **host_a, double **host_b, double **host_offset, 
           double *host_special_lj,
           const int nlocal, const int nall, const int max_nbors, 
           const int maxspecial, const double cell_size, 
           const double gpu_split, FILE *screen);
  
  /// Send updated coeffs from host to device (to be compatible with fix adapt)
  void reinit(const int ntypes, double **host_cutsq,
              double **host_a, double **host_b, double **host_offset);
           
  /// Clear all host and device data
  /** \note This is called at the beginning of the init() routine **/
  void clear();

  /// Returns memory usage on device per atom
  int bytes_per_atom(const int max_nbors) const;

  /// Total host memory used by library for pair style
  double host_memory_usage() const;

  // --------------------------- TYPE DATA --------------------------

  /// gauss1.x = a, gauss1.y = b, gauss1.z = cutsq, gauss1.w = offset
  UCL_D_Vec<numtyp4> gauss1;
  /// Special LJ values
  UCL_D_Vec<numtyp> sp_lj;

  /// If atom type constants fit in shared memory, use fast kernels
  bool shared_types;

  /// Number of atom types 
  int _lj_types;

 private:
  bool _allocated;
  void loop(const bool _eflag, const bool _vflag);
};

}

#endif
