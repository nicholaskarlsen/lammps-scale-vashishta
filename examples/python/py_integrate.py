from __future__ import print_function
import lammps
import ctypes
import traceback
import numpy as np

class LAMMPSIntegrator(object):
    def __init__(self, ptr):
        self.lmp = lammps.lammps(ptr=ptr)

    def init(self):
        pass

    def initial_integrate(self, vflag):
        pass

    def final_integrate(self):
        pass

    def initial_integrate_respa(self, vflag, ilevel, iloop):
        pass

    def final_integrate_respa(self, ilevel, iloop):
        pass

    def reset_dt(self):
        pass


class NVE(LAMMPSIntegrator):
    """ Python implementation of fix/nve """
    def __init__(self, ptr):
        super(NVE, self).__init__(ptr)

    def init(self):
        dt = self.lmp.extract_global("dt", 1)
        ftm2v = self.lmp.extract_global("ftm2v", 1)
        self.ntypes = self.lmp.extract_global("ntypes", 0)
        self.dtv = dt
        self.dtf = 0.5 * dt * ftm2v

    def initial_integrate(self, vflag):
        nlocal = self.lmp.extract_global("nlocal", 0)
        mass = self.lmp.numpy.extract_atom_darray("mass", self.ntypes+1)
        atype = self.lmp.numpy.extract_atom_iarray("type", nlocal)
        x = self.lmp.numpy.extract_atom_darray("x", nlocal, dim=3)
        v = self.lmp.numpy.extract_atom_darray("v", nlocal, dim=3)
        f = self.lmp.numpy.extract_atom_darray("f", nlocal, dim=3)

        for i in range(x.shape[0]):
            dtfm = self.dtf / mass[int(atype[i])]
            v[i,:]+= dtfm * f[i,:]
            x[i,:] += self.dtv * v[i,:]

    def final_integrate(self):
        nlocal = self.lmp.extract_global("nlocal", 0)
        mass = self.lmp.numpy.extract_atom_darray("mass", self.ntypes+1)
        atype = self.lmp.numpy.extract_atom_iarray("type", nlocal)
        v = self.lmp.numpy.extract_atom_darray("v", nlocal, dim=3)
        f = self.lmp.numpy.extract_atom_darray("f", nlocal, dim=3)

        for i in range(v.shape[0]):
            dtfm = self.dtf / mass[int(atype[i])]
            v[i,:] += dtfm * f[i,:]


class NVE_Opt(LAMMPSIntegrator):
    """ Tuned Python implementation of fix/nve """
    def __init__(self, ptr):
        super(NVE_Opt, self).__init__(ptr)

    def init(self):
        dt = self.lmp.extract_global("dt", 1)
        ftm2v = self.lmp.extract_global("ftm2v", 1)
        self.ntypes = self.lmp.extract_global("ntypes", 0)
        self.dtv = dt
        self.dtf = 0.5 * dt * ftm2v
        self.mass = self.lmp.numpy.extract_atom_darray("mass", self.ntypes+1)

    def initial_integrate(self, vflag):        
        nlocal = self.lmp.extract_global("nlocal", 0)
        atype = self.lmp.numpy.extract_atom_iarray("type", nlocal)
        x = self.lmp.numpy.extract_atom_darray("x", nlocal, dim=3)
        v = self.lmp.numpy.extract_atom_darray("v", nlocal, dim=3)
        f = self.lmp.numpy.extract_atom_darray("f", nlocal, dim=3)
        dtf = self.dtf
        dtv = self.dtv
        mass = self.mass

        dtfm = dtf / np.take(mass, atype)

        for i in range(x.shape[0]):
            vi = v[i,:] 
            vi += dtfm[i] * f[i,:]
            x[i,:] += dtv * vi

    def final_integrate(self):
        nlocal = self.lmp.extract_global("nlocal", 0)
        mass = self.lmp.numpy.extract_atom_darray("mass", self.ntypes+1)
        atype = self.lmp.numpy.extract_atom_iarray("type", nlocal)
        v = self.lmp.numpy.extract_atom_darray("v", nlocal, dim=3)
        f = self.lmp.numpy.extract_atom_darray("f", nlocal, dim=3)
        dtf = self.dtf
        dtv = self.dtv
        mass = self.mass

        dtfm = dtf / np.take(mass, atype)

        for i in range(v.shape[0]):
            v[i,:] += dtfm[i] * f[i,:]
