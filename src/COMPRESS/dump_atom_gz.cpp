/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://lammps.sandia.gov/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "domain.h"
#include "dump_atom_gz.h"
#include "error.h"
#include "update.h"

#include <cstring>

using namespace LAMMPS_NS;

DumpAtomGZ::DumpAtomGZ(LAMMPS *lmp, int narg, char **arg) :
  DumpAtom(lmp, narg, arg)
{
  if (!compressed)
    error->all(FLERR,"Dump atom/gz only writes compressed files");
}

/* ---------------------------------------------------------------------- */

DumpAtomGZ::~DumpAtomGZ()
{
}

/* ----------------------------------------------------------------------
   generic opening of a dump file
   ASCII or binary or gzipped
   some derived classes override this function
------------------------------------------------------------------------- */

void DumpAtomGZ::openfile()
{
  // single file, already opened, so just return

  if (singlefile_opened) return;
  if (multifile == 0) singlefile_opened = 1;

  // if one file per timestep, replace '*' with current timestep

  char *filecurrent = filename;
  if (multiproc) filecurrent = multiname;

  if (multifile) {
    char *filestar = filecurrent;
    filecurrent = new char[strlen(filestar) + 16];
    char *ptr = strchr(filestar,'*');
    *ptr = '\0';
    if (padflag == 0)
      sprintf(filecurrent,"%s" BIGINT_FORMAT "%s",
              filestar,update->ntimestep,ptr+1);
    else {
      char bif[8],pad[16];
      strcpy(bif,BIGINT_FORMAT);
      sprintf(pad,"%%s%%0%d%s%%s",padflag,&bif[1]);
      sprintf(filecurrent,pad,filestar,update->ntimestep,ptr+1);
    }
    *ptr = '*';
    if (maxfiles > 0) {
      if (numfiles < maxfiles) {
        nameslist[numfiles] = utils::strdup(filecurrent);
        ++numfiles;
      } else {
        if (remove(nameslist[fileidx]) != 0) {
          error->warning(FLERR, fmt::format("Could not delete {}", nameslist[fileidx]));
        }
        delete[] nameslist[fileidx];
        nameslist[fileidx] = utils::strdup(filecurrent);
        fileidx = (fileidx + 1) % maxfiles;
      }
    }
  }

  // each proc with filewriter = 1 opens a file

  if (filewriter) {
    try {
      writer.open(filecurrent, append_flag);
    } catch (FileWriterException &e) {
      error->one(FLERR, e.what());
    }
  }

  // delete string with timestep replaced

  if (multifile) delete [] filecurrent;
}

/* ---------------------------------------------------------------------- */

void DumpAtomGZ::write_header(bigint ndump)
{
  std::string header;

  if ((multiproc) || (!multiproc && me == 0)) {
    if (unit_flag && !unit_count) {
      ++unit_count;
      header = fmt::format("ITEM: UNITS\n{}\n",update->unit_style);
    }

    if (time_flag) {
      header += fmt::format("ITEM: TIME\n{0:.16g}\n", compute_time());
    }

    header += fmt::format("ITEM: TIMESTEP\n{}\n", update->ntimestep);
    header += fmt::format("ITEM: NUMBER OF ATOMS\n{}\n", ndump);
    if (domain->triclinic == 0) {
      header += fmt::format("ITEM: BOX BOUNDS {}\n", boundstr);
      header += fmt::format("{0:-1.16e} {1:-1.16e}\n", boxxlo, boxxhi);
      header += fmt::format("{0:-1.16e} {1:-1.16e}\n", boxylo, boxyhi);
      header += fmt::format("{0:-1.16e} {1:-1.16e}\n", boxzlo, boxzhi);
    } else {
      header += fmt::format("ITEM: BOX BOUNDS xy xz yz {}\n", boundstr);
      header += fmt::format("{0:-1.16e} {1:-1.16e} {2:-1.16e}\n", boxxlo, boxxhi, boxxy);
      header += fmt::format("{0:-1.16e} {1:-1.16e} {2:-1.16e}\n", boxylo, boxyhi, boxxz);
      header += fmt::format("{0:-1.16e} {1:-1.16e} {2:-1.16e}\n", boxzlo, boxzhi, boxyz);
    }
    header += fmt::format("ITEM: ATOMS {}\n", columns);

    writer.write(header.c_str(), header.length());
  }
}

/* ---------------------------------------------------------------------- */

void DumpAtomGZ::write_data(int n, double *mybuf)
{
  writer.write(mybuf, n);
}

/* ---------------------------------------------------------------------- */

void DumpAtomGZ::write()
{
  DumpAtom::write();
  if (filewriter) {
    if (multifile) {
      writer.close();
    } else {
      if (flush_flag && writer.isopen()) {
        writer.flush();
      }
    }
  }
}

/* ---------------------------------------------------------------------- */

int DumpAtomGZ::modify_param(int narg, char **arg)
{
  int consumed = DumpAtom::modify_param(narg, arg);
  if (consumed == 0) {
    try {
      if (strcmp(arg[0],"compression_level") == 0) {
        if (narg < 2) error->all(FLERR,"Illegal dump_modify command");
        int compression_level = utils::inumeric(FLERR, arg[1], false, lmp);
        writer.setCompressionLevel(compression_level);
        return 2;
      }
    } catch (FileWriterException &e) {
      error->one(FLERR, fmt::format("Illegal dump_modify command: {}", e.what()));
    }
  }
  return consumed;
}
