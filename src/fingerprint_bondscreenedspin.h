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
#ifdef FINGERPRINT_CLASS

FingerprintStyle(bondscreenedspin,Fingerprint_bondscreenedspin)

#else

#ifndef FINGERPRINT_BONDSCREENEDSPIN_H_
#define FINGERPRINT_BONDSCREENEDSPIN_H_

#include "fingerprint.h"

namespace LAMMPS_NS {

class Fingerprint_bondscreenedspin : public Fingerprint {
 public:
	Fingerprint_bondscreenedspin(PairRANN *);
	~Fingerprint_bondscreenedspin();
	bool parse_values(char*,char*);
	void write_values(FILE *);
	void init(int*,int);
	void allocate();
	void compute_fingerprint(double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,bool*,int,int,double*,double*,double*,int*,int,int*);
	void do3bodyfeatureset_doubleneighborloop(double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,bool*,int,int,double*,double*,double*,int*,int,int *);
	void do3bodyfeatureset_singleneighborloop(double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,double*,bool*,int,int,double*,double*,double*,int*,int,int*);
	int factorial(int);
	void generate_exp_cut_table();
	void generate_coefficients();
	int get_length();

	double *expcuttable;
    double *dfctable;
    double dr;
    double *alpha_k;
    double re;
    int **coeff;
    int **coeffx;
    int **coeffy;
    int **coeffz;
    int k;
    int m;
	int **Mf;

};


}


#endif
#endif /* FINGERPRINT_BOND_H_ */
