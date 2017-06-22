#include "meam.h"
#include <math.h>
#include "math_special.h"

using namespace LAMMPS_NS;
// Extern "C" declaration has the form:
//
//  void meam_dens_final_(int *, int *, int *, int *, int *, double *, double *,
//                int *, int *, int *,
//		 double *, double *, double *, double *, double *, double *,
//		 double *, double *, double *, double *, double *, double *,
//		 double *, double *, double *, double *, double *, int *);
//
// Call from pair_meam.cpp has the form:
//
//  meam_dens_final_(&nlocal,&nmax,&eflag_either,&eflag_global,&eflag_atom,
//            &eng_vdwl,eatom,ntype,type,fmap,
//	     &arho1[0][0],&arho2[0][0],arho2b,&arho3[0][0],
//	     &arho3b[0][0],&t_ave[0][0],&tsq_ave[0][0],gamma,dgamma1,
//	     dgamma2,dgamma3,rho,rho0,rho1,rho2,rho3,frhop,&errorflag);
//

void
MEAM::meam_dens_final(int* nlocal, int* eflag_either, int* eflag_global,
                 int* eflag_atom, double* eng_vdwl, double* eatom, int* ntype,
                 int* type, int* fmap, int* errorflag)
{
  int i, elti;
  int m;
  double rhob, G, dG, Gbar, dGbar, gam, shp[3 + 1], Z;
  double B, denom, rho_bkgd;

  //     Complete the calculation of density

  for (i = 1; i <= *nlocal; i++) {
    elti = fmap[arr1v(type, i)];
    if (elti >= 0) {
      arr1v(rho1, i) = 0.0;
      arr1v(rho2, i) = -1.0 / 3.0 * arr1v(arho2b, i) * arr1v(arho2b, i);
      arr1v(rho3, i) = 0.0;
      for (m = 1; m <= 3; m++) {
        arr1v(rho1, i) =
          arr1v(rho1, i) + arr2v(arho1, m, i) * arr2v(arho1, m, i);
        arr1v(rho3, i) = arr1v(rho3, i) -
                         3.0 / 5.0 * arr2v(arho3b, m, i) * arr2v(arho3b, m, i);
      }
      for (m = 1; m <= 6; m++) {
        arr1v(rho2, i) =
          arr1v(rho2, i) +
          this->v2D[m] * arr2v(arho2, m, i) * arr2v(arho2, m, i);
      }
      for (m = 1; m <= 10; m++) {
        arr1v(rho3, i) =
          arr1v(rho3, i) +
          this->v3D[m] * arr2v(arho3, m, i) * arr2v(arho3, m, i);
      }

      if (arr1v(rho0, i) > 0.0) {
        if (this->ialloy == 1) {
          arr2v(t_ave, 1, i) = arr2v(t_ave, 1, i) / arr2v(tsq_ave, 1, i);
          arr2v(t_ave, 2, i) = arr2v(t_ave, 2, i) / arr2v(tsq_ave, 2, i);
          arr2v(t_ave, 3, i) = arr2v(t_ave, 3, i) / arr2v(tsq_ave, 3, i);
        } else if (this->ialloy == 2) {
          arr2v(t_ave, 1, i) = this->t1_meam[elti];
          arr2v(t_ave, 2, i) = this->t2_meam[elti];
          arr2v(t_ave, 3, i) = this->t3_meam[elti];
        } else {
          arr2v(t_ave, 1, i) = arr2v(t_ave, 1, i) / arr1v(rho0, i);
          arr2v(t_ave, 2, i) = arr2v(t_ave, 2, i) / arr1v(rho0, i);
          arr2v(t_ave, 3, i) = arr2v(t_ave, 3, i) / arr1v(rho0, i);
        }
      }

      arr1v(gamma, i) = arr2v(t_ave, 1, i) * arr1v(rho1, i) +
                        arr2v(t_ave, 2, i) * arr1v(rho2, i) +
                        arr2v(t_ave, 3, i) * arr1v(rho3, i);

      if (arr1v(rho0, i) > 0.0) {
        arr1v(gamma, i) = arr1v(gamma, i) / (arr1v(rho0, i) * arr1v(rho0, i));
      }

      Z = this->Z_meam[elti];

      G_gam(arr1v(gamma, i), this->ibar_meam[elti], &G, errorflag);
      if (*errorflag != 0)
        return;
      get_shpfcn(shp, this->lattce_meam[elti][elti]);
      if (this->ibar_meam[elti] <= 0) {
        Gbar = 1.0;
        dGbar = 0.0;
      } else {
        if (this->mix_ref_t == 1) {
          gam = (arr2v(t_ave, 1, i) * shp[1] + arr2v(t_ave, 2, i) * shp[2] +
                 arr2v(t_ave, 3, i) * shp[3]) /
                (Z * Z);
        } else {
          gam = (this->t1_meam[elti] * shp[1] +
                 this->t2_meam[elti] * shp[2] +
                 this->t3_meam[elti] * shp[3]) /
                (Z * Z);
        }
        G_gam(gam, this->ibar_meam[elti], &Gbar, errorflag);
      }
      arr1v(rho, i) = arr1v(rho0, i) * G;

      if (this->mix_ref_t == 1) {
        if (this->ibar_meam[elti] <= 0) {
          Gbar = 1.0;
          dGbar = 0.0;
        } else {
          gam = (arr2v(t_ave, 1, i) * shp[1] + arr2v(t_ave, 2, i) * shp[2] +
                 arr2v(t_ave, 3, i) * shp[3]) /
                (Z * Z);
          dG_gam(gam, this->ibar_meam[elti], &Gbar, &dGbar);
        }
        rho_bkgd = this->rho0_meam[elti] * Z * Gbar;
      } else {
        if (this->bkgd_dyn == 1) {
          rho_bkgd = this->rho0_meam[elti] * Z;
        } else {
          rho_bkgd = this->rho_ref_meam[elti];
        }
      }
      rhob = arr1v(rho, i) / rho_bkgd;
      denom = 1.0 / rho_bkgd;

      dG_gam(arr1v(gamma, i), this->ibar_meam[elti], &G, &dG);

      arr1v(dgamma1, i) = (G - 2 * dG * arr1v(gamma, i)) * denom;

      if (!iszero(arr1v(rho0, i))) {
        arr1v(dgamma2, i) = (dG / arr1v(rho0, i)) * denom;
      } else {
        arr1v(dgamma2, i) = 0.0;
      }

      //     dgamma3 is nonzero only if we are using the "mixed" rule for
      //     computing t in the reference system (which is not correct, but
      //     included for backward compatibility
      if (this->mix_ref_t == 1) {
        arr1v(dgamma3, i) = arr1v(rho0, i) * G * dGbar / (Gbar * Z * Z) * denom;
      } else {
        arr1v(dgamma3, i) = 0.0;
      }

      B = this->A_meam[elti] * this->Ec_meam[elti][elti];

      if (!iszero(rhob)) {
        if (this->emb_lin_neg == 1 && rhob <= 0) {
          arr1v(frhop, i) = -B;
        } else {
          arr1v(frhop, i) = B * (log(rhob) + 1.0);
        }
        if (*eflag_either != 0) {
          if (*eflag_global != 0) {
            if (this->emb_lin_neg == 1 && rhob <= 0) {
              *eng_vdwl = *eng_vdwl - B * rhob;
            } else {
              *eng_vdwl = *eng_vdwl + B * rhob * log(rhob);
            }
          }
          if (*eflag_atom != 0) {
            if (this->emb_lin_neg == 1 && rhob <= 0) {
              arr1v(eatom, i) = arr1v(eatom, i) - B * rhob;
            } else {
              arr1v(eatom, i) = arr1v(eatom, i) + B * rhob * log(rhob);
            }
          }
        }
      } else {
        if (this->emb_lin_neg == 1) {
          arr1v(frhop, i) = -B;
        } else {
          arr1v(frhop, i) = B;
        }
      }
    }
  }
}

// ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc

void
MEAM::G_gam(double gamma, int ibar, double* G, int* errorflag)
{
  //     Compute G(gamma) based on selection flag ibar:
  //   0 => G = sqrt(1+gamma)
  //   1 => G = exp(gamma/2)
  //   2 => not implemented
  //   3 => G = 2/(1+exp(-gamma))
  //   4 => G = sqrt(1+gamma)
  //  -5 => G = +-sqrt(abs(1+gamma))

  double gsmooth_switchpoint;
  if (ibar == 0 || ibar == 4) {
    gsmooth_switchpoint = -gsmooth_factor / (gsmooth_factor + 1);
    if (gamma < gsmooth_switchpoint) {
      //         e.g. gsmooth_factor is 99, {:
      //         gsmooth_switchpoint = -0.99
      //         G = 0.01*(-0.99/gamma)**99
      *G = 1 / (gsmooth_factor + 1) *
           pow((gsmooth_switchpoint / gamma), gsmooth_factor);
      *G = sqrt(*G);
    } else {
      *G = sqrt(1.0 + gamma);
    }
  } else if (ibar == 1) {
    *G = MathSpecial::fm_exp(gamma / 2.0);
  } else if (ibar == 3) {
    *G = 2.0 / (1.0 + exp(-gamma));
  } else if (ibar == -5) {
    if ((1.0 + gamma) >= 0) {
      *G = sqrt(1.0 + gamma);
    } else {
      *G = -sqrt(-1.0 - gamma);
    }
  } else {
    *errorflag = 1;
  }
}

// ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc

void
MEAM::dG_gam(double gamma, int ibar, double* G, double* dG)
{
  // Compute G(gamma) and dG(gamma) based on selection flag ibar:
  //   0 => G = sqrt(1+gamma)
  //   1 => G = MathSpecial::fm_exp(gamma/2)
  //   2 => not implemented
  //   3 => G = 2/(1+MathSpecial::fm_exp(-gamma))
  //   4 => G = sqrt(1+gamma)
  //  -5 => G = +-sqrt(abs(1+gamma))
  double gsmooth_switchpoint;

  if (ibar == 0 || ibar == 4) {
    gsmooth_switchpoint = -gsmooth_factor / (gsmooth_factor + 1);
    if (gamma < gsmooth_switchpoint) {
      //         e.g. gsmooth_factor is 99, {:
      //         gsmooth_switchpoint = -0.99
      //         G = 0.01*(-0.99/gamma)**99
      *G = 1 / (gsmooth_factor + 1) *
           pow((gsmooth_switchpoint / gamma), gsmooth_factor);
      *G = sqrt(*G);
      *dG = -gsmooth_factor * *G / (2.0 * gamma);
    } else {
      *G = sqrt(1.0 + gamma);
      *dG = 1.0 / (2.0 * *G);
    }
  } else if (ibar == 1) {
    *G = MathSpecial::fm_exp(gamma / 2.0);
    *dG = *G / 2.0;
  } else if (ibar == 3) {
    *G = 2.0 / (1.0 + MathSpecial::fm_exp(-gamma));
    *dG = *G * (2.0 - *G) / 2;
  } else if (ibar == -5) {
    if ((1.0 + gamma) >= 0) {
      *G = sqrt(1.0 + gamma);
      *dG = 1.0 / (2.0 * *G);
    } else {
      *G = -sqrt(-1.0 - gamma);
      *dG = -1.0 / (2.0 * *G);
    }
  }
}

// ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
