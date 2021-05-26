
/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/ Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */
/*  ----------------------------------------------------------------------
   Contributing authors: Christopher Barrett (MSU) barrett@me.msstate.edu
                              Doyl Dickel (MSU) doyl@me.msstate.edu
    ----------------------------------------------------------------------*/
/*
“The research described and the resulting data presented herein, unless
otherwise noted, was funded under PE 0602784A, Project T53 "Military
Engineering Applied Research", Task 002 under Contract No. W56HZV-17-C-0095,
managed by the U.S. Army Combat Capabilities Development Command (CCDC) and
the Engineer Research and Development Center (ERDC).  The work described in
this document was conducted at CAVS, MSU.  Permission was granted by ERDC
to publish this information. Any opinions, findings and conclusions or
recommendations expressed in this material are those of the author(s) and
do not necessarily reflect the views of the United States Army.​”

DISTRIBUTION A. Approved for public release; distribution unlimited. OPSEC#4918
 */

#ifndef LMP_RANN_FINGERPRINT_RADIAL_H
#define LMP_RANN_FINGERPRINT_RADIAL_H

#include "rann_fingerprint.h"

namespace LAMMPS_NS {
  namespace RANN {

    class Fingerprint_radial : public Fingerprint {
     public:
      Fingerprint_radial(PairRANN *);
      ~Fingerprint_radial();
      bool parse_values(std::string,std::vector<std::string>);
      void write_values(FILE *);
      void init(int*,int);
      void allocate();
      void compute_fingerprint(double*,double*,double*,double*,int,int,double*,double*,double*,int*,int,int*);
      int get_length();

      double *radialtable;
      double *dfctable;
      double dr;
      double *alpha;
      double re;
      int nmax;//highest term
      int omin;//lowest term

    };

  }
}

#endif /* FINGERPRINT_RADIAL_H_ */
