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

/*  ----------------------------------------------------------------------
   Contributing author: Christopher Barrett (MSU) barrett@me.msstate.edu
*/

#ifdef ACTIVATION_CLASS

ActivationStyle(sigI,Activation_sigI)

#else

#ifndef ACTIVATION_SIGI_H_
#define ACTIVATION_SIGI_H_

#include "activation.h"

namespace LAMMPS_NS {

	class Activation_sigI : public Activation {
	public:
		Activation_sigI(class PairRANN *);
		double activation_function(double);
		double dactivation_function(double);
		double ddactivation_function(double);
	};
}


#endif
#endif /* ACTIVATION_SIGI_H_ */
