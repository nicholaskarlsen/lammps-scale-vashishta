/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov
------------------------------------------------------------------------- */

#ifndef MC_H
#define MC_H

/* ---------------------------------------------------------------------- */

class MC {
 public:
  int naccept;           // # of accepted MC events
  int nattempt;          // # of attempted MC events

  MC(char *, class CSlib *);
  ~MC();
  void run();

 private:
  int nsteps;            // total # of MD steps
  int ndynamics;         // steps in one short dynamics run
  int nloop;             // nsteps/ndynamics
  int natoms;            // # of MD atoms

  double delta;          // MC displacement distance
  double temperature;    // MC temperature for Boltzmann criterion
  double *x;             // atom coords as 3N 1d vector
  double energy;         // global potential energy

  int seed;              // RNG seed
  class RanPark *random;

  class CSlib *cs;              // messaging library

  void options(char *);
};

#endif
