/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef MINIMIZE_CLASS

MinimizeStyle(spin/oso_lbfgs, MinSpinOSO_LBFGS)

#else

#ifndef LMP_MIN_SPIN_OSO_LBFGS_H
#define LMP_MIN_SPIN_OSO_LBFGS_H

#include "min.h"

namespace LAMMPS_NS {

class MinSpinOSO_LBFGS : public Min {

public:
    MinSpinOSO_LBFGS(class LAMMPS *);
    ~MinSpinOSO_LBFGS() {}
    void init();
    void setup_style();
    int modify_param(int, char **);
    void reset_vectors();
    int iterate(int);
    double evaluate_dt();
    void advance_spins();
    double fmnorm_sqr();
    void calc_gradient(double);
    void calc_search_direction(int);

private:
    // global and spin timesteps

    double dt;
    double dts;

    double alpha_damp;		// damping for spin minimization
    double discrete_factor;	// factor for spin timestep evaluation

    double *spvec;		// variables for atomic dof, as 1d vector
    double *fmvec;		// variables for atomic dof, as 1d vector

    double *g;  		// gradient vector
    double *g_old;  	// gradient vector at previous step
    double *p;  		// search direction vector
    double **ds;  		// change in rotation matrix between two iterations, da
    double **dy;        // change in gradients between two iterations, dg
    double *rho;        // estimation of curvature
    int num_mem;  // number of stored steps


    void vm3(const double *m, const double *v, double *out);
    void rodrigues_rotation(const double *upp_tr, double *out);

    bigint last_negative;
};

}

#endif
#endif
