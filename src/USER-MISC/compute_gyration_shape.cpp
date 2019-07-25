/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
 *    Contributing author:  Evangelos Voyiatzis (Royal DSM)
 * ------------------------------------------------------------------------- */


#include <cmath>
#include <cstring>
#include "compute_gyration_shape.h"
#include "math_extra.h"
#include "update.h"
#include "atom.h"
#include "group.h"
#include "domain.h"
#include "error.h"
#include "modify.h"
#include "compute.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

ComputeGyrationShape::ComputeGyrationShape(LAMMPS *lmp, int narg, char **arg) :
  Compute(lmp, narg, arg), id_gyration(NULL)
{
  if (narg != 4) error->all(FLERR,"Illegal compute gyration/shape command");

  vector_flag = 1;
  size_vector = 6;
  extscalar = 0;
  extvector = 0;

  // ID of compute gyration 
  int n = strlen(arg[3]) + 1;
  id_gyration = new char[n];
  strcpy(id_gyration,arg[3]);

  init();

  vector = new double[6];
}

/* ---------------------------------------------------------------------- */

ComputeGyrationShape::~ComputeGyrationShape()
{
  delete [] id_gyration;
  delete [] vector;
}

/* ---------------------------------------------------------------------- */

void ComputeGyrationShape::init()
{
  // check that the compute gyration command exist
  int icompute = modify->find_compute(id_gyration);
  if (icompute < 0)
    error->all(FLERR,"Compute gyration does not exist for compute gyration/shape");

  // check the id_gyration corresponds really to a compute gyration command
  c_gyration = (Compute *) modify->compute[icompute];
  if (strcmp(c_gyration->style,"gyration") != 0)
    error->all(FLERR,"Compute gyration/shape does not use gyration compute");
}

/* ----------------------------------------------------------------------
   compute shape parameters based on the eigenvalues of the gyration tensor of group of atoms
------------------------------------------------------------------------- */

void ComputeGyrationShape::compute_vector()
{
  invoked_vector = update->ntimestep;

  // get the gyration tensor from the compute gyration
  int icompute = modify->find_compute(id_gyration);
  Compute *compute = modify->compute[icompute];
  compute->compute_vector();
  double *gyration_tensor = compute->vector;

  // call the function for the calculation of the eigenvalues
  double ione[3][3], evalues[3], evectors[3][3];

  ione[0][0] = gyration_tensor[0];
  ione[1][1] = gyration_tensor[1];
  ione[2][2] = gyration_tensor[2];
  ione[0][1] = ione[1][0] = gyration_tensor[3];
  ione[1][2] = ione[2][1] = gyration_tensor[4];
  ione[0][2] = ione[2][0] = gyration_tensor[5];

  int ierror = MathExtra::jacobi(ione,evalues,evectors);
  if (ierror) error->all(FLERR, "Insufficient Jacobi rotations for gyration/shape");

  // sort the eigenvalues according to their size with bubble sort
  double t;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 2-i; j++) {
      if (fabs(evalues[j]) < fabs(evalues[j+1])) {
        t = evalues[j];
        evalues[j] = evalues[j+1];
        evalues[j+1] = t;
      }
    }
  }

  // compute the shape parameters of the gyration tensor
  double sq_eigen_x = pow(evalues[0], 2);
  double sq_eigen_y = pow(evalues[1], 2);
  double sq_eigen_z = pow(evalues[2], 2);

  double nominator = pow(sq_eigen_x, 2) + pow(sq_eigen_y, 2) + pow(sq_eigen_z, 2);
  double denominator = pow(sq_eigen_x+sq_eigen_y+sq_eigen_z, 2);

  vector[0] = evalues[0];
  vector[1] = evalues[1];
  vector[2] = evalues[2];
  vector[3] = sq_eigen_z - 0.5*(sq_eigen_x + sq_eigen_y);
  vector[4] = sq_eigen_y - sq_eigen_x;
  vector[5] = 0.5*(3*nominator/denominator -1);

}

