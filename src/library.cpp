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

// C style library interface to LAMMPS.
// See the manual for detailed documentation.

#define LAMMPS_LIB_MPI 1
#include "library.h"
#include <mpi.h>

#include "atom.h"
#include "atom_vec.h"
#include "comm.h"
#include "compute.h"
#include "domain.h"
#include "dump.h"
#include "error.h"
#include "fix.h"
#include "fix_external.h"
#include "force.h"
#include "group.h"
#include "info.h"
#include "input.h"
#include "memory.h"
#include "modify.h"
#include "molecule.h"
#include "neigh_list.h"
#include "neighbor.h"
#include "region.h"
#include "output.h"
#if defined(LMP_PLUGIN)
#include "plugin.h"
#endif
#include "thermo.h"
#include "timer.h"
#include "universe.h"
#include "update.h"
#include "variable.h"

#include <cstring>
#include <vector>

#if defined(LAMMPS_EXCEPTIONS)
#include "exceptions.h"
#endif

using namespace LAMMPS_NS;

// for printing the non-null pointer argument warning only once

static int ptr_argument_flag = 1;
static void ptr_argument_warning()
{
  if (!ptr_argument_flag) return;
  fprintf(stderr,"Using a 'void **' argument to return the LAMMPS handle "
          "is deprecated.  Please use the return value instead.\n");
  ptr_argument_flag = 0;
}

// ----------------------------------------------------------------------
// utility macros
// ----------------------------------------------------------------------

/* ----------------------------------------------------------------------
   macros for optional code path which captures all exceptions
   and stores the last error message. These assume there is a variable lmp
   which is a pointer to the current LAMMPS instance.

   Usage:

   BEGIN_CAPTURE
   {
     // code paths which might throw exception
     ...
   }
   END_CAPTURE
------------------------------------------------------------------------- */

#ifdef LAMMPS_EXCEPTIONS
#define BEGIN_CAPTURE \
  Error *error = lmp->error; \
  try

#define END_CAPTURE \
  catch(LAMMPSAbortException &ae) { \
    int nprocs = 0; \
    MPI_Comm_size(ae.universe, &nprocs ); \
    \
    if (nprocs > 1) { \
      error->set_last_error(ae.message, ERROR_ABORT); \
    } else { \
      error->set_last_error(ae.message, ERROR_NORMAL); \
    } \
  } catch(LAMMPSException &e) { \
    error->set_last_error(e.message, ERROR_NORMAL); \
  }
#else
#define BEGIN_CAPTURE
#define END_CAPTURE
#endif

// ----------------------------------------------------------------------
// Library functions to create/destroy an instance of LAMMPS
// ----------------------------------------------------------------------

/** Create instance of the LAMMPS class and return pointer to it.
 *
\verbatim embed:rst

The :cpp:func:`lammps_open` function creates a new :cpp:class:`LAMMPS
<LAMMPS_NS::LAMMPS>` class instance while passing in a list of strings
as if they were :doc:`command-line arguments <Run_options>` for the
LAMMPS executable, and an MPI communicator for LAMMPS to run under.
Since the list of arguments is **exactly** as when called from the
command line, the first argument would be the name of the executable and
thus is otherwise ignored.  However ``argc`` may be set to 0 and then
``argv`` may be ``NULL``.  If MPI is not yet initialized, ``MPI_Init()``
will be called during creation of the LAMMPS class instance.

If for some reason the creation or initialization of the LAMMPS instance
fails a null pointer is returned.

.. versionchanged:: 18Sep2020

   This function now has the pointer to the created LAMMPS class
   instance as return value.  For backward compatibility it is still
   possible to provide the address of a pointer variable as final
   argument *ptr*\ .

.. deprecated:: 18Sep2020

   The *ptr* argument will be removed in a future release of LAMMPS.
   It should be set to ``NULL`` instead.

.. note::

   This function is **only** declared when the code using the LAMMPS
   ``library.h`` include file is compiled with ``-DLAMMPS_LIB_MPI``,
   or contains a ``#define LAMMPS_LIB_MPI 1`` statement before
   ``#include "library.h"``.  Otherwise you can only use the
   :cpp:func:`lammps_open_no_mpi` or :cpp:func:`lammps_open_fortran`
   functions.

*See also*
   :cpp:func:`lammps_open_no_mpi`, :cpp:func:`lammps_open_fortran`

\endverbatim
 *
 * \param  argc  number of command line arguments
 * \param  argv  list of command line argument strings
 * \param  comm  MPI communicator for this LAMMPS instance
 * \param  ptr   pointer to a void pointer variable which serves
 *               as a handle; may be ``NULL``
 * \return       pointer to new LAMMPS instance cast to ``void *`` */

void *lammps_open(int argc, char **argv, MPI_Comm comm, void **ptr)
{
  LAMMPS *lmp = nullptr;
  lammps_mpi_init();
  if (ptr) ptr_argument_warning();

#ifdef LAMMPS_EXCEPTIONS
  try
  {
    lmp = new LAMMPS(argc, argv, comm);
    if (ptr) *ptr = (void *) lmp;
  }
  catch(LAMMPSException &e) {
    fmt::print(stderr, "LAMMPS Exception: {}", e.message);
    if (ptr) *ptr = nullptr;
  }
#else
  lmp = new LAMMPS(argc, argv, comm);
  if (ptr) *ptr = (void *) lmp;
#endif
  return (void *) lmp;
}

/* ---------------------------------------------------------------------- */

/** Variant of ``lammps_open()`` that implicitly uses ``MPI_COMM_WORLD``.
 *
\verbatim embed:rst

This function is a version of :cpp:func:`lammps_open`, that is missing
the MPI communicator argument.  It will use ``MPI_COMM_WORLD`` instead.
The type and purpose of arguments and return value are otherwise the
same.

Outside of the convenience, this function is useful, when the LAMMPS
library was compiled in serial mode, but the calling code runs in
parallel and the ``MPI_Comm`` data type of the STUBS library would not
be compatible with that of the calling code.

If for some reason the creation or initialization of the LAMMPS instance
fails a null pointer is returned.

.. versionchanged:: 18Sep2020

   This function now has the pointer to the created LAMMPS class
   instance as return value.  For backward compatibility it is still
   possible to provide the address of a pointer variable as final
   argument *ptr*\ .

.. deprecated:: 18Sep2020

   The *ptr* argument will be removed in a future release of LAMMPS.
   It should be set to ``NULL`` instead.


*See also*
   :cpp:func:`lammps_open`, :cpp:func:`lammps_open_fortran`

\endverbatim
 *
 * \param  argc  number of command line arguments
 * \param  argv  list of command line argument strings
 * \param  ptr   pointer to a void pointer variable
 *               which serves as a handle; may be ``NULL``
 * \return       pointer to new LAMMPS instance cast to ``void *`` */

void *lammps_open_no_mpi(int argc, char **argv, void **ptr)
{
  return lammps_open(argc,argv,MPI_COMM_WORLD,ptr);
}

/* ---------------------------------------------------------------------- */

/** Variant of ``lammps_open()`` using a Fortran MPI communicator.
 *
\verbatim embed:rst

This function is a version of :cpp:func:`lammps_open`, that uses an
integer for the MPI communicator as the MPI Fortran interface does.  It
is used in the :f:func:`lammps` constructor of the LAMMPS Fortran
module.  Internally it converts the *f_comm* argument into a C-style MPI
communicator with ``MPI_Comm_f2c()`` and then calls
:cpp:func:`lammps_open`.

If for some reason the creation or initialization of the LAMMPS instance
fails a null pointer is returned.

.. versionadded:: 18Sep2020

*See also*
   :cpp:func:`lammps_open_fortran`, :cpp:func:`lammps_open_no_mpi`

\endverbatim
 *
 * \param  argc   number of command line arguments
 * \param  argv   list of command line argument strings
 * \param  f_comm Fortran style MPI communicator for this LAMMPS instance
 * \return        pointer to new LAMMPS instance cast to ``void *`` */

void *lammps_open_fortran(int argc, char **argv, int f_comm)
{
  lammps_mpi_init();
  MPI_Comm c_comm = MPI_Comm_f2c((MPI_Fint)f_comm);
  return lammps_open(argc, argv, c_comm, nullptr);
}

/* ---------------------------------------------------------------------- */

/** Delete a LAMMPS instance created by lammps_open() or its variants.
 *
\verbatim embed:rst

This function deletes the LAMMPS class instance pointed to by ``handle``
that was created by one of the :cpp:func:`lammps_open` variants.  It
does **not** call ``MPI_Finalize()`` to allow creating and deleting
multiple LAMMPS instances concurrently or sequentially.  See
:cpp:func:`lammps_mpi_finalize` for a function performing this operation.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance */

void lammps_close(void *handle)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  delete lmp;
}

/* ---------------------------------------------------------------------- */

/** Ensure the MPI environment is initialized.
 *
\verbatim embed:rst

The MPI standard requires that any MPI application must call
``MPI_Init()`` exactly once before performing any other MPI function
calls.  This function checks, whether MPI is already initialized and
calls ``MPI_Init()`` in case it is not.

.. versionadded:: 18Sep2020

\endverbatim */

void lammps_mpi_init()
{
  int flag;
  MPI_Initialized(&flag);

  if (!flag) {
    // provide a dummy argc and argv for MPI_Init().
    int argc = 1;
    char *args[] = { (char *)"liblammps" , nullptr  };
    char **argv = args;
    MPI_Init(&argc,&argv);
  }
}

/* ---------------------------------------------------------------------- */

/** Shut down the MPI infrastructure.
 *
\verbatim embed:rst

The MPI standard requires that any MPI application calls
``MPI_Finalize()`` before exiting.  Even if a calling program does not
do any MPI calls, MPI is still initialized internally to avoid errors
accessing any MPI functions.  This function should then be called right
before exiting the program to wait until all (parallel) tasks are
completed and then MPI is cleanly shut down.  After this function no
more MPI calls may be made.

.. versionadded:: 18Sep2020

\endverbatim */

void lammps_mpi_finalize()
{
  int flag;
  MPI_Initialized(&flag);
  if (flag) {
    MPI_Finalized(&flag);
    if (!flag) {
      MPI_Barrier(MPI_COMM_WORLD);
      MPI_Finalize();
    }
  }
}

// ----------------------------------------------------------------------
// Library functions to process commands
// ----------------------------------------------------------------------

/** Process LAMMPS input from a file.
 *
\verbatim embed:rst

This function processes commands in the file pointed to by *filename*
line by line and thus functions very similar to the :doc:`include
<include>` command. The function returns when the end of the file is
reached and the commands have completed.

The actual work is done by the functions
:cpp:func:`Input::file(const char *)<void LAMMPS_NS::Input::file(const char *)>`
and :cpp:func:`Input::file()<void LAMMPS_NS::Input::file()>`.

\endverbatim
 *
 * \param  handle    pointer to a previously created LAMMPS instance
 * \param  filename  name of a file with LAMMPS input */

void lammps_file(void *handle, const char *filename)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
    if (lmp->update->whichflag != 0)
      lmp->error->all(FLERR,"Library error: issuing LAMMPS commands "
                      "during a run is not allowed.");
    else
      lmp->input->file(filename);
  }
  END_CAPTURE
}

/* ---------------------------------------------------------------------- */

/** Process a single LAMMPS input command from a string.
 *
\verbatim embed:rst

This function tells LAMMPS to execute the single command in the string
*cmd*.  The entire string is considered as command and need not have a
(final) newline character.  Newline characters in the body of the
string, however, will be treated as part of the command and will **not**
start a second command.  The function :cpp:func:`lammps_commands_string`
processes a string with multiple command lines.

The function returns the name of the command on success or ``NULL`` when
passing a string without a command.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  cmd     string with a single LAMMPS command
 * \return         string with parsed command name or ``NULL`` */

char *lammps_command(void *handle, const char *cmd)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  char *result = nullptr;

  BEGIN_CAPTURE
  {
    if (lmp->update->whichflag != 0)
      lmp->error->all(FLERR,"Library error: issuing LAMMPS commands "
                      "during a run is not allowed.");
    else
      result = lmp->input->one(cmd);
  }
  END_CAPTURE

  return result;
}

/* ---------------------------------------------------------------------- */

/** Process multiple LAMMPS input commands from list of strings.
 *
\verbatim embed:rst

This function processes multiple commands from a list of strings by
first concatenating the individual strings in *cmds* into a single
string, inserting newline characters as needed.  The combined string
is passed to :cpp:func:`lammps_commands_string` for processing.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  ncmd    number of lines in *cmds*
 * \param  cmds    list of strings with LAMMPS commands */

void lammps_commands_list(void *handle, int ncmd, const char **cmds)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  std::string allcmds;

  for (int i = 0; i < ncmd; i++) {
    allcmds.append(cmds[i]);
    if (allcmds.back() != '\n') allcmds.append(1,'\n');
  }

  lammps_commands_string(handle,allcmds.c_str());
}

/* ---------------------------------------------------------------------- */

/** Process a block of LAMMPS input commands from a single string.
 *
\verbatim embed:rst

This function processes a multi-line string similar to a block of
commands from a file.  The string may have multiple lines (separated by
newline characters) and also single commands may be distributed over
multiple lines with continuation characters ('&').  Those lines are
combined by removing the '&' and the following newline character.  After
this processing the string is handed to LAMMPS for parsing and
executing.

.. note::

   Multi-line commands enabled by triple quotes will NOT work with
   this function.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  str     string with block of LAMMPS input commands */

void lammps_commands_string(void *handle, const char *str)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  // copy str and convert from CR-LF (DOS-style) to LF (Unix style) line
  int n = strlen(str);
  char *ptr, *copy = new char[n+1];

  for (ptr = copy; *str != '\0'; ++str) {
    if ((str[0] == '\r') && (str[1] == '\n')) continue;
    *ptr++ = *str;
  }
  *ptr = '\0';

  BEGIN_CAPTURE
  {
    if (lmp->update->whichflag != 0) {
      lmp->error->all(FLERR,"Library error: issuing LAMMPS command during run");
    }

    n = strlen(copy);
    ptr = copy;
    for (int i=0; i < n; ++i) {

      // handle continuation character as last character in line or string
      if ((copy[i] == '&') && (copy[i+1] == '\n'))
        copy[i+1] = copy[i] = ' ';
      else if ((copy[i] == '&') && (copy[i+1] == '\0'))
        copy[i] = ' ';

      if (copy[i] == '\n') {
        copy[i] = '\0';
        lmp->input->one(ptr);
        ptr = copy + i+1;
      } else if (copy[i+1] == '\0')
        lmp->input->one(ptr);
    }
  }
  END_CAPTURE

  delete [] copy;
}

// -----------------------------------------------------------------------
// Library functions to extract info from LAMMPS or set data in LAMMPS
// -----------------------------------------------------------------------

/** Return the total number of atoms in the system.
 *
\verbatim embed:rst

This number may be very large when running large simulations across
multiple processors.  Depending on compile time choices, LAMMPS may be
using either 32-bit or a 64-bit integer to store this number. For
portability this function returns thus a double precision
floating point number, which can represent up to a 53-bit signed
integer exactly (:math:`\approx 10^{16}`).

As an alternative, you can use :cpp:func:`lammps_extract_global`
and cast the resulting pointer to an integer pointer of the correct
size and dereference it.  The size of that integer (in bytes) can be
queried by calling :cpp:func:`lammps_extract_setting` to return
the size of a ``bigint`` integer.

.. versionchanged:: 18Sep2020

   The type of the return value was changed from ``int`` to ``double``
   to accommodate reporting atom counts for larger systems that would
   overflow a 32-bit int without having to depend on a 64-bit bit
   integer type definition.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \return         total number of atoms or 0 if value is too large */

double lammps_get_natoms(void *handle)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  double natoms = static_cast<double>(lmp->atom->natoms);
  if (natoms > 9.0e15) return 0; // TODO:XXX why not -1?
  return natoms;
}

/* ---------------------------------------------------------------------- */

/** Get current value of a thermo keyword.
 *
\verbatim embed:rst

This function returns the current value of a :doc:`thermo keyword
<thermo_style>`.  Unlike :cpp:func:`lammps_extract_global` it does not
give access to the storage of the desired data but returns its value as
a ``double``, so it can also return information that is computed on-the-fly.

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance
 * \param  keyword  string with the name of the thermo keyword
 * \return          value of the requested thermo property or 0.0 */

double lammps_get_thermo(void *handle, const char *keyword)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  double dval = 0.0;

  BEGIN_CAPTURE
  {
    lmp->output->thermo->evaluate_keyword(keyword,&dval);
  }
  END_CAPTURE

  return dval;
}

/* ---------------------------------------------------------------------- */

/** Extract simulation box parameters.
 *
\verbatim embed:rst

This function (re-)initializes the simulation box and boundary
information and then assign the designated data to the locations in the
pointers passed as arguments. Any argument (except the first) may be
a NULL pointer and then will not be assigned.

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance
 * \param  boxlo    pointer to 3 doubles where the lower box boundary is stored
 * \param  boxhi    pointer to 3 doubles where the upper box boundary is stored
 * \param  xy       pointer to a double where the xy tilt factor is stored
 * \param  yz       pointer to a double where the yz tilt factor is stored
 * \param  xz       pointer to a double where the xz tilt factor is stored
 * \param  pflags   pointer to 3 ints, set to 1 for periodic boundaries
                    and 0 for non-periodic
 * \param  boxflag  pointer to an int, which is set to 1 if the box will be
 *                  changed during a simulation by a fix and 0 if not. */

void lammps_extract_box(void *handle, double *boxlo, double *boxhi,
                        double *xy, double *yz, double *xz,
                        int *pflags, int *boxflag)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  Domain *domain = lmp->domain;

  BEGIN_CAPTURE
  {
    // do nothing if box does not yet exist
    if ((lmp->domain->box_exist == 0)
        && (lmp->comm->me == 0)) {
      lmp->error->warning(FLERR,"Calling lammps_extract_box without a box");
      return;
    }

    // domain->init() is needed to update domain->box_change
    domain->init();

    if (boxlo) {
      boxlo[0] = domain->boxlo[0];
      boxlo[1] = domain->boxlo[1];
      boxlo[2] = domain->boxlo[2];
    }
    if (boxhi) {
      boxhi[0] = domain->boxhi[0];
      boxhi[1] = domain->boxhi[1];
      boxhi[2] = domain->boxhi[2];
    }
    if (xy) *xy = domain->xy;
    if (yz) *yz = domain->yz;
    if (xz) *xz = domain->xz;

    if (pflags) {
      pflags[0] = domain->periodicity[0];
      pflags[1] = domain->periodicity[1];
      pflags[2] = domain->periodicity[2];
    }
    if (boxflag) *boxflag = domain->box_change;
  }
  END_CAPTURE
}

/* ---------------------------------------------------------------------- */

/** Reset simulation box parameters.
 *
\verbatim embed:rst

This function sets the simulation box dimensions (upper and lower bounds
and tilt factors) from the provided data and then re-initializes the box
information and all derived settings.

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance
 * \param  boxlo    pointer to 3 doubles containing the lower box boundary
 * \param  boxhi    pointer to 3 doubles containing the upper box boundary
 * \param  xy       xy tilt factor
 * \param  yz       yz tilt factor
 * \param  xz       xz tilt factor */

void lammps_reset_box(void *handle, double *boxlo, double *boxhi,
                      double xy, double yz, double xz)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  Domain *domain = lmp->domain;

  BEGIN_CAPTURE
  {
    // error if box does not exist
    if ((lmp->domain->box_exist == 0)
        && (lmp->comm->me == 0)) {
      lmp->error->warning(FLERR,"Calling lammps_reset_box without a box");
      return;
    }
    domain->boxlo[0] = boxlo[0];
    domain->boxlo[1] = boxlo[1];
    domain->boxlo[2] = boxlo[2];
    domain->boxhi[0] = boxhi[0];
    domain->boxhi[1] = boxhi[1];
    domain->boxhi[2] = boxhi[2];

    domain->xy = xy;
    domain->yz = yz;
    domain->xz = xz;

    domain->set_global_box();
    lmp->comm->set_proc_grid();
    domain->set_local_box();
  }
  END_CAPTURE
}

/* ---------------------------------------------------------------------- */

/** Get memory usage information
 *
\verbatim embed:rst

This function will retrieve memory usage information for the current
LAMMPS instance or process.  The *meminfo* buffer will be filled with
3 different numbers (if supported by the operating system).  The first
is the tally (in MBytes) of all large memory allocations made by LAMMPS.
This is a lower boundary of how much memory is requested and does not
account for memory allocated on the stack or allocations via ``new``.
The second number is the current memory allocation of the current process
as returned by a memory allocation reporting in the system library.  The
third number is the maximum amount of RAM (not swap) used by the process
so far. If any of the two latter parameters is not supported by the operating
system it will be set to zero.

.. versionadded:: 18Sep2020

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance
 * \param  meminfo  buffer with space for at least 3 double to store
 * data in. */

void lammps_memory_usage(void *handle, double *meminfo)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  Info info(lmp);
  info.get_memory_info(meminfo);
}

/* ---------------------------------------------------------------------- */

/** Return current LAMMPS world communicator as integer
 *
\verbatim embed:rst

This will take the LAMMPS "world" communicator and convert it to an
integer using ``MPI_Comm_c2f()``, so it is equivalent to the
corresponding MPI communicator in Fortran. This way it can be safely
passed around between different programming languages.  To convert it
to the C language representation use ``MPI_Comm_f2c()``.

If LAMMPS was compiled with MPI_STUBS, this function returns -1.

.. versionadded:: 18Sep2020

*See also*
   :cpp:func:`lammps_open_fortran`

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \return         Fortran representation of the LAMMPS world communicator */

int lammps_get_mpi_comm(void *handle)
{
#ifdef MPI_STUBS
  return -1;
#else
  LAMMPS *lmp = (LAMMPS *) handle;
  MPI_Fint f_comm = MPI_Comm_c2f(lmp->world);
  return f_comm;
#endif
}

/* ---------------------------------------------------------------------- */

/** Query LAMMPS about global settings.
 *
\verbatim embed:rst

This function will retrieve or compute global properties. In contrast to
:cpp:func:`lammps_get_thermo` this function returns an ``int``.  The
following tables list the currently supported keyword.  If a keyword is
not recognized, the function returns -1.

* :ref:`Integer sizes <extract_integer_sizes>`
* :ref:`System status <extract_system_status>`
* :ref:`System sizes <extract_system_sizes>`
* :ref:`Atom style flags <extract_atom_flags>`

.. _extract_integer_sizes:

**Integer sizes**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Keyword
     - Description / Return value
   * - bigint
     - size of the ``bigint`` integer type, 4 or 8 bytes.
       Set at :ref:`compile time <size>`.
   * - tagint
     - size of the ``tagint`` integer type, 4 or 8 bytes.
       Set at :ref:`compile time <size>`.
   * - imageint
     - size of the ``imageint`` integer type, 4 or 8 bytes.
       Set at :ref:`compile time <size>`.

.. _extract_system_status:

**System status**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Keyword
     - Description / Return value
   * - dimension
     - Number of dimensions: 2 or 3. See :doc:`dimension`.
   * - box_exist
     - 1 if the simulation box is defined, 0 if not.
       See :doc:`create_box`.
   * - nthreads
     - Number of requested OpenMP threads for LAMMPS' execution
   * - newton_bond
     - 1 if Newton's 3rd law is applied to bonded interactions, 0 if not.
   * - newton_pair
     - 1 if Newton's 3rd law is applied to non-bonded interactions, 0 if not.
   * - triclinic
     - 1 if the the simulation box is triclinic, 0 if orthogonal.
       See :doc:`change_box`.
   * - universe_rank
     - MPI rank on LAMMPS' universe communicator (0 <= universe_rank < universe_size)
   * - universe_size
     - Number of ranks on LAMMPS' universe communicator (world_size <= universe_size)
   * - world_rank
     - MPI rank on LAMMPS' world communicator (0 <= world_rank < world_size)
   * - world_size
     - Number of ranks on LAMMPS' world communicator

.. _extract_system_sizes:

**System sizes**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Keyword
     - Description / Return value
   * - nlocal
     - number of "owned" atoms of the current MPI rank.
   * - nghost
     - number of "ghost" atoms of the current MPI rank.
   * - nall
     - number of all "owned" and "ghost" atoms of the current MPI rank.
   * - nmax
     - maximum of nlocal+nghost across all MPI ranks (for per-atom data array size).
   * - ntypes
     - number of atom types
   * - nbondtypes
     - number of bond types
   * - nangletypes
     - number of angle types
   * - ndihedraltypes
     - number of dihedral types
   * - nimpropertypes
     - number of improper types

.. _extract_atom_flags:

**Atom style flags**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Keyword
     - Description / Return value
   * - molecule_flag
     - 1 if the atom style includes molecular topology data. See :doc:`atom_style`.
   * - q_flag
     - 1 if the atom style includes point charges. See :doc:`atom_style`.
   * - mu_flag
     - 1 if the atom style includes point dipoles. See :doc:`atom_style`.
   * - rmass_flag
     - 1 if the atom style includes per-atom masses, 0 if there are per-type masses. See :doc:`atom_style`.
   * - radius_flag
     - 1 if the atom style includes a per-atom radius. See :doc:`atom_style`.
   * - sphere_flag
     - 1 if the atom style describes extended particles that can rotate. See :doc:`atom_style`.
   * - ellipsoid_flag
     - 1 if the atom style describes extended particles that may be ellipsoidal. See :doc:`atom_style`.
   * - omega_flag
     - 1 if the atom style can store per-atom rotational velocities. See :doc:`atom_style`.
   * - torque_flag
     - 1 if the atom style can store per-atom torques. See :doc:`atom_style`.
   * - angmom_flag
     - 1 if the atom style can store per-atom angular momentum. See :doc:`atom_style`.

*See also*
   :cpp:func:`lammps_extract_global`

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance
 * \param  keyword  string with the name of the thermo keyword
 * \return          value of the queried setting or -1 if unknown */

int lammps_extract_setting(void *handle, const char *keyword)
{
  LAMMPS *lmp = (LAMMPS *) handle;

// This can be customized by adding keywords and documenting them in the section above.
  if (strcmp(keyword,"bigint") == 0) return sizeof(bigint);
  if (strcmp(keyword,"tagint") == 0) return sizeof(tagint);
  if (strcmp(keyword,"imageint") == 0) return sizeof(imageint);

  if (strcmp(keyword,"dimension") == 0) return lmp->domain->dimension;
  if (strcmp(keyword,"box_exist") == 0) return lmp->domain->box_exist;
  if (strcmp(keyword,"newton_bond") == 0) return lmp->force->newton_bond;
  if (strcmp(keyword,"newton_pair") == 0) return lmp->force->newton_pair;
  if (strcmp(keyword,"triclinic") == 0) return lmp->domain->triclinic;

  if (strcmp(keyword,"universe_rank") == 0) return lmp->universe->me;
  if (strcmp(keyword,"universe_size") == 0) return lmp->universe->nprocs;
  if (strcmp(keyword,"world_rank") == 0) return lmp->comm->me;
  if (strcmp(keyword,"world_size") == 0) return lmp->comm->nprocs;
  if (strcmp(keyword,"nthreads") == 0) return lmp->comm->nthreads;

  if (strcmp(keyword,"nlocal") == 0) return lmp->atom->nlocal;
  if (strcmp(keyword,"nghost") == 0) return lmp->atom->nghost;
  if (strcmp(keyword,"nall") == 0) return lmp->atom->nlocal+lmp->atom->nghost;
  if (strcmp(keyword,"nmax") == 0) return lmp->atom->nmax;
  if (strcmp(keyword,"ntypes") == 0) return lmp->atom->ntypes;
  if (strcmp(keyword,"nbondtypes") == 0) return lmp->atom->nbondtypes;
  if (strcmp(keyword,"nangletypes") == 0) return lmp->atom->nangletypes;
  if (strcmp(keyword,"ndihedraltypes") == 0) return lmp->atom->ndihedraltypes;
  if (strcmp(keyword,"nimpropertypes") == 0) return lmp->atom->nimpropertypes;

  if (strcmp(keyword,"molecule_flag") == 0) return lmp->atom->molecule_flag;
  if (strcmp(keyword,"q_flag") == 0) return lmp->atom->q_flag;
  if (strcmp(keyword,"mu_flag") == 0) return lmp->atom->mu_flag;
  if (strcmp(keyword,"rmass_flag") == 0) return lmp->atom->rmass_flag;
  if (strcmp(keyword,"radius_flag") == 0) return lmp->atom->radius_flag;
  if (strcmp(keyword,"sphere_flag") == 0) return lmp->atom->sphere_flag;
  if (strcmp(keyword,"ellipsoid_flag") == 0) return lmp->atom->ellipsoid_flag;
  if (strcmp(keyword,"omega_flag") == 0) return lmp->atom->omega_flag;
  if (strcmp(keyword,"torque_flag") == 0) return lmp->atom->torque_flag;
  if (strcmp(keyword,"angmom_flag") == 0) return lmp->atom->angmom_flag;
  if (strcmp(keyword,"peri_flag") == 0) return lmp->atom->peri_flag;

  return -1;
}

/* ---------------------------------------------------------------------- */

/** Get data type of internal global LAMMPS variables or arrays.
 *
\verbatim embed:rst

This function returns an integer that encodes the data type of the global
property with the specified name. See :cpp:enum:`_LMP_DATATYPE_CONST` for valid
values. Callers of :cpp:func:`lammps_extract_global` can use this information
to then decide how to cast the (void*) pointer and access the data.

.. versionadded:: 18Sep2020

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance (unused)
 * \param  name     string with the name of the extracted property
 * \return          integer constant encoding the data type of the property
 *                  or -1 if not found. */

int lammps_extract_global_datatype(void * /*handle*/, const char *name)
{
  if (strcmp(name,"dt") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"ntimestep") == 0) return LAMMPS_BIGINT;
  if (strcmp(name,"atime") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"atimestep") == 0) return LAMMPS_BIGINT;

  if (strcmp(name,"boxlo") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"boxhi") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"sublo") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"subhi") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"sublo_lambda") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"subhi_lambda") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"boxxlo") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"boxxhi") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"boxylo") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"boxyhi") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"boxzlo") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"boxzhi") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"periodicity") == 0) return LAMMPS_INT;
  if (strcmp(name,"triclinic") == 0) return LAMMPS_INT;
  if (strcmp(name,"xy") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"xz") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"yz") == 0) return LAMMPS_DOUBLE;

  if (strcmp(name,"natoms") == 0) return LAMMPS_BIGINT;
  if (strcmp(name,"nbonds") == 0) return LAMMPS_BIGINT;
  if (strcmp(name,"nangles") == 0) return LAMMPS_BIGINT;
  if (strcmp(name,"ndihedrals") == 0) return LAMMPS_BIGINT;
  if (strcmp(name,"nimpropers") == 0) return LAMMPS_BIGINT;
  if (strcmp(name,"nlocal") == 0) return LAMMPS_INT;
  if (strcmp(name,"nghost") == 0) return LAMMPS_INT;
  if (strcmp(name,"nmax") == 0) return LAMMPS_INT;
  if (strcmp(name,"ntypes") == 0) return LAMMPS_INT;

  if (strcmp(name,"q_flag") == 0) return LAMMPS_INT;

  if (strcmp(name,"units") == 0) return LAMMPS_STRING;
  if (strcmp(name,"boltz") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"hplanck") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"mvv2e") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"ftm2v") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"mv2d") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"nktv2p") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"qqr2e") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"qe2f") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"vxmu2f") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"xxt2kmu") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"dielectric") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"qqrd2e") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"e_mass") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"hhmrr2e") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"mvh2r") == 0) return LAMMPS_DOUBLE;

  if (strcmp(name,"angstrom") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"femtosecond") == 0) return LAMMPS_DOUBLE;
  if (strcmp(name,"qelectron") == 0) return LAMMPS_DOUBLE;

  return -1;
}

/* ---------------------------------------------------------------------- */

/** Get pointer to internal global LAMMPS variables or arrays.
 *
\verbatim embed:rst

This function returns a pointer to the location of some global property
stored in one of the constituent classes of a LAMMPS instance.  The
returned pointer is cast to ``void *`` and needs to be cast to a pointer
of the type that the entity represents. The pointers returned by this
function are generally persistent; therefore it is not necessary to call
the function again, unless a :doc:`clear` command is issued which wipes
out and recreates the contents of the :cpp:class:`LAMMPS
<LAMMPS_NS::LAMMPS>` class.

Please also see :cpp:func:`lammps_extract_setting`,
:cpp:func:`lammps_get_thermo`, and :cpp:func:`lammps_extract_box`.

.. warning::

   Modifying the data in the location pointed to by the returned pointer
   may lead to inconsistent internal data and thus may cause failures or
   crashes or bogus simulations.  In general it is thus usually better
   to use a LAMMPS input command that sets or changes these parameters.
   Those will takes care of all side effects and necessary updates of
   settings derived from such settings.  Where possible a reference to
   such a command or a relevant section of the manual is given below.

The following tables list the supported names, their data types, length
of the data area, and a short description.  The data type can also be
queried through calling :cpp:func:`lammps_extract_global_datatype`.
The ``bigint`` type may be defined to be either an ``int`` or an
``int64_t``.  This is set at :ref:`compile time <size>` of the LAMMPS
library and can be queried through calling
:cpp:func:`lammps_extract_setting`.
The function :cpp:func:`lammps_extract_global_datatype` will directly
report the "native" data type.  The following tables are provided:

* :ref:`Timestep settings <extract_timestep_settings>`
* :ref:`Simulation box settings <extract_box_settings>`
* :ref:`System property settings <extract_system_settings>`
* :ref:`Unit settings <extract_unit_settings>`

.. _extract_timestep_settings:

**Timestep settings**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Name
     - Type
     - Length
     - Description
   * - dt
     - double
     - 1
     - length of the time step. See :doc:`timestep`.
   * - ntimestep
     - bigint
     - 1
     - current time step number. See :doc:`reset_timestep`.
   * - atime
     - double
     - 1
     - accumulated simulation time in time units.
   * - atimestep
     - bigint
     - 1
     - the number of the timestep when "atime" was last updated.

.. _extract_box_settings:

**Simulation box settings**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Name
     - Type
     - Length
     - Description
   * - boxlo
     - double
     - 3
     - lower box boundaries. See :doc:`create_box`.
   * - boxhi
     - double
     - 3
     - upper box boundaries. See :doc:`create_box`.
   * - boxxlo
     - double
     - 1
     - lower box boundary in x-direction. See :doc:`create_box`.
   * - boxxhi
     - double
     - 1
     - upper box boundary in x-direction. See :doc:`create_box`.
   * - boxylo
     - double
     - 1
     - lower box boundary in y-direction. See :doc:`create_box`.
   * - boxyhi
     - double
     - 1
     - upper box boundary in y-direction. See :doc:`create_box`.
   * - boxzlo
     - double
     - 1
     - lower box boundary in z-direction. See :doc:`create_box`.
   * - boxzhi
     - double
     - 1
     - upper box boundary in z-direction. See :doc:`create_box`.
   * - sublo
     - double
     - 3
     - subbox lower boundaries
   * - subhi
     - double
     - 3
     - subbox upper boundaries
   * - sublo_lambda
     - double
     - 3
     - subbox lower boundaries in fractional coordinates (for triclinic cells)
   * - subhi_lambda
     - double
     - 3
     - subbox upper boundaries in fractional coordinates (for triclinic cells)
   * - periodicity
     - int
     - 3
     - 0 if non-periodic, 1 if periodic for x, y, and z;
       See :doc:`boundary`.
   * - triclinic
     - int
     - 1
     - 1 if the the simulation box is triclinic, 0 if orthogonal;
       See :doc:`change_box`.
   * - xy
     - double
     - 1
     - triclinic tilt factor. See :doc:`Howto_triclinic`.
   * - yz
     - double
     - 1
     - triclinic tilt factor. See :doc:`Howto_triclinic`.
   * - xz
     - double
     - 1
     - triclinic tilt factor. See :doc:`Howto_triclinic`.

.. _extract_system_settings:

**System property settings**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Name
     - Type
     - Length
     - Description
   * - ntypes
     - int
     - 1
     - number of atom types
   * - nbonds
     - bigint
     - 1
     - total number of bonds in the simulation.
   * - nangles
     - bigint
     - 1
     - total number of angles in the simulation.
   * - ndihedrals
     - bigint
     - 1
     - total number of dihedrals in the simulation.
   * - nimpropers
     - bigint
     - 1
     - total number of impropers in the simulation.
   * - natoms
     - bigint
     - 1
     - total number of atoms in the simulation.
   * - nlocal
     - int
     - 1
     - number of "owned" atoms of the current MPI rank.
   * - nghost
     - int
     - 1
     - number of "ghost" atoms of the current MPI rank.
   * - nmax
     - int
     - 1
     - maximum of nlocal+nghost across all MPI ranks (for per-atom data array size).
   * - q_flag
     - int
     - 1
     - **deprecated**. Use :cpp:func:`lammps_extract_setting` instead.

.. _extract_unit_settings:

**Unit settings**

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Name
     - Type
     - Length
     - Description
   * - units
     - char \*
     - 1
     - string with the current unit style. See :doc:`units`.
   * - boltz
     - double
     - 1
     - value of the "boltz" constant. See :doc:`units`.
   * - hplanck
     - double
     - 1
     - value of the "hplanck" constant. See :doc:`units`.
   * - mvv2e
     - double
     - 1
     - factor to convert :math:`\frac{1}{2}mv^2` for a particle to
       the current energy unit; See :doc:`units`.
   * - ftm2v
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - mv2d
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - nktv2p
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - qqr2e
     - double
     - 1
     - factor to convert :math:`\frac{q_i q_j}{r}` to energy units;
       See :doc:`units`.
   * - qe2f
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - vxmu2f
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - xxt2kmu
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - dielectric
     - double
     - 1
     - value of the dielectric constant. See :doc:`dielectric`.
   * - qqrd2e
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - e_mass
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - hhmrr2e
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - mvh2r
     - double
     - 1
     - (description missing) See :doc:`units`.
   * - angstrom
     - double
     - 1
     - constant to convert current length unit to angstroms;
       1.0 for reduced (aka "lj") units. See :doc:`units`.
   * - femtosecond
     - double
     - 1
     - constant to convert current time unit to femtoseconds;
       1.0 for reduced (aka "lj") units
   * - qelectron
     - double
     - 1
     - (description missing) See :doc:`units`.

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance
 * \param  name     string with the name of the extracted property
 * \return          pointer (cast to ``void *``) to the location of the
                    requested property. NULL if name is not known. */

void *lammps_extract_global(void *handle, const char *name)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  if (strcmp(name,"units") == 0) return (void *) lmp->update->unit_style;
  if (strcmp(name,"dt") == 0) return (void *) &lmp->update->dt;
  if (strcmp(name,"ntimestep") == 0) return (void *) &lmp->update->ntimestep;
  // update->atime can be referenced as a pointer
  // thermo "timer" data cannot be, since it is computed on request
  // lammps_get_thermo() can access all thermo keywords by value
  if (strcmp(name,"atime") == 0) return (void *) &lmp->update->atime;
  if (strcmp(name,"atimestep") == 0) return (void *) &lmp->update->atimestep;

  if (strcmp(name,"boxlo") == 0) return (void *) lmp->domain->boxlo;
  if (strcmp(name,"boxhi") == 0) return (void *) lmp->domain->boxhi;
  if (strcmp(name,"sublo") == 0) return (void *) lmp->domain->sublo;
  if (strcmp(name,"subhi") == 0) return (void *) lmp->domain->subhi;
  if (strcmp(name,"sublo_lambda") == 0) return (void *) lmp->domain->sublo_lamda;
  if (strcmp(name,"subhi_lambda") == 0) return (void *) lmp->domain->subhi_lamda;
  if (strcmp(name,"boxxlo") == 0) return (void *) &lmp->domain->boxlo[0];
  if (strcmp(name,"boxxhi") == 0) return (void *) &lmp->domain->boxhi[0];
  if (strcmp(name,"boxylo") == 0) return (void *) &lmp->domain->boxlo[1];
  if (strcmp(name,"boxyhi") == 0) return (void *) &lmp->domain->boxhi[1];
  if (strcmp(name,"boxzlo") == 0) return (void *) &lmp->domain->boxlo[2];
  if (strcmp(name,"boxzhi") == 0) return (void *) &lmp->domain->boxhi[2];
  if (strcmp(name,"periodicity") == 0) return (void *) lmp->domain->periodicity;
  if (strcmp(name,"triclinic") == 0) return (void *) &lmp->domain->triclinic;
  if (strcmp(name,"xy") == 0) return (void *) &lmp->domain->xy;
  if (strcmp(name,"xz") == 0) return (void *) &lmp->domain->xz;
  if (strcmp(name,"yz") == 0) return (void *) &lmp->domain->yz;

  if (strcmp(name,"natoms") == 0) return (void *) &lmp->atom->natoms;
  if (strcmp(name,"ntypes") == 0) return (void *) &lmp->atom->ntypes;
  if (strcmp(name,"nbonds") == 0) return (void *) &lmp->atom->nbonds;
  if (strcmp(name,"nangles") == 0) return (void *) &lmp->atom->nangles;
  if (strcmp(name,"ndihedrals") == 0) return (void *) &lmp->atom->ndihedrals;
  if (strcmp(name,"nimpropers") == 0) return (void *) &lmp->atom->nimpropers;
  if (strcmp(name,"nlocal") == 0) return (void *) &lmp->atom->nlocal;
  if (strcmp(name,"nghost") == 0) return (void *) &lmp->atom->nghost;
  if (strcmp(name,"nmax") == 0) return (void *) &lmp->atom->nmax;

  if (strcmp(name,"q_flag") == 0) return (void *) &lmp->atom->q_flag;

  // global constants defined by units

  if (strcmp(name,"boltz") == 0) return (void *) &lmp->force->boltz;
  if (strcmp(name,"hplanck") == 0) return (void *) &lmp->force->hplanck;
  if (strcmp(name,"mvv2e") == 0) return (void *) &lmp->force->mvv2e;
  if (strcmp(name,"ftm2v") == 0) return (void *) &lmp->force->ftm2v;
  if (strcmp(name,"mv2d") == 0) return (void *) &lmp->force->mv2d;
  if (strcmp(name,"nktv2p") == 0) return (void *) &lmp->force->nktv2p;
  if (strcmp(name,"qqr2e") == 0) return (void *) &lmp->force->qqr2e;
  if (strcmp(name,"qe2f") == 0) return (void *) &lmp->force->qe2f;
  if (strcmp(name,"vxmu2f") == 0) return (void *) &lmp->force->vxmu2f;
  if (strcmp(name,"xxt2kmu") == 0) return (void *) &lmp->force->xxt2kmu;
  if (strcmp(name,"dielectric") == 0) return (void *) &lmp->force->dielectric;
  if (strcmp(name,"qqrd2e") == 0) return (void *) &lmp->force->qqrd2e;
  if (strcmp(name,"e_mass") == 0) return (void *) &lmp->force->e_mass;
  if (strcmp(name,"hhmrr2e") == 0) return (void *) &lmp->force->hhmrr2e;
  if (strcmp(name,"mvh2r") == 0) return (void *) &lmp->force->mvh2r;

  if (strcmp(name,"angstrom") == 0) return (void *) &lmp->force->angstrom;
  if (strcmp(name,"femtosecond") == 0) return (void *) &lmp->force->femtosecond;
  if (strcmp(name,"qelectron") == 0) return (void *) &lmp->force->qelectron;

  return nullptr;
}

/* ---------------------------------------------------------------------- */

/** Get data type of a LAMMPS per-atom property
 *
\verbatim embed:rst

This function returns an integer that encodes the data type of the per-atom
property with the specified name. See :cpp:enum:`_LMP_DATATYPE_CONST` for valid
values. Callers of :cpp:func:`lammps_extract_atom` can use this information
to then decide how to cast the (void*) pointer and access the data.

.. versionadded:: 18Sep2020

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  name    string with the name of the extracted property
 * \return         integer constant encoding the data type of the property
 *                 or -1 if not found.
 * */

int lammps_extract_atom_datatype(void *handle, const char *name)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  return lmp->atom->extract_datatype(name);
}

/* ---------------------------------------------------------------------- */

/** Get pointer to a LAMMPS per-atom property.
 *
\verbatim embed:rst

This function returns a pointer to the location of per-atom properties
(and per-atom-type properties in the case of the 'mass' keyword).
Per-atom data is distributed across sub-domains and thus MPI ranks.  The
returned pointer is cast to ``void *`` and needs to be cast to a pointer
of data type that the entity represents.

A table with supported keywords is included in the documentation
of the :cpp:func:`Atom::extract() <LAMMPS_NS::Atom::extract>` function.

.. warning::

   The pointers returned by this function are generally not persistent
   since per-atom data may be re-distributed, re-allocated, and
   re-ordered at every re-neighboring operation.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  name    string with the name of the extracted property
 * \return         pointer (cast to ``void *``) to the location of the
 *                 requested data or ``NULL`` if not found. */

void *lammps_extract_atom(void *handle, const char *name)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  return lmp->atom->extract(name);
}

// ----------------------------------------------------------------------
// Library functions to access data from computes, fixes, variables in LAMMPS
// ----------------------------------------------------------------------

/** Get pointer to data from a LAMMPS compute.
 *
\verbatim embed:rst

This function returns a pointer to the location of data provided by a
:doc:`compute` instance identified by the compute-ID.  Computes may
provide global, per-atom, or local data, and those may be a scalar, a
vector, or an array or they may provide the information about the
dimensions of the respective data.  Since computes may provide multiple
kinds of data, it is required to set style and type flags representing
what specific data is desired.  This also determines to what kind of
pointer the returned pointer needs to be cast to access the data
correctly.  The function returns ``NULL`` if the compute ID is not found
or the requested data is not available or current. The following table
lists the available options.

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Style (see :cpp:enum:`_LMP_STYLE_CONST`)
     - Type (see :cpp:enum:`_LMP_TYPE_CONST`)
     - Returned type
     - Returned data
   * - LMP_STYLE_GLOBAL
     - LMP_TYPE_SCALAR
     - ``double *``
     - Global scalar
   * - LMP_STYLE_GLOBAL
     - LMP_TYPE_VECTOR
     - ``double *``
     - Global vector
   * - LMP_STYLE_GLOBAL
     - LMP_TYPE_ARRAY
     - ``double **``
     - Global array
   * - LMP_STYLE_GLOBAL
     - LMP_SIZE_VECTOR
     - ``int *``
     - Length of global vector
   * - LMP_STYLE_GLOBAL
     - LMP_SIZE_ROWS
     - ``int *``
     - Rows of global array
   * - LMP_STYLE_GLOBAL
     - LMP_SIZE_COLS
     - ``int *``
     - Columns of global array
   * - LMP_STYLE_ATOM
     - LMP_TYPE_VECTOR
     - ``double *``
     - Per-atom value
   * - LMP_STYLE_ATOM
     - LMP_TYPE_ARRAY
     - ``double **``
     - Per-atom vector
   * - LMP_STYLE_ATOM
     - LMP_SIZE_COLS
     - ``int *``
     - Columns in per-atom array, 0 if vector
   * - LMP_STYLE_LOCAL
     - LMP_TYPE_VECTOR
     - ``double *``
     - Local data vector
   * - LMP_STYLE_LOCAL
     - LMP_TYPE_ARRAY
     - ``double **``
     - Local data array
   * - LMP_STYLE_LOCAL
     - LMP_SIZE_ROWS
     - ``int *``
     - Number of local data rows
   * - LMP_STYLE_LOCAL
     - LMP_SIZE_COLS
     - ``int *``
     - Number of local data columns

.. warning::

   The pointers returned by this function are generally not persistent
   since the computed data may be re-distributed, re-allocated, and
   re-ordered at every invocation. It is advisable to re-invoke this
   function before the data is accessed, or make a copy if the data shall
   be used after other LAMMPS commands have been issued.

.. note::

   If the compute's data is not computed for the current step, the
   compute will be invoked.  LAMMPS cannot easily check at that time, if
   it is valid to invoke a compute, so it may fail with an error.  The
   caller has to check to avoid such an error.


\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  id      string with ID of the compute
 * \param  style   constant indicating the style of data requested
                   (global, per-atom, or local)
 * \param  type    constant indicating type of data (scalar, vector,
                   or array) or size of rows or columns
 * \return         pointer (cast to ``void *``) to the location of the
 *                 requested data or ``NULL`` if not found. */

void *lammps_extract_compute(void *handle, char *id, int style, int type)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
    int icompute = lmp->modify->find_compute(id);
    if (icompute < 0) return nullptr;
    Compute *compute = lmp->modify->compute[icompute];

    if (style == LMP_STYLE_GLOBAL) {
      if (type == LMP_TYPE_SCALAR) {
        if (!compute->scalar_flag) return nullptr;
        if (compute->invoked_scalar != lmp->update->ntimestep)
          compute->compute_scalar();
        return (void *) &compute->scalar;
      }
      if ((type == LMP_TYPE_VECTOR) || (type == LMP_SIZE_VECTOR)) {
        if (!compute->vector_flag) return nullptr;
        if (compute->invoked_vector != lmp->update->ntimestep)
          compute->compute_vector();
        if (type == LMP_TYPE_VECTOR)
          return (void *) compute->vector;
        else
          return (void *) &compute->size_vector;
      }
      if ((type == LMP_TYPE_ARRAY) || (type == LMP_SIZE_ROWS) || (type == LMP_SIZE_COLS)) {
        if (!compute->array_flag) return nullptr;
        if (compute->invoked_array != lmp->update->ntimestep)
          compute->compute_array();
        if (type == LMP_TYPE_ARRAY)
          return (void *) compute->array;
        else if (type == LMP_SIZE_ROWS)
          return (void *) &compute->size_array_rows;
        else
          return (void *) &compute->size_array_cols;
      }
    }

    if (style == LMP_STYLE_ATOM) {
      if (!compute->peratom_flag) return nullptr;
      if (compute->invoked_peratom != lmp->update->ntimestep)
        compute->compute_peratom();
      if (type == LMP_TYPE_VECTOR) return (void *) compute->vector_atom;
      if (type == LMP_TYPE_ARRAY) return (void *) compute->array_atom;
      if (type == LMP_SIZE_COLS) return (void *) &compute->size_peratom_cols;
    }

    if (style == LMP_STYLE_LOCAL) {
      if (!compute->local_flag) return nullptr;
      if (compute->invoked_local != lmp->update->ntimestep)
        compute->compute_local();
      if (type == LMP_TYPE_SCALAR) return (void *) &compute->size_local_rows;  /* for backward compatibility */
      if (type == LMP_TYPE_VECTOR) return (void *) compute->vector_local;
      if (type == LMP_TYPE_ARRAY) return (void *) compute->array_local;
      if (type == LMP_SIZE_ROWS) return (void *) &compute->size_local_rows;
      if (type == LMP_SIZE_COLS) return (void *) &compute->size_local_cols;
    }
  }
  END_CAPTURE

  return nullptr;
}

/* ---------------------------------------------------------------------- */

/** Get pointer to data from a LAMMPS fix.
 *
\verbatim embed:rst

This function returns a pointer to data provided by a :doc:`fix`
instance identified by its fix-ID.  Fixes may provide global, per-atom,
or local data, and those may be a scalar, a vector, or an array, or they
may provide the information about the dimensions of the respective data.
Since individual fixes may provide multiple kinds of data, it is
required to set style and type flags representing what specific data is
desired.  This also determines to what kind of pointer the returned
pointer needs to be cast to access the data correctly.  The function
returns ``NULL`` if the fix ID is not found or the requested data is not
available.

.. note::

   When requesting global data, the fix data can only be accessed one
   item at a time without access to the pointer itself.  Thus this
   function will allocate storage for a single double value, copy the
   returned value to it, and returns a pointer to the location of the
   copy.  Therefore the allocated storage needs to be freed after its
   use to avoid a memory leak. Example:

   .. code-block:: c

      double *dptr = (double *) lammps_extract_fix(handle,name,0,1,0,0);
      double value = *dptr;
      lammps_free((void *)dptr);

The following table lists the available options.

.. list-table::
   :header-rows: 1
   :widths: auto

   * - Style (see :cpp:enum:`_LMP_STYLE_CONST`)
     - Type (see :cpp:enum:`_LMP_TYPE_CONST`)
     - Returned type
     - Returned data
   * - LMP_STYLE_GLOBAL
     - LMP_TYPE_SCALAR
     - ``double *``
     - Copy of global scalar
   * - LMP_STYLE_GLOBAL
     - LMP_TYPE_VECTOR
     - ``double *``
     - Copy of global vector element at index nrow
   * - LMP_STYLE_GLOBAL
     - LMP_TYPE_ARRAY
     - ``double *``
     - Copy of global array element at nrow, ncol
   * - LMP_STYLE_GLOBAL
     - LMP_SIZE_VECTOR
     - ``int *``
     - Length of global vector
   * - LMP_STYLE_GLOBAL
     - LMP_SIZE_ROWS
     - ``int *``
     - Rows in global array
   * - LMP_STYLE_GLOBAL
     - LMP_SIZE_COLS
     - ``int *``
     - Columns in global array
   * - LMP_STYLE_ATOM
     - LMP_TYPE_VECTOR
     - ``double *``
     - Per-atom value
   * - LMP_STYLE_ATOM
     - LMP_TYPE_ARRAY
     - ``double **``
     - Per-atom vector
   * - LMP_STYLE_ATOM
     - LMP_SIZE_COLS
     - ``int *``
     - Columns of per-atom array, 0 if vector
   * - LMP_STYLE_LOCAL
     - LMP_TYPE_VECTOR
     - ``double *``
     - Local data vector
   * - LMP_STYLE_LOCAL
     - LMP_TYPE_ARRAY
     - ``double **``
     - Local data array
   * - LMP_STYLE_LOCAL
     - LMP_SIZE_ROWS
     - ``int *``
     - Number of local data rows
   * - LMP_STYLE_LOCAL
     - LMP_SIZE_COLS
     - ``int *``
     - Number of local data columns

.. warning::

   The pointers returned by this function for per-atom or local data are
   generally not persistent, since the computed data may be re-distributed,
   re-allocated, and re-ordered at every invocation of the fix.  It is thus
   advisable to re-invoke this function before the data is accessed, or
   make a copy, if the data shall be used after other LAMMPS commands have
   been issued.

.. note::

   LAMMPS cannot easily check if it is valid to access the data, so it
   may fail with an error.  The caller has to avoid such an error.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  id      string with ID of the fix
 * \param  style   constant indicating the style of data requested
                   (global, per-atom, or local)
 * \param  type    constant indicating type of data (scalar, vector,
                   or array) or size of rows or columns
 * \param  nrow    row index (only used for global vectors and arrays)
 * \param  ncol    column index (only used for global arrays)
 * \return         pointer (cast to ``void *``) to the location of the
 *                 requested data or ``NULL`` if not found. */

void *lammps_extract_fix(void *handle, char *id, int style, int type,
                         int nrow, int ncol)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
    int ifix = lmp->modify->find_fix(id);
    if (ifix < 0) return nullptr;
    Fix *fix = lmp->modify->fix[ifix];

    if (style == LMP_STYLE_GLOBAL) {
      if (type == LMP_TYPE_SCALAR) {
        if (!fix->scalar_flag) return nullptr;
        double *dptr = (double *) malloc(sizeof(double));
        *dptr = fix->compute_scalar();
        return (void *) dptr;
      }
      if (type == LMP_TYPE_VECTOR) {
        if (!fix->vector_flag) return nullptr;
        double *dptr = (double *) malloc(sizeof(double));
        *dptr = fix->compute_vector(nrow);
        return (void *) dptr;
      }
      if (type == LMP_TYPE_ARRAY) {
        if (!fix->array_flag) return nullptr;
        double *dptr = (double *) malloc(sizeof(double));
        *dptr = fix->compute_array(nrow,ncol);
        return (void *) dptr;
      }
      if (type == LMP_SIZE_VECTOR) {
        if (!fix->vector_flag) return nullptr;
        return (void *) &fix->size_vector;
      }
      if ((type == LMP_SIZE_ROWS) || (type == LMP_SIZE_COLS)) {
        if (!fix->array_flag) return nullptr;
        if (type == LMP_SIZE_ROWS)
          return (void *) &fix->size_array_rows;
        else
          return (void *) &fix->size_array_cols;
      }
    }

    if (style == LMP_STYLE_ATOM) {
      if (!fix->peratom_flag) return nullptr;
      if (type == LMP_TYPE_VECTOR) return (void *) fix->vector_atom;
      if (type == LMP_TYPE_ARRAY) return (void *) fix->array_atom;
      if (type == LMP_SIZE_COLS) return (void *) &fix->size_peratom_cols;
    }

    if (style == LMP_STYLE_LOCAL) {
      if (!fix->local_flag) return nullptr;
      if (type == LMP_TYPE_SCALAR) return (void *) &fix->size_local_rows;
      if (type == LMP_TYPE_VECTOR) return (void *) fix->vector_local;
      if (type == LMP_TYPE_ARRAY) return (void *) fix->array_local;
      if (type == LMP_SIZE_ROWS) return (void *) &fix->size_local_rows;
      if (type == LMP_SIZE_COLS) return (void *) &fix->size_local_cols;
    }
  }
  END_CAPTURE

  return nullptr;
}

/* ---------------------------------------------------------------------- */

/** Get pointer to data from a LAMMPS variable.
 *
\verbatim embed:rst

This function returns a pointer to data from a LAMMPS :doc:`variable`
identified by its name.  When the variable is either an *equal*\ -style
compatible or an *atom*\ -style variable the variable is evaluated and
the corresponding value(s) returned.  Variables of style *internal*
are compatible with *equal*\ -style variables and so are *python*\
-style variables, if they return a numeric value.  For other
variable styles their string value is returned.  The function returns
``NULL`` when a variable of the provided *name* is not found or of an
incompatible style.  The *group* argument is only used for *atom*\
-style variables and ignored otherwise.  If set to ``NULL`` when
extracting data from and *atom*\ -style variable, the group is assumed
to be "all".

When requesting data from an *equal*\ -style or compatible variable
this function allocates storage for a single double value, copies the
returned value to it, and returns a pointer to the location of the
copy.  Therefore the allocated storage needs to be freed after its
use to avoid a memory leak. Example:

.. code-block:: c

   double *dptr = (double *) lammps_extract_variable(handle,name,NULL);
   double value = *dptr;
   lammps_free((void *)dptr);

For *atom*\ -style variables the data returned is a pointer to an
allocated block of storage of double of the length ``atom->nlocal``.
Since the data is returned a copy, the location will persist, but its
content will not be updated, in case the variable is re-evaluated.
To avoid a memory leak this pointer needs to be freed after use in
the calling program.

For other variable styles the returned pointer needs to be cast to
a char pointer.

.. code-block:: c

   const char *cptr = (const char *) lammps_extract_variable(handle,name,NULL);
   printf("The value of variable %s is %s\n", name, cptr);

.. note::

   LAMMPS cannot easily check if it is valid to access the data
   referenced by the variables, e.g. computes or fixes or thermodynamic
   info, so it may fail with an error.  The caller has to make certain,
   that the data is extracted only when it safe to evaluate the variable
   and thus an error and crash is avoided.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  name    name of the variable
 * \param  group   group-ID for atom style variable or ``NULL``
 * \return         pointer (cast to ``void *``) to the location of the
 *                 requested data or ``NULL`` if not found. */

void *lammps_extract_variable(void *handle, const char *name, const char *group)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
    int ivar = lmp->input->variable->find(name);
    if (ivar < 0) return nullptr;

    if (lmp->input->variable->equalstyle(ivar)) {
      double *dptr = (double *) malloc(sizeof(double));
      *dptr = lmp->input->variable->compute_equal(ivar);
      return (void *) dptr;
    } else if (lmp->input->variable->atomstyle(ivar)) {
      if (group == nullptr) group = (char *)"all";
      int igroup = lmp->group->find(group);
      if (igroup < 0) return nullptr;
      int nlocal = lmp->atom->nlocal;
      double *vector = (double *) malloc(nlocal*sizeof(double));
      lmp->input->variable->compute_atom(ivar,igroup,vector,1,0);
      return (void *) vector;
    } else {
      return lmp->input->variable->retrieve(name);
    }
  }
  END_CAPTURE

  return nullptr;
}

/* ---------------------------------------------------------------------- */

/** Set the value of a string-style variable.
 *
 * This function assigns a new value from the string str to the
 * string-style variable name. Returns -1 if a variable of that
 * name does not exist or is not a string-style variable, otherwise 0.
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \param  name    name of the variable
 * \param  str     new value of the variable
 * \return         0 on success or -1 on failure
 */
int lammps_set_variable(void *handle, char *name, char *str)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  int err = -1;

  BEGIN_CAPTURE
  {
    err = lmp->input->variable->set_string(name,str);
  }
  END_CAPTURE

  return err;
}

// ----------------------------------------------------------------------
// Library functions for scatter/gather operations of data
// ----------------------------------------------------------------------

/* ----------------------------------------------------------------------
   gather the named atom-based entity for all atoms
     return it in user-allocated data
   data will be ordered by atom ID
     requirement for consecutive atom IDs (1 to N)
   see gather_atoms_concat() to return data for all atoms, unordered
   see gather_atoms_subset() to return data for only a subset of atoms
   name = desired quantity, e.g. x or charge
   type = 0 for integer values, 1 for double values
   count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
     use count = 3 with "image" if want single image flag unpacked into xyz
   return atom-based values in 1d data, ordered by count, then by atom ID
     e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
     data must be pre-allocated by caller to correct length
     correct length = count*Natoms, as queried by get_natoms()
   method:
     alloc and zero count*Natom length vector
     loop over Nlocal to fill vector with my values
     Allreduce to sum vector into data across all procs
------------------------------------------------------------------------- */

void lammps_gather_atoms(void *handle, char *name, int type, int count, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_gather_atoms() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,offset;

    // error if tags are not defined or not consecutive
    // NOTE: test that name = image or ids is not a 64-bit int in code?

    int flag = 0;
    if (lmp->atom->tag_enable == 0 || lmp->atom->tag_consecutive() == 0)
      flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_gather_atoms");
      return;
    }

    int natoms = static_cast<int> (lmp->atom->natoms);

    void *vptr = lmp->atom->extract(name);
    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_gather_atoms: unknown property name");
      return;
    }

    // copy = Natom length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data, ordered by atom ID

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgunpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgunpack) vector = (int *) vptr;
      else array = (int **) vptr;

      int *copy;
      lmp->memory->create(copy,count*natoms,"lib/gather:copy");
      for (i = 0; i < count*natoms; i++) copy[i] = 0;

      tagint *tag = lmp->atom->tag;
      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < nlocal; i++)
          copy[tag[i]-1] = vector[i];

      } else if (imgunpack) {
        for (i = 0; i < nlocal; i++) {
          offset = count*(tag[i]-1);
          const int image = vector[i];
          copy[offset++] = (image & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMGBITS) & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMG2BITS) & IMGMASK) - IMGMAX;
        }

      } else {
        for (i = 0; i < nlocal; i++) {
          offset = count*(tag[i]-1);
          for (j = 0; j < count; j++)
            copy[offset++] = array[i][j];
        }
      }

      MPI_Allreduce(copy,data,count*natoms,MPI_INT,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);

    } else if (type == 1) {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;

      double *copy;
      lmp->memory->create(copy,count*natoms,"lib/gather:copy");
      for (i = 0; i < count*natoms; i++) copy[i] = 0.0;

      tagint *tag = lmp->atom->tag;
      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < nlocal; i++)
          copy[tag[i]-1] = vector[i];

      } else {
        for (i = 0; i < nlocal; i++) {
          offset = count*(tag[i]-1);
          for (j = 0; j < count; j++)
            copy[offset++] = array[i][j];
        }
      }

      MPI_Allreduce(copy,data,count*natoms,MPI_DOUBLE,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);
    } else {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_gather_atoms: unsupported data type");
      return;
    }
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
   gather the named atom-based entity for all atoms
     return it in user-allocated data
   data will be a concatenation of chunks of each proc's atoms,
     in whatever order the atoms are on each proc
     no requirement for consecutive atom IDs (1 to N)
     can do a gather_atoms_concat for "id" if need to know atom IDs
   see gather_atoms() to return data ordered by consecutive atom IDs
   see gather_atoms_subset() to return data for only a subset of atoms
   name = desired quantity, e.g. x or charge
   type = 0 for integer values, 1 for double values
   count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
     use count = 3 with "image" if want single image flag unpacked into xyz
   return atom-based values in 1d data, ordered by count, then by atom
     e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
     data must be pre-allocated by caller to correct length
     correct length = count*Natoms, as queried by get_natoms()
   method:
     Allgather Nlocal atoms from each proc into data
------------------------------------------------------------------------- */

void lammps_gather_atoms_concat(void *handle, char *name, int type, int count, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_gather_atoms_concat() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,offset;

    // error if tags are not defined
    // NOTE: test that name = image or ids is not a 64-bit int in code?

    int flag = 0;
    if (lmp->atom->tag_enable == 0) flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_gather_atoms");
      return;
    }

    int natoms = static_cast<int> (lmp->atom->natoms);

    void *vptr = lmp->atom->extract(name);
    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_gather_atoms: unknown property name");
      return;
    }

    // perform MPI_Allgatherv on each proc's chunk of Nlocal atoms

    int nprocs = lmp->comm->nprocs;

    int *recvcounts,*displs;
    lmp->memory->create(recvcounts,nprocs,"lib/gather:recvcounts");
    lmp->memory->create(displs,nprocs,"lib/gather:displs");

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgunpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgunpack) vector = (int *) vptr;
      else array = (int **) vptr;

      int *copy;
      lmp->memory->create(copy,count*natoms,"lib/gather:copy");
      for (i = 0; i < count*natoms; i++) copy[i] = 0;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        MPI_Allgather(&nlocal,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(vector,nlocal,MPI_INT,data,recvcounts,displs,
                       MPI_INT,lmp->world);

      } else if (imgunpack) {
        int *copy;
        lmp->memory->create(copy,count*nlocal,"lib/gather:copy");
        offset = 0;
        for (i = 0; i < nlocal; i++) {
          const int image = vector[i];
          copy[offset++] = (image & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMGBITS) & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMG2BITS) & IMGMASK) - IMGMAX;
        }
        int n = count*nlocal;
        MPI_Allgather(&n,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(copy,count*nlocal,MPI_INT,
                       data,recvcounts,displs,MPI_INT,lmp->world);
        lmp->memory->destroy(copy);

      } else {
        int n = count*nlocal;
        MPI_Allgather(&n,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(&array[0][0],count*nlocal,MPI_INT,
                       data,recvcounts,displs,MPI_INT,lmp->world);
      }

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        MPI_Allgather(&nlocal,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(vector,nlocal,MPI_DOUBLE,data,recvcounts,displs,
                       MPI_DOUBLE,lmp->world);

      } else {
        int n = count*nlocal;
        MPI_Allgather(&n,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(&array[0][0],count*nlocal,MPI_DOUBLE,
                       data,recvcounts,displs,MPI_DOUBLE,lmp->world);
      }
    }

    lmp->memory->destroy(recvcounts);
    lmp->memory->destroy(displs);
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
   gather the named atom-based entity for a subset of atoms
     return it in user-allocated data
   data will be ordered by requested atom IDs
     no requirement for consecutive atom IDs (1 to N)
   see gather_atoms() to return data for all atoms, ordered by consecutive IDs
   see gather_atoms_concat() to return data for all atoms, unordered
   name = desired quantity, e.g. x or charge
   type = 0 for integer values, 1 for double values
   count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
     use count = 3 with "image" if want single image flag unpacked into xyz
   ndata = # of atoms to return data for (could be all atoms)
   ids = list of Ndata atom IDs to return data for
   return atom-based values in 1d data, ordered by count, then by atom
     e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
     data must be pre-allocated by caller to correct length
     correct length = count*Ndata
   method:
     alloc and zero count*Ndata length vector
     loop over Ndata to fill vector with my values
     Allreduce to sum vector into data across all procs
------------------------------------------------------------------------- */

void lammps_gather_atoms_subset(void *handle, char *name, int type, int count,
                                int ndata, int *ids, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_gather_atoms_subset() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,m,offset;
    tagint id;

    // error if tags are not defined
    // NOTE: test that name = image or ids is not a 64-bit int in code?

    int flag = 0;
    if (lmp->atom->tag_enable == 0) flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_gather_atoms_subset");
      return;
    }

    void *vptr = lmp->atom->extract(name);
    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_gather_atoms_subset: "
                            "unknown property name");
      return;
    }

    // copy = Ndata length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgunpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgunpack) vector = (int *) vptr;
      else array = (int **) vptr;

      int *copy;
      lmp->memory->create(copy,count*ndata,"lib/gather:copy");
      for (i = 0; i < count*ndata; i++) copy[i] = 0;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal)
            copy[i] = vector[m];
        }

      } else if (imgunpack) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal) {
            offset = count*i;
            const int image = vector[m];
            copy[offset++] = (image & IMGMASK) - IMGMAX;
            copy[offset++] = ((image >> IMGBITS) & IMGMASK) - IMGMAX;
            copy[offset++] = ((image >> IMG2BITS) & IMGMASK) - IMGMAX;
          }
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal) {
            offset = count*i;
            for (j = 0; j < count; j++)
              copy[offset++] = array[m][j];
          }
        }
      }

      MPI_Allreduce(copy,data,count*ndata,MPI_INT,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;

      double *copy;
      lmp->memory->create(copy,count*ndata,"lib/gather:copy");
      for (i = 0; i < count*ndata; i++) copy[i] = 0.0;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal)
            copy[i] = vector[m];
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal) {
            offset = count*i;
            for (j = 0; j < count; j++)
              copy[offset++] = array[m][j];
          }
        }
      }

      MPI_Allreduce(copy,data,count*ndata,MPI_DOUBLE,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);
    }
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
   scatter the named atom-based entity in data to all atoms
   data is ordered by atom ID
     requirement for consecutive atom IDs (1 to N)
   see scatter_atoms_subset() to scatter data for some (or all) atoms, unordered
   name = desired quantity, e.g. x or charge
   type = 0 for integer values, 1 for double values
   count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
     use count = 3 with "image" for xyz to be packed into single image flag
   data = atom-based values in 1d data, ordered by count, then by atom ID
     e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
     data must be correct length = count*Natoms, as queried by get_natoms()
   method:
     loop over Natoms, if I own atom ID, set its values from data
------------------------------------------------------------------------- */

void lammps_scatter_atoms(void *handle, char *name, int type, int count, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_scatter_atoms() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,m,offset;

    // error if tags are not defined or not consecutive or no atom map
    // NOTE: test that name = image or ids is not a 64-bit int in code?

    int flag = 0;
    if (lmp->atom->tag_enable == 0 || lmp->atom->tag_consecutive() == 0)
      flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (lmp->atom->map_style == Atom::MAP_NONE) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_scatter_atoms");
      return;
    }

    int natoms = static_cast<int> (lmp->atom->natoms);

    void *vptr = lmp->atom->extract(name);
    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,
                            "lammps_scatter_atoms: unknown property name");
      return;
    }

    // copy = Natom length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data, ordered by atom ID

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgpack) vector = (int *) vptr;
      else array = (int **) vptr;
      int *dptr = (int *) data;

      if (count == 1) {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0)
            vector[m] = dptr[i];

      } else if (imgpack) {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0) {
            offset = count*i;
            int image = dptr[offset++] + IMGMAX;
            image += (dptr[offset++] + IMGMAX) << IMGBITS;
            image += (dptr[offset++] + IMGMAX) << IMG2BITS;
            vector[m] = image;
          }

      } else {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
      }

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;
      double *dptr = (double *) data;

      if (count == 1) {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0)
            vector[m] = dptr[i];

      } else {
        for (i = 0; i < natoms; i++) {
          if ((m = lmp->atom->map(i+1)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
        }
      }
    }
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
   scatter the named atom-based entity in data to a subset of atoms
   data is ordered by provided atom IDs
     no requirement for consecutive atom IDs (1 to N)
   see scatter_atoms() to scatter data for all atoms, ordered by consecutive IDs
   name = desired quantity, e.g. x or charge
   type = 0 for integer values, 1 for double values
   count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
     use count = 3 with "image" for xyz to be packed into single image flag
   ndata = # of atoms in ids and data (could be all atoms)
   ids = list of Ndata atom IDs to scatter data to
   data = atom-based values in 1d data, ordered by count, then by atom ID
     e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
     data must be correct length = count*Ndata
   method:
     loop over Ndata, if I own atom ID, set its values from data
------------------------------------------------------------------------- */

void lammps_scatter_atoms_subset(void *handle, char *name, int type, int count,
                                 int ndata, int *ids, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_scatter_atoms_subset() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,m,offset;
    tagint id;

    // error if tags are not defined or no atom map
    // NOTE: test that name = image or ids is not a 64-bit int in code?

    int flag = 0;
    if (lmp->atom->tag_enable == 0) flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (lmp->atom->map_style == Atom::MAP_NONE) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_scatter_atoms_subset");
      return;
    }

    void *vptr = lmp->atom->extract(name);
    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,
                            "lammps_scatter_atoms_subset: unknown property name");
      return;
    }

    // copy = Natom length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data, ordered by atom ID

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgpack) vector = (int *) vptr;
      else array = (int **) vptr;
      int *dptr = (int *) data;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0)
            vector[m] = dptr[i];
        }

      } else if (imgpack) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0) {
            offset = count*i;
            int image = dptr[offset++] + IMGMAX;
            image += (dptr[offset++] + IMGMAX) << IMGBITS;
            image += (dptr[offset++] + IMGMAX) << IMG2BITS;
            vector[m] = image;
          }
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
        }
      }

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;
      double *dptr = (double *) data;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0)
            vector[m] = dptr[i];
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
        }
      }
    }
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
  Contributing author: Thomas Swinburne (CNRS & CINaM, Marseille, France)
  gather the named atom-based entity for all atoms
    return it in user-allocated data
  data will be ordered by atom ID
    requirement for consecutive atom IDs (1 to N)
  see gather_concat() to return data for all atoms, unordered
  see gather_subset() to return data for only a subset of atoms
  name = "x" , "f" or other atom properties
        "d_name" or "i_name" for fix property/atom quantities
        "f_fix", "c_compute" for fixes / computes
        will return error if fix/compute doesn't isn't atom-based
  type = 0 for integer values, 1 for double values
  count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
    use count = 3 with "image" if want single image flag unpacked into xyz
  return atom-based values in 1d data, ordered by count, then by atom ID
    e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
    data must be pre-allocated by caller to correct length
    correct length = count*Natoms, as queried by get_natoms()
  method:
    alloc and zero count*Natom length vector
    loop over Nlocal to fill vector with my values
    Allreduce to sum vector into data across all procs
------------------------------------------------------------------------- */

void lammps_gather(void *handle, char *name, int type, int count, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
  lmp->error->all(FLERR,"Library function lammps_gather"
                  " not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,offset,fcid,ltype;

    // error if tags are not defined or not consecutive
    int flag = 0;
    if (lmp->atom->tag_enable == 0 || lmp->atom->tag_consecutive() == 0)
      flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_gather");
      return;
    }

    int natoms = static_cast<int> (lmp->atom->natoms);

    void *vptr = lmp->atom->extract(name);

    if (vptr==nullptr && utils::strmatch(name,"^f_")) { // fix

      fcid = lmp->modify->find_fix(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather: unknown fix id");
        return;
      }

      if (lmp->modify->fix[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather:"
                              " fix does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->fix[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather:"
                              " count != values peratom for fix");
        return;
      }

      if (lmp->update->ntimestep % lmp->modify->fix[fcid]->peratom_freq) {
        if (lmp->comm->me == 0)
          lmp->error->all(FLERR,"lammps_gather:"
                          " fix not computed at compatible time");
        return;
      }

      if (count==1) vptr = (void *) lmp->modify->fix[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->fix[fcid]->array_atom;
    }

    if (vptr==nullptr && utils::strmatch(name,"^c_")) { // compute

      fcid = lmp->modify->find_compute(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather: unknown compute id");
        return;
      }

      if (lmp->modify->compute[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather:"
                              " compute does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->compute[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather:"
                              " count != values peratom for compute");
        return;
      }

      if (lmp->modify->compute[fcid]->invoked_peratom != lmp->update->ntimestep)
        lmp->modify->compute[fcid]->compute_peratom();

      if (count==1) vptr = (void *) lmp->modify->compute[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->compute[fcid]->array_atom;


    }

    // property / atom

    if ((vptr == nullptr) && (utils::strmatch(name,"^[di]_"))) {
      fcid = lmp->atom->find_custom(&name[2], ltype);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather: unknown property/atom id");
        return;
      }
      if (ltype != type) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather: mismatch property/atom type");
        return;
      }
      if (count != 1) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather: property/atom has count=1");
        return;
      }
      if (ltype==0) vptr = (void *) lmp->atom->ivector[fcid];
      else vptr = (void *) lmp->atom->dvector[fcid];
    }

    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_gather: unknown property name");
      return;
    }

    // copy = Natom length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data, ordered by atom ID
    if (type==0) {
      int *vector = nullptr;
      int **array = nullptr;

      const int imgunpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgunpack) vector = (int *) vptr;
      else array = (int **) vptr;

      int *copy;
      lmp->memory->create(copy,count*natoms,"lib/gather:copy");
      for (i = 0; i < count*natoms; i++) copy[i] = 0;

      tagint *tag = lmp->atom->tag;
      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < nlocal; i++)
          copy[tag[i]-1] = vector[i];

      } else if (imgunpack) {
        for (i = 0; i < nlocal; i++) {
          offset = count*(tag[i]-1);
          const int image = vector[i];
          copy[offset++] = (image & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMGBITS) & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMG2BITS) & IMGMASK) - IMGMAX;
        }

      } else {
        for (i = 0; i < nlocal; i++) {
          offset = count*(tag[i]-1);
          for (j = 0; j < count; j++)
            copy[offset++] = array[i][j];
        }
      }

      MPI_Allreduce(copy,data,count*natoms,MPI_INT,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);

    } else {

      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;

      double *copy;
      lmp->memory->create(copy,count*natoms,"lib/gather:copy");
      for (i = 0; i < count*natoms; i++) copy[i] = 0.0;

      tagint *tag = lmp->atom->tag;
      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < nlocal; i++)
          copy[tag[i]-1] = vector[i];
      } else {
        for (i = 0; i < nlocal; i++) {
          offset = count*(tag[i]-1);
          for (j = 0; j < count; j++)
            copy[offset++] = array[i][j];
        }
      }
      MPI_Allreduce(copy,data,count*natoms,MPI_DOUBLE,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);
    }
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
  Contributing author: Thomas Swinburne (CNRS & CINaM, Marseille, France)
  gather the named atom-based entity for all atoms
    return it in user-allocated data
  data will be ordered by atom ID
    requirement for consecutive atom IDs (1 to N)
  see gather() to return data ordered by consecutive atom IDs
  see gather_subset() to return data for only a subset of atoms
  name = "x" , "f" or other atom properties
        "d_name" or "i_name" for fix property/atom quantities
        "f_fix", "c_compute" for fixes / computes
        will return error if fix/compute doesn't isn't atom-based
  type = 0 for integer values, 1 for double values
  count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
    use count = 3 with "image" if want single image flag unpacked into xyz
  return atom-based values in 1d data, ordered by count, then by atom ID
    e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
    data must be pre-allocated by caller to correct length
    correct length = count*Natoms, as queried by get_natoms()
  method:
    alloc and zero count*Natom length vector
    loop over Nlocal to fill vector with my values
    Allreduce to sum vector into data across all procs
------------------------------------------------------------------------- */

void lammps_gather_concat(void *handle, char *name, int type, int count, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_gather_concat"
                          " not compatible with -DLAMMPS_BIGBIG");
#else
    int i,offset,fcid,ltype;

    // error if tags are not defined or not consecutive
    int flag = 0;
    if (lmp->atom->tag_enable == 0) flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_gather_concat");
      return;
    }


    int natoms = static_cast<int> (lmp->atom->natoms);

    void *vptr = lmp->atom->extract(name);

    if (vptr==nullptr && utils::strmatch(name,"^f_")) { // fix

      fcid = lmp->modify->find_fix(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat: unknown fix id");
        return;
      }

      if (lmp->modify->fix[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat:"
                              " fix does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->fix[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat:"
                              " count != values peratom for fix");
        return;
      }


      if (lmp->update->ntimestep % lmp->modify->fix[fcid]->peratom_freq) {
        if (lmp->comm->me == 0)
          lmp->error->all(FLERR,"lammps_gather_concat:"
                          " fix not computed at compatible time");
        return;
      }

      if (count==1) vptr = (void *) lmp->modify->fix[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->fix[fcid]->array_atom;
    }

    if (vptr==nullptr && utils::strmatch(name,"^c_")) { // compute

      fcid = lmp->modify->find_compute(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat: unknown compute id");
        return;
      }

      if (lmp->modify->compute[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat:"
                              " compute does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->compute[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat:"
                              " count != values peratom for compute");
        return;
      }

      if (lmp->modify->compute[fcid]->invoked_peratom != lmp->update->ntimestep)
        lmp->modify->compute[fcid]->compute_peratom();

      if (count==1) vptr = (void *) lmp->modify->compute[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->compute[fcid]->array_atom;


    }

    if (vptr==nullptr && utils::strmatch(name,"^[di]_")) { // property / atom

      fcid = lmp->atom->find_custom(&name[2], ltype);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat: "
                              "unknown property/atom id");
        return;
      }
      if (ltype != type) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat: "
                              "mismatch property/atom type");
        return;
      }
      if (count != 1) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_concat: "
                              "property/atom has count=1");
        return;
      }
      if (ltype==0) vptr = (void *) lmp->atom->ivector[fcid];
      else vptr = (void *) lmp->atom->dvector[fcid];

    }

    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_gather_concat: unknown property name");
      return;
    }

    // perform MPI_Allgatherv on each proc's chunk of Nlocal atoms

    int nprocs = lmp->comm->nprocs;

    int *recvcounts,*displs;
    lmp->memory->create(recvcounts,nprocs,"lib/gather:recvcounts");
    lmp->memory->create(displs,nprocs,"lib/gather:displs");

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgunpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgunpack) vector = (int *) vptr;
      else array = (int **) vptr;

      int *copy;
      lmp->memory->create(copy,count*natoms,"lib/gather:copy");
      for (i = 0; i < count*natoms; i++) copy[i] = 0;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        MPI_Allgather(&nlocal,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(vector,nlocal,MPI_INT,data,recvcounts,displs,
                       MPI_INT,lmp->world);

      } else if (imgunpack) {
        int *copy;
        lmp->memory->create(copy,count*nlocal,"lib/gather:copy");
        offset = 0;
        for (i = 0; i < nlocal; i++) {
          const int image = vector[i];
          copy[offset++] = (image & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMGBITS) & IMGMASK) - IMGMAX;
          copy[offset++] = ((image >> IMG2BITS) & IMGMASK) - IMGMAX;
        }
        int n = count*nlocal;
        MPI_Allgather(&n,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(copy,count*nlocal,MPI_INT,
                       data,recvcounts,displs,MPI_INT,lmp->world);
        lmp->memory->destroy(copy);

      } else {
        int n = count*nlocal;
        MPI_Allgather(&n,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(&array[0][0],count*nlocal,MPI_INT,
                       data,recvcounts,displs,MPI_INT,lmp->world);
      }

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        MPI_Allgather(&nlocal,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(vector,nlocal,MPI_DOUBLE,data,recvcounts,displs,
                       MPI_DOUBLE,lmp->world);

      } else {
        int n = count*nlocal;
        MPI_Allgather(&n,1,MPI_INT,recvcounts,1,MPI_INT,lmp->world);
        displs[0] = 0;
        for (i = 1; i < nprocs; i++)
          displs[i] = displs[i-1] + recvcounts[i-1];
        MPI_Allgatherv(&array[0][0],count*nlocal,MPI_DOUBLE,
                       data,recvcounts,displs,MPI_DOUBLE,lmp->world);
      }
    }

    lmp->memory->destroy(recvcounts);
    lmp->memory->destroy(displs);
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
  Contributing author: Thomas Swinburne (CNRS & CINaM, Marseille, France)
  gather the named atom-based entity for all atoms
    return it in user-allocated data
  data will be ordered by atom ID
    requirement for consecutive atom IDs (1 to N)
  see gather() to return data ordered by consecutive atom IDs
  see gather_concat() to return data for all atoms, unordered
  name = "x" , "f" or other atom properties
        "d_name" or "i_name" for fix property/atom quantities
        "f_fix", "c_compute" for fixes / computes
        will return error if fix/compute doesn't isn't atom-based
  type = 0 for integer values, 1 for double values
  count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
    use count = 3 with "image" if want single image flag unpacked into xyz
  return atom-based values in 1d data, ordered by count, then by atom ID
    e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
    data must be pre-allocated by caller to correct length
    correct length = count*Natoms, as queried by get_natoms()
  method:
    alloc and zero count*Natom length vector
    loop over Nlocal to fill vector with my values
    Allreduce to sum vector into data across all procs
------------------------------------------------------------------------- */

void lammps_gather_subset(void *handle, char *name,
                                int type, int count,
                                int ndata, int *ids, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
      lmp->error->all(FLERR,"Library function lammps_gather_subset() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,m,offset,fcid,ltype;
    tagint id;

    // error if tags are not defined or not consecutive
    int flag = 0;
    if (lmp->atom->tag_enable == 0) flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_gather_subset");
      return;
    }

    void *vptr = lmp->atom->extract(name);

    if (vptr==nullptr && utils::strmatch(name,"^f_")) { // fix

      fcid = lmp->modify->find_fix(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset: unknown fix id");
        return;
      }

      if (lmp->modify->fix[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset:"
                              " fix does not return peratom data");
        return;
      }

      if (count>1 && lmp->modify->fix[fcid]->size_peratom_cols != count) {
        lmp->error->warning(FLERR,"lammps_gather_subset:"
                                  " count != values peratom for fix");
        return;
      }

      if (lmp->update->ntimestep % lmp->modify->fix[fcid]->peratom_freq) {
        if (lmp->comm->me == 0)
          lmp->error->all(FLERR,"lammps_gather_subset:"
                          " fix not computed at compatible time");
        return;
      }

      if (count==1) vptr = (void *) lmp->modify->fix[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->fix[fcid]->array_atom;
    }

    if (vptr==nullptr && utils::strmatch(name,"^c_")) { // compute

      fcid = lmp->modify->find_compute(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset: unknown compute id");
        return;
      }

      if (lmp->modify->compute[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset:"
                              " compute does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->compute[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset:"
                              " count != values peratom for compute");
        return;
      }

      if (lmp->modify->compute[fcid]->invoked_peratom != lmp->update->ntimestep)
        lmp->modify->compute[fcid]->compute_peratom();

      if (count==1) vptr = (void *) lmp->modify->compute[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->compute[fcid]->array_atom;


    }

    if (vptr==nullptr && utils::strmatch(name,"^[di]_")) { // property / atom

      fcid = lmp->atom->find_custom(&name[2], ltype);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset: "
                              "unknown property/atom id");
        return;
      }
      if (ltype != type) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset: "
                              "mismatch property/atom type");
        return;
      }
      if (count != 1) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_gather_subset: "
                              "property/atom has count=1");
        return;
      }
      if (ltype==0) vptr = (void *) lmp->atom->ivector[fcid];
      else vptr = (void *) lmp->atom->dvector[fcid];
    }

    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_gather_subset: "
                            "unknown property name");
      return;
    }

    // copy = Ndata length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgunpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgunpack) vector = (int *) vptr;
      else array = (int **) vptr;

      int *copy;
      lmp->memory->create(copy,count*ndata,"lib/gather:copy");
      for (i = 0; i < count*ndata; i++) copy[i] = 0;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal)
            copy[i] = vector[m];
        }

      } else if (imgunpack) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal) {
            offset = count*i;
            const int image = vector[m];
            copy[offset++] = (image & IMGMASK) - IMGMAX;
            copy[offset++] = ((image >> IMGBITS) & IMGMASK) - IMGMAX;
            copy[offset++] = ((image >> IMG2BITS) & IMGMASK) - IMGMAX;
          }
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal) {
            offset = count*i;
            for (j = 0; j < count; j++)
              copy[offset++] = array[m][j];
          }
        }
      }

      MPI_Allreduce(copy,data,count*ndata,MPI_INT,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;

      double *copy;
      lmp->memory->create(copy,count*ndata,"lib/gather:copy");
      for (i = 0; i < count*ndata; i++) copy[i] = 0.0;

      int nlocal = lmp->atom->nlocal;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal)
            copy[i] = vector[m];
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0 && m < nlocal) {
            offset = count*i;
            for (j = 0; j < count; j++)
              copy[offset++] = array[m][j];
          }
        }
      }

      MPI_Allreduce(copy,data,count*ndata,MPI_DOUBLE,MPI_SUM,lmp->world);
      lmp->memory->destroy(copy);
    }
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
  Contributing author: Thomas Swinburne (CNRS & CINaM, Marseille, France)
  scatter the named atom-based entity in data to all atoms
  data will be ordered by atom ID
    requirement for consecutive atom IDs (1 to N)
  see scatter_subset() to scatter data for some (or all) atoms, unordered
  name = "x" , "f" or other atom properties
        "d_name" or "i_name" for fix property/atom quantities
        "f_fix", "c_compute" for fixes / computes
        will return error if fix/compute doesn't isn't atom-based
  type = 0 for integer values, 1 for double values
  count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
    use count = 3 with "image" if want single image flag unpacked into xyz
  return atom-based values in 1d data, ordered by count, then by atom ID
    e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
    data must be pre-allocated by caller to correct length
    correct length = count*Natoms, as queried by get_natoms()
  method:
    alloc and zero count*Natom length vector
    loop over Nlocal to fill vector with my values
    Allreduce to sum vector into data across all procs
------------------------------------------------------------------------- */

void lammps_scatter(void *handle, char *name, int type, int count, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_scatter() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,m,offset,fcid,ltype;

    // error if tags are not defined or not consecutive or no atom map
    // NOTE: test that name = image or ids is not a 64-bit int in code?

    int flag = 0;
    if (lmp->atom->tag_enable == 0 || lmp->atom->tag_consecutive() == 0)
      flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (lmp->atom->map_style == Atom::MAP_NONE) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_scatter");
      return;
    }

    int natoms = static_cast<int> (lmp->atom->natoms);

    void *vptr = lmp->atom->extract(name);

    if (vptr==nullptr && utils::strmatch(name,"^f_")) { // fix

      fcid = lmp->modify->find_fix(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter: unknown fix id");
        return;
      }

      if (lmp->modify->fix[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter:"
                              " fix does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->fix[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter:"
                              " count != values peratom for fix");
        return;
      }

      if (count==1) vptr = (void *) lmp->modify->fix[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->fix[fcid]->array_atom;
    }

    if (vptr==nullptr && utils::strmatch(name,"^c_")) { // compute

      fcid = lmp->modify->find_compute(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter: unknown compute id");
        return;
      }

      if (lmp->modify->compute[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter:"
                              " compute does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->compute[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter:"
                              " count != values peratom for compute");
        return;
      }

      if (lmp->modify->compute[fcid]->invoked_peratom != lmp->update->ntimestep)
        lmp->modify->compute[fcid]->compute_peratom();

      if (count==1) vptr = (void *) lmp->modify->compute[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->compute[fcid]->array_atom;


    }

    if (vptr==nullptr && utils::strmatch(name,"^[di]_")) { // property / atom

      fcid = lmp->atom->find_custom(&name[2], ltype);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter: unknown property/atom id");
        return;
      }
      if (ltype != type) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter: mismatch property/atom type");
        return;
      }
      if (count != 1) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter: property/atom has count=1");
        return;
      }
      if (ltype==0) vptr = (void *) lmp->atom->ivector[fcid];
      else vptr = (void *) lmp->atom->dvector[fcid];

    }

    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_scatter: unknown property name");
      return;
    }

    // copy = Natom length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data, ordered by atom ID

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgpack) vector = (int *) vptr;
      else array = (int **) vptr;
      int *dptr = (int *) data;

      if (count == 1) {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0)
            vector[m] = dptr[i];

      } else if (imgpack) {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0) {
            offset = count*i;
            int image = dptr[offset++] + IMGMAX;
            image += (dptr[offset++] + IMGMAX) << IMGBITS;
            image += (dptr[offset++] + IMGMAX) << IMG2BITS;
            vector[m] = image;
          }

      } else {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
      }

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;
      double *dptr = (double *) data;

      if (count == 1) {
        for (i = 0; i < natoms; i++)
          if ((m = lmp->atom->map(i+1)) >= 0)
            vector[m] = dptr[i];

      } else {
        for (i = 0; i < natoms; i++) {
          if ((m = lmp->atom->map(i+1)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
        }
      }
    }
#endif
  }
  END_CAPTURE
}

/* ----------------------------------------------------------------------
  Contributing author: Thomas Swinburne (CNRS & CINaM, Marseille, France)
   scatter the named atom-based entity in data to a subset of atoms
   data is ordered by provided atom IDs
     no requirement for consecutive atom IDs (1 to N)
   see scatter_atoms() to scatter data for all atoms, ordered by consecutive IDs
   name = desired quantity, e.g. x or charge
   type = 0 for integer values, 1 for double values
   count = # of per-atom values, e.g. 1 for type or charge, 3 for x or f
     use count = 3 with "image" for xyz to be packed into single image flag
   ndata = # of atoms in ids and data (could be all atoms)
   ids = list of Ndata atom IDs to scatter data to
   data = atom-based values in 1d data, ordered by count, then by atom ID
     e.g. x[0][0],x[0][1],x[0][2],x[1][0],x[1][1],x[1][2],x[2][0],...
     data must be correct length = count*Ndata
   method:
     loop over Ndata, if I own atom ID, set its values from data
------------------------------------------------------------------------- */

void lammps_scatter_subset(void *handle, char *name,int type, int count,
                                 int ndata, int *ids, void *data)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
#if defined(LAMMPS_BIGBIG)
    lmp->error->all(FLERR,"Library function lammps_scatter_subset() "
                    "is not compatible with -DLAMMPS_BIGBIG");
#else
    int i,j,m,offset,fcid,ltype;
    tagint id;

    // error if tags are not defined or no atom map
    // NOTE: test that name = image or ids is not a 64-bit int in code?

    int flag = 0;
    if (lmp->atom->tag_enable == 0) flag = 1;
    if (lmp->atom->natoms > MAXSMALLINT) flag = 1;
    if (lmp->atom->map_style == Atom::MAP_NONE) flag = 1;
    if (flag) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"Library error in lammps_scatter_atoms_subset");
      return;
    }

    void *vptr = lmp->atom->extract(name);

    if (vptr==nullptr && utils::strmatch(name,"^f_")) { // fix

      fcid = lmp->modify->find_fix(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset: unknown fix id");
        return;
      }

      if (lmp->modify->fix[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset:"
                              " fix does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->fix[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset:"
                              " count != values peratom for fix");
        return;
      }

      if (count==1) vptr = (void *) lmp->modify->fix[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->fix[fcid]->array_atom;
    }

    if (vptr==nullptr && utils::strmatch(name,"^c_")) { // compute

      fcid = lmp->modify->find_compute(&name[2]);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset: unknown compute id");
        return;
      }

      if (lmp->modify->compute[fcid]->peratom_flag == 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset:"
                              " compute does not return peratom data");
        return;
      }
      if (count>1 && lmp->modify->compute[fcid]->size_peratom_cols != count) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset:"
                              " count != values peratom for compute");
        return;
      }

      if (lmp->modify->compute[fcid]->invoked_peratom != lmp->update->ntimestep)
        lmp->modify->compute[fcid]->compute_peratom();

      if (count==1) vptr = (void *) lmp->modify->compute[fcid]->vector_atom;
      else vptr = (void *) lmp->modify->compute[fcid]->array_atom;
    }

    if (vptr==nullptr && utils::strmatch(name,"^[di]_")) { // property / atom

      fcid = lmp->atom->find_custom(&name[2], ltype);
      if (fcid < 0) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset: "
                              "unknown property/atom id");
        return;
      }
      if (ltype != type) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset: "
                              "mismatch property/atom type");
        return;
      }
      if (count != 1) {
        if (lmp->comm->me == 0)
          lmp->error->warning(FLERR,"lammps_scatter_subset: "
                              "property/atom has count=1");
        return;
      }
      if (ltype==0) vptr = (void *) lmp->atom->ivector[fcid];
      else vptr = (void *) lmp->atom->dvector[fcid];
    }

    if (vptr == nullptr) {
      if (lmp->comm->me == 0)
        lmp->error->warning(FLERR,"lammps_scatter_atoms_subset: "
                                  "unknown property name");
      return;
    }

    // copy = Natom length vector of per-atom values
    // use atom ID to insert each atom's values into copy
    // MPI_Allreduce with MPI_SUM to merge into data, ordered by atom ID

    if (type == 0) {
      int *vector = nullptr;
      int **array = nullptr;
      const int imgpack = (count == 3) && (strcmp(name,"image") == 0);

      if ((count == 1) || imgpack) vector = (int *) vptr;
      else array = (int **) vptr;
      int *dptr = (int *) data;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0)
            vector[m] = dptr[i];
        }

      } else if (imgpack) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0) {
            offset = count*i;
            int image = dptr[offset++] + IMGMAX;
            image += (dptr[offset++] + IMGMAX) << IMGBITS;
            image += (dptr[offset++] + IMGMAX) << IMG2BITS;
            vector[m] = image;
          }
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
        }
      }

    } else {
      double *vector = nullptr;
      double **array = nullptr;
      if (count == 1) vector = (double *) vptr;
      else array = (double **) vptr;
      double *dptr = (double *) data;

      if (count == 1) {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0)
            vector[m] = dptr[i];
        }

      } else {
        for (i = 0; i < ndata; i++) {
          id = ids[i];
          if ((m = lmp->atom->map(id)) >= 0) {
            offset = count*i;
            for (j = 0; j < count; j++)
              array[m][j] = dptr[offset++];
          }
        }
      }
    }
#endif
  }
  END_CAPTURE
}

/* ---------------------------------------------------------------------- */

/** Create N atoms from list of coordinates
 *
\verbatim embed:rst

The prototype for this function when compiling with ``-DLAMMPS_BIGBIG``
is:

.. code-block:: c

   int lammps_create_atoms(void *handle, int n, int64_t *id, int *type, double *x, double *v, int64_t *image, int bexpand);

This function creates additional atoms from a given list of coordinates
and a list of atom types.  Additionally the atom-IDs, velocities, and
image flags may be provided.  If atom-IDs are not provided, they will be
automatically created as a sequence following the largest existing
atom-ID.

This function is useful to add atoms to a simulation or - in tandem with
:cpp:func:`lammps_reset_box` - to restore a previously extracted and
saved state of a simulation.  Additional properties for the new atoms
can then be assigned via the :cpp:func:`lammps_scatter_atoms`
:cpp:func:`lammps_extract_atom` functions.

For non-periodic boundaries, atoms will **not** be created that have
coordinates outside the box unless it is a shrink-wrap boundary and the
shrinkexceed flag has been set to a non-zero value.  For periodic
boundaries atoms will be wrapped back into the simulation cell and its
image flags adjusted accordingly, unless explicit image flags are
provided.

The function returns the number of atoms created or -1 on failure, e.g.
when called before as box has been created.

Coordinates and velocities have to be given in a 1d-array in the order
X(1),Y(1),Z(1),X(2),Y(2),Z(2),...,X(N),Y(N),Z(N).

\endverbatim
 *
 * \param  handle   pointer to a previously created LAMMPS instance
 * \param  n        number of atoms, N, to be added to the system
 * \param  id       pointer to N atom IDs; ``NULL`` will generate IDs
 * \param  type     pointer to N atom types (required)
 * \param  x        pointer to 3N doubles with x-,y-,z- positions
                    of the new atoms (required)
 * \param  v        pointer to 3N doubles with x-,y-,z- velocities
                    of the new atoms (set to 0.0 if ``NULL``)
 * \param  image    pointer to N imageint sets of image flags, or ``NULL``
 * \param  bexpand  if 1, atoms outside of shrink-wrap boundaries will
                    still be created and not dropped and the box extended
 * \return          number of atoms created on success;
                    -1 on failure (no box, no atom IDs, etc.) */

int lammps_create_atoms(void *handle, int n, tagint *id, int *type,
                        double *x, double *v, imageint *image,
                        int bexpand)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  bigint natoms_prev = lmp->atom->natoms;

  BEGIN_CAPTURE
  {
    // error if box does not exist or tags not defined

    int flag = 0;
    std::string msg("Failure in lammps_create_atoms: ");
    if (lmp->domain->box_exist == 0) {
      flag = 1;
      msg += "trying to create atoms before before simulation box is defined";
    }
    if (lmp->atom->tag_enable == 0) {
      flag = 1;
      msg += "must have atom IDs to use this function";
    }

    if (flag) {
      if (lmp->comm->me == 0) lmp->error->warning(FLERR,msg);
      return -1;
    }

    // loop over all N atoms on all MPI ranks
    // if this proc would own it based on its coordinates, invoke create_atom()
    // optionally set atom tags and velocities

    Atom *atom = lmp->atom;
    Domain *domain = lmp->domain;
    int nlocal = atom->nlocal;

    int nlocal_prev = nlocal;
    double xdata[3];

    for (int i = 0; i < n; i++) {
      xdata[0] = x[3*i];
      xdata[1] = x[3*i+1];
      xdata[2] = x[3*i+2];
      imageint * img = image ? image + i : nullptr;
      tagint     tag = id    ? id[i]     : 0;

      // create atom only on MPI rank that would own it

      if (!domain->ownatom(tag, xdata, img, bexpand)) continue;

      atom->avec->create_atom(type[i],xdata);
      if (id) atom->tag[nlocal] = id[i];
      else atom->tag[nlocal] = 0;
      if (v) {
        atom->v[nlocal][0] = v[3*i];
        atom->v[nlocal][1] = v[3*i+1];
        atom->v[nlocal][2] = v[3*i+2];
      }
      if (image) atom->image[nlocal] = image[i];
      nlocal++;
    }

    // if no tags are given explicitly, create new and unique tags

    if (id == nullptr) atom->tag_extend();

    // reset box info, if extended when adding atoms.

    if (bexpand) domain->reset_box();

    // need to reset atom->natoms inside LAMMPS

    bigint ncurrent = nlocal;
    MPI_Allreduce(&ncurrent,&lmp->atom->natoms,1,MPI_LMP_BIGINT,
                  MPI_SUM,lmp->world);

    // init per-atom fix/compute/variable values for created atoms

    atom->data_fix_compute_variable(nlocal_prev,nlocal);

    // if global map exists, reset it
    // invoke map_init() b/c atom count has grown

    if (lmp->atom->map_style != Atom::MAP_NONE) {
      lmp->atom->map_init();
      lmp->atom->map_set();
    }
  }
  END_CAPTURE;
  return (int) lmp->atom->natoms - natoms_prev;
}

// ----------------------------------------------------------------------
// Library functions for accessing neighbor lists
// ----------------------------------------------------------------------

/** Find neighbor list index of pair style neighbor list
 *
 * Try finding pair instance that matches style. If exact is set, the pair must
 * match style exactly. If exact is 0, style must only be contained. If pair is
 * of style pair/hybrid, style is instead matched the nsub-th hybrid sub-style.
 *
 * Once the pair instance has been identified, multiple neighbor list requests
 * may be found. Every neighbor list is uniquely identified by its request
 * index. Thus, providing this request index ensures that the correct neighbor
 * list index is returned.
 *
 * \param  handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param  style    String used to search for pair style instance
 * \param  exact    Flag to control whether style should match exactly or only
 *                  must be contained in pair style name
 * \param  nsub     match nsub-th hybrid sub-style
 * \param  request  request index that specifies which neighbor list should be
 *                  returned, in case there are multiple neighbor lists requests
 *                  for the found pair style
 * \return          return neighbor list index if found, otherwise -1 */

int lammps_find_pair_neighlist(void* handle, char * style, int exact, int nsub, int request) {
  LAMMPS *  lmp = (LAMMPS *) handle;
  Pair* pair = lmp->force->pair_match(style, exact, nsub);

  if (pair != nullptr) {
    // find neigh list
    for (int i = 0; i < lmp->neighbor->nlist; i++) {
      NeighList * list = lmp->neighbor->lists[i];
      if (list->requestor_type != NeighList::PAIR || pair != list->requestor) continue;

      if (list->index == request) {
          return i;
      }
    }
  }
  return -1;
}

/* ---------------------------------------------------------------------- */

/** Find neighbor list index of fix neighbor list
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param id       Identifier of fix instance
 * \param request  request index that specifies which request should be returned,
 *                 in case there are multiple neighbor lists for this fix
 * \return         return neighbor list index if found, otherwise -1  */

int lammps_find_fix_neighlist(void* handle, char *id, int request) {
  LAMMPS *  lmp = (LAMMPS *) handle;
  Fix* fix = nullptr;
  const int nfix = lmp->modify->nfix;

  // find fix with name
  for (int ifix = 0; ifix < nfix; ifix++) {
    if (strcmp(lmp->modify->fix[ifix]->id, id) == 0) {
        fix = lmp->modify->fix[ifix];
        break;
    }
  }

  if (fix != nullptr) {
    // find neigh list
    for (int i = 0; i < lmp->neighbor->nlist; i++) {
      NeighList * list = lmp->neighbor->lists[i];
      if (list->requestor_type != NeighList::FIX || fix != list->requestor) continue;

      if (list->index == request) {
          return i;
      }
    }
  }
  return -1;
}

/* ---------------------------------------------------------------------- */

/** Find neighbor list index of compute neighbor list
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param id       Identifier of fix instance
 * \param request  request index that specifies which request should be returned,
 *                 in case there are multiple neighbor lists for this fix
 * \return         return neighbor list index if found, otherwise -1 */

int lammps_find_compute_neighlist(void* handle, char *id, int request) {
  LAMMPS *  lmp = (LAMMPS *) handle;
  Compute* compute = nullptr;
  const int ncompute = lmp->modify->ncompute;

  // find compute with name
  for (int icompute = 0; icompute < ncompute; icompute++) {
    if (strcmp(lmp->modify->compute[icompute]->id, id) == 0) {
        compute = lmp->modify->compute[icompute];
        break;
    }
  }

  if (compute != nullptr) {
    // find neigh list
    for (int i = 0; i < lmp->neighbor->nlist; i++) {
      NeighList * list = lmp->neighbor->lists[i];
      if (list->requestor_type != NeighList::COMPUTE || compute != list->requestor) continue;

      if (list->index == request) {
          return i;
      }
    }
  }
  return -1;
}

/* ---------------------------------------------------------------------- */

/** Return the number of entries in the neighbor list with given index
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param idx      neighbor list index
 * \return         return number of entries in neighbor list, -1 if idx is
 *                 not a valid index
 */
int lammps_neighlist_num_elements(void *handle, int idx) {
  LAMMPS *  lmp = (LAMMPS *) handle;
  Neighbor * neighbor = lmp->neighbor;

  if (idx < 0 || idx >= neighbor->nlist) {
    return -1;
  }

  NeighList * list = neighbor->lists[idx];
  return list->inum;
}

/* ---------------------------------------------------------------------- */

/** Return atom local index, number of neighbors, and array of neighbor local
 * atom indices of neighbor list entry
 *
 * \param handle          pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param idx             index of this neighbor list in the list of all neighbor lists
 * \param element         index of this neighbor list entry
 * \param[out] iatom      local atom index (i.e. in the range [0, nlocal + nghost), -1 if
                          invalid idx or element value
 * \param[out] numneigh   number of neighbors of atom iatom or 0
 * \param[out] neighbors  pointer to array of neighbor atom local indices or NULL */

void lammps_neighlist_element_neighbors(void *handle, int idx, int element, int *iatom, int *numneigh, int **neighbors) {
  LAMMPS *  lmp = (LAMMPS *) handle;
  Neighbor * neighbor = lmp->neighbor;
  *iatom = -1;
  *numneigh = 0;
  *neighbors = nullptr;

  if (idx < 0 || idx >= neighbor->nlist) {
    return;
  }

  NeighList * list = neighbor->lists[idx];

  if (element < 0 || element >= list->inum) {
    return;
  }

  int i = list->ilist[element];
  *iatom     = i;
  *numneigh  = list->numneigh[i];
  *neighbors = list->firstneigh[i];
}

// ----------------------------------------------------------------------
// Library functions for accessing LAMMPS configuration
// ----------------------------------------------------------------------

/** Get numerical representation of the LAMMPS version date.
 *
\verbatim embed:rst

The :cpp:func:`lammps_version` function returns an integer representing
the version of the LAMMPS code in the format YYYYMMDD.  This can be used
to implement backward compatibility in software using the LAMMPS library
interface.  The specific format guarantees, that this version number is
growing with every new LAMMPS release.

\endverbatim
 *
 * \param  handle  pointer to a previously created LAMMPS instance
 * \return         an integer representing the version data in the
 *                 format YYYYMMDD */

int lammps_version(void *handle)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  return lmp->num_ver;
}

/** Get operating system and architecture information
 *
\verbatim embed:rst

The :cpp:func:`lammps_get_os_info` function can be used to retrieve
detailed information about the hosting operating system and
compiler/runtime.
A suitable buffer for a C-style string has to be provided and its length.
If the assembled text will be truncated to not overflow this buffer.

.. versionadded:: 9Oct2020

\endverbatim
 *
 * \param  buffer    string buffer to copy the information to
 * \param  buf_size  size of the provided string buffer */

/* ---------------------------------------------------------------------- */

void lammps_get_os_info(char *buffer, int buf_size)
{
  if (buf_size <=0) return;
  buffer[0] = buffer[buf_size-1] = '\0';
  std::string txt = Info::get_os_info() + "\n";
  txt += Info::get_compiler_info();
  txt += " with " + Info::get_openmp_info() + "\n";
  strncpy(buffer, txt.c_str(), buf_size-1);
}

/* ---------------------------------------------------------------------- */

/** This function is used to query whether LAMMPS was compiled with
 *  a real MPI library or in serial. For the real MPI library it
 *  reports the size of the MPI communicator in bytes (4 or 8),
 *  which allows to check for compatibility with a hosting code.
 *
 * \return 0 when compiled with MPI STUBS, otherwise the MPI_Comm size in bytes */

int lammps_config_has_mpi_support()
{
#ifdef MPI_STUBS
  return 0;
#else
  return sizeof(MPI_Comm);
#endif
}

/* ---------------------------------------------------------------------- */

/** Check if the LAMMPS library supports compressed files via a pipe to gzip

\verbatim embed:rst
Several LAMMPS commands (e.g. :doc:`read_data`, :doc:`write_data`,
:doc:`dump styles atom, custom, and xyz <dump>`) support reading and
writing compressed files via creating a pipe to the ``gzip`` program.
This function checks whether this feature was :ref:`enabled at compile
time <gzip>`. It does **not** check whether the ``gzip`` itself is
installed and usable.
\endverbatim
 *
 * \return 1 if yes, otherwise 0
 */
int lammps_config_has_gzip_support() {
  return Info::has_gzip_support() ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Check if the LAMMPS library supports writing PNG format images

\verbatim embed:rst
The LAMMPS :doc:`dump style image <dump_image>` supports writing multiple
image file formats.  Most of them need, however, support from an external
library and using that has to be :ref:`enabled at compile time <graphics>`.
This function checks whether support for the `PNG image file format
<https://en.wikipedia.org/wiki/Portable_Network_Graphics>`_ is available
in the current LAMMPS library.
\endverbatim
 *
 * \return 1 if yes, otherwise 0
 */
int lammps_config_has_png_support() {
  return Info::has_png_support() ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Check if the LAMMPS library supports writing JPEG format images

\verbatim embed:rst
The LAMMPS :doc:`dump style image <dump_image>` supports writing multiple
image file formats.  Most of them need, however, support from an external
library and using that has to be :ref:`enabled at compile time <graphics>`.
This function checks whether support for the `JPEG image file format
<https://jpeg.org/jpeg/>`_ is available in the current LAMMPS library.
\endverbatim
 *
 * \return 1 if yes, otherwise 0
 */
int lammps_config_has_jpeg_support() {
  return Info::has_jpeg_support() ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Check if the LAMMPS library supports creating movie files via a pipe to ffmpeg

\verbatim embed:rst
The LAMMPS :doc:`dump style movie <dump_image>` supports generating movies
from images on-the-fly  via creating a pipe to the
`ffmpeg <https://ffmpeg.org/ffmpeg/>`_ program.
This function checks whether this feature was :ref:`enabled at compile time <graphics>`.
It does **not** check whether the ``ffmpeg`` itself is installed and usable.
\endverbatim
 *
 * \return 1 if yes, otherwise 0
 */
int lammps_config_has_ffmpeg_support() {
  return Info::has_ffmpeg_support() ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Check whether LAMMPS errors will throw a C++ exception
 *
\verbatim embed:rst
In case of errors LAMMPS will either abort or throw a C++ exception.
The latter has to be :ref:`enabled at compile time <exceptions>`.
This function checks if exceptions were enabled.

When using the library interface and C++ exceptions are enabled,
the library interface functions will "catch" them and the
error status can then be checked by calling
:cpp:func:`lammps_has_error` and the most recent error message
can be retrieved via :cpp:func:`lammps_get_last_error_message`.
This can allow to restart a calculation or delete and recreate
the LAMMPS instance when C++ exceptions are enabled.  One application
of using exceptions this way is the :ref:`lammps_shell`.  If C++
exceptions are disabled and an error happens during a call to
LAMMPS, the application will terminate.
\endverbatim
 * \return 1 if yes, otherwise 0
 */
int lammps_config_has_exceptions() {
  return Info::has_exceptions() ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Check if a specific package has been included in LAMMPS
 *
\verbatim embed:rst
This function checks if the LAMMPS library in use includes the
specific :doc:`LAMMPS package <Packages>` provided as argument.
\endverbatim
 *
 * \param name string with the name of the package
 * \return 1 if included, 0 if not.
 */
int lammps_config_has_package(const char *name) {
  return Info::has_package(name) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Count the number of installed packages in the LAMMPS library.
 *
\verbatim embed:rst
This function counts how many :doc:`LAMMPS packages <Packages>` are
included in the LAMMPS library in use.
\endverbatim
 *
 * \return number of packages included
 */
int lammps_config_package_count() {
  int i = 0;
  while (LAMMPS::installed_packages[i] != nullptr) {
    ++i;
  }
  return i;
}

/* ---------------------------------------------------------------------- */

/** Get the name of a package in the list of installed packages in the LAMMPS library.
 *
\verbatim embed:rst
This function copies the name of the package with the index *idx* into the
provided C-style string buffer.  The length of the buffer must be provided
as *buf_size* argument.  If the name of the package exceeds the length of the
buffer, it will be truncated accordingly.  If the index is out of range,
the function returns 0 and *buffer* is set to an empty string, otherwise 1;
\endverbatim
 *
 * \param idx index of the package in the list of included packages (0 <= idx < package count)
 * \param buffer string buffer to copy the name of the package to
 * \param buf_size size of the provided string buffer
 * \return 1 if successful, otherwise 0
 */
int lammps_config_package_name(int idx, char *buffer, int buf_size) {
  int maxidx = lammps_config_package_count();
  if ((idx < 0) || (idx >= maxidx)) {
      buffer[0] = '\0';
      return 0;
  }

  strncpy(buffer, LAMMPS::installed_packages[idx], buf_size);
  return 1;
}

/** Check for compile time settings in accelerator packages included in LAMMPS.
 *
\verbatim embed:rst
This function checks availability of compile time settings of included
:doc:`accelerator packages <Speed_packages>` in LAMMPS.
Supported packages names are "GPU", "KOKKOS", "USER-INTEL", and "USER-OMP".
Supported categories are "api" with possible settings "cuda", "hip", "phi",
"pthreads", "opencl", "openmp", and "serial", and "precision" with
possible settings "double", "mixed", and "single".  If the combination
of package, category, and setting is available, the function returns 1,
otherwise 0.
\endverbatim
 *
 * \param  package   string with the name of the accelerator package
 * \param  category  string with the category name of the setting
 * \param  setting   string with the name of the specific setting
 * \return 1 if available, 0 if not.
 */
int lammps_config_accelerator(const char *package,
                              const char *category,
                              const char *setting)
{
  return Info::has_accelerator_feature(package,category,setting) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Check if a specific style has been included in LAMMPS
 *
\verbatim embed:rst
This function checks if the LAMMPS library in use includes the
specific *style* of a specific *category* provided as an argument.
Valid categories are: *atom*\ , *integrate*\ , *minimize*\ ,
*pair*\ , *bond*\ , *angle*\ , *dihedral*\ , *improper*\ , *kspace*\ ,
*compute*\ , *fix*\ , *region*\ , *dump*\ , and *command*\ .
\endverbatim
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param  category  category of the style
 * \param  name      name of the style
 * \return           1 if included, 0 if not.
 */
int lammps_has_style(void *handle, const char *category, const char *name) {
  LAMMPS *lmp = (LAMMPS *) handle;
  Info info(lmp);
  return info.has_style(category, name) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */

/** Count the number of styles of category in the LAMMPS library.
 *
\verbatim embed:rst
This function counts how many styles in the provided *category*
are included in the LAMMPS library in use.
Please see :cpp:func:`lammps_has_style` for a list of valid
categories.
\endverbatim
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param category category of styles
 * \return number of styles in category
 */
int lammps_style_count(void *handle, const char *category) {
  LAMMPS *lmp = (LAMMPS *) handle;
  Info info(lmp);
  return info.get_available_styles(category).size();
}

/* ---------------------------------------------------------------------- */

/** Look up the name of a style by index in the list of style of a given category in the LAMMPS library.
 *
 *
 * This function copies the name of the *category* style with the index
 * *idx* into the provided C-style string buffer.  The length of the buffer
 * must be provided as *buf_size* argument.  If the name of the style
 * exceeds the length of the buffer, it will be truncated accordingly.
 * If the index is out of range, the function returns 0 and *buffer* is
 * set to an empty string, otherwise 1.
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param category category of styles
 * \param idx      index of the style in the list of *category* styles (0 <= idx < style count)
 * \param buffer   string buffer to copy the name of the style to
 * \param buf_size size of the provided string buffer
 * \return 1 if successful, otherwise 0
 */
int lammps_style_name(void *handle, const char *category, int idx,
                      char *buffer, int buf_size) {
  LAMMPS *lmp = (LAMMPS *) handle;
  Info info(lmp);
  auto styles = info.get_available_styles(category);

  if ((idx >=0) && (idx < (int) styles.size())) {
    strncpy(buffer, styles[idx].c_str(), buf_size);
    return 1;
  }

  buffer[0] = '\0';
  return 0;
}

/* ---------------------------------------------------------------------- */

/** Check if a specific ID exists in the current LAMMPS instance
 *
\verbatim embed:rst
This function checks if the current LAMMPS instance a *category* ID of
the given *name* exists.  Valid categories are: *compute*\ , *dump*\ ,
*fix*\ , *group*\ , *molecule*\ , *region*\ , and *variable*\ .

.. versionadded:: 9Oct2020

\endverbatim
 *
 * \param  handle    pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param  category  category of the id
 * \param  name      name of the id
 * \return           1 if included, 0 if not.
 */
int lammps_has_id(void *handle, const char *category, const char *name) {
  LAMMPS *lmp = (LAMMPS *) handle;

  if (strcmp(category,"compute") == 0) {
    int ncompute = lmp->modify->ncompute;
    Compute **compute = lmp->modify->compute;
    for (int i=0; i < ncompute; ++i) {
      if (strcmp(name,compute[i]->id) == 0) return 1;
    }
  } else if (strcmp(category,"dump") == 0) {
    int ndump = lmp->output->ndump;
    Dump **dump = lmp->output->dump;
    for (int i=0; i < ndump; ++i) {
      if (strcmp(name,dump[i]->id) == 0) return 1;
    }
  } else if (strcmp(category,"fix") == 0) {
    int nfix = lmp->modify->nfix;
    Fix **fix = lmp->modify->fix;
    for (int i=0; i < nfix; ++i) {
      if (strcmp(name,fix[i]->id) == 0) return 1;
    }
  } else if (strcmp(category,"group") == 0) {
    int ngroup = lmp->group->ngroup;
    char **groups = lmp->group->names;
    for (int i=0; i < ngroup; ++i) {
      if (strcmp(groups[i],name) == 0) return 1;
    }
  } else if (strcmp(category,"molecule") == 0) {
    int nmolecule = lmp->atom->nmolecule;
    Molecule **molecule = lmp->atom->molecules;
    for (int i=0; i < nmolecule; ++i) {
      if (strcmp(name,molecule[i]->id) == 0) return 1;
    }
  } else if (strcmp(category,"region") == 0) {
    int nregion = lmp->domain->nregion;
    Region **region = lmp->domain->regions;
    for (int i=0; i < nregion; ++i) {
      if (strcmp(name,region[i]->id) == 0) return 1;
    }
  } else if (strcmp(category,"variable") == 0) {
    int nvariable = lmp->input->variable->nvar;
    char **varnames = lmp->input->variable->names;
    for (int i=0; i < nvariable; ++i) {
      if (strcmp(name,varnames[i]) == 0) return 1;
    }
  }
  return 0;
}

/* ---------------------------------------------------------------------- */

/** Count the number of IDs of a category.
 *
\verbatim embed:rst
This function counts how many IDs in the provided *category*
are defined in the current LAMMPS instance.
Please see :cpp:func:`lammps_has_id` for a list of valid
categories.

.. versionadded:: 9Oct2020

\endverbatim
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param category category of IDs
 * \return number of IDs in category
 */
int lammps_id_count(void *handle, const char *category) {
  LAMMPS *lmp = (LAMMPS *) handle;
  if (strcmp(category,"compute") == 0) {
    return lmp->modify->ncompute;
  } else if (strcmp(category,"dump") == 0) {
    return lmp->output->ndump;
  } else if (strcmp(category,"fix") == 0) {
    return lmp->modify->nfix;
  } else if (strcmp(category,"group") == 0) {
    return lmp->group->ngroup;
  } else if (strcmp(category,"molecule") == 0) {
    return lmp->atom->nmolecule;
  } else if (strcmp(category,"region") == 0) {
    return lmp->domain->nregion;
  } else if (strcmp(category,"variable") == 0) {
    return lmp->input->variable->nvar;
  }
  return 0;
}

/* ---------------------------------------------------------------------- */

/** Look up the name of an ID by index in the list of IDs of a given category.
 *
\verbatim embed:rst
This function copies the name of the *category* ID with the index
*idx* into the provided C-style string buffer.  The length of the buffer
must be provided as *buf_size* argument.  If the name of the style
exceeds the length of the buffer, it will be truncated accordingly.
If the index is out of range, the function returns 0 and *buffer* is
set to an empty string, otherwise 1.

.. versionadded:: 9Oct2020

\endverbatim
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param category category of IDs
 * \param idx      index of the ID in the list of *category* styles (0 <= idx < count)
 * \param buffer   string buffer to copy the name of the style to
 * \param buf_size size of the provided string buffer
 * \return 1 if successful, otherwise 0
 */
int lammps_id_name(void *handle, const char *category, int idx,
                      char *buffer, int buf_size) {
  LAMMPS *lmp = (LAMMPS *) handle;

  if (strcmp(category,"compute") == 0) {
    if ((idx >=0) && (idx < lmp->modify->ncompute)) {
      strncpy(buffer, lmp->modify->compute[idx]->id, buf_size);
      return 1;
    }
  } else if (strcmp(category,"dump") == 0) {
    if ((idx >=0) && (idx < lmp->output->ndump)) {
      strncpy(buffer, lmp->output->dump[idx]->id, buf_size);
      return 1;
    }
  } else if (strcmp(category,"fix") == 0) {
    if ((idx >=0) && (idx < lmp->modify->nfix)) {
      strncpy(buffer, lmp->modify->fix[idx]->id, buf_size);
      return 1;
    }
  } else if (strcmp(category,"group") == 0) {
    if ((idx >=0) && (idx < lmp->group->ngroup)) {
      strncpy(buffer, lmp->group->names[idx], buf_size);
      return 1;
    }
  } else if (strcmp(category,"molecule") == 0) {
    if ((idx >=0) && (idx < lmp->atom->nmolecule)) {
      strncpy(buffer, lmp->atom->molecules[idx]->id, buf_size);
      return 1;
    }
  } else if (strcmp(category,"region") == 0) {
    if ((idx >=0) && (idx < lmp->domain->nregion)) {
      strncpy(buffer, lmp->domain->regions[idx]->id, buf_size);
      return 1;
    }
  } else if (strcmp(category,"variable") == 0) {
    if ((idx >=0) && (idx < lmp->input->variable->nvar)) {
      strncpy(buffer, lmp->input->variable->names[idx], buf_size);
      return 1;
    }
  }
  buffer[0] = '\0';
  return 0;
}

/* ---------------------------------------------------------------------- */

/** Count the number of loaded plugins
 *
\verbatim embed:rst
This function counts how many plugins are currently loaded.

.. versionadded:: 10Mar2021

\endverbatim
 *
 * \return number of loaded plugins
 */
int lammps_plugin_count()
{
#if defined(LMP_PLUGIN)
  return plugin_get_num_plugins();
#else
  return 0;
#endif
}

/* ---------------------------------------------------------------------- */

/** Look up the info of a loaded plugin by its index in the list of plugins
 *
\verbatim embed:rst
This function copies the name of the *style* plugin with the index
*idx* into the provided C-style string buffer.  The length of the buffer
must be provided as *buf_size* argument.  If the name of the style
exceeds the length of the buffer, it will be truncated accordingly.
If the index is out of range, the function returns 0 and *buffer* is
set to an empty string, otherwise 1.

.. versionadded:: 10Mar2021

\endverbatim
 *
 * \param  idx       index of the plugin in the list all or *style* plugins
 * \param  stylebuf  string buffer to copy the style of the plugin to
 * \param  namebuf   string buffer to copy the name of the plugin to
 * \param  buf_size  size of the provided string buffers
 * \return 1 if successful, otherwise 0
 */
int lammps_plugin_name(int idx, char *stylebuf, char *namebuf, int buf_size)
{
#if defined(LMP_PLUGIN)
  stylebuf[0] = namebuf[0] = '\0';

  const lammpsplugin_t *plugin = plugin_get_info(idx);
  if (plugin) {
    strncpy(stylebuf,plugin->style,buf_size);
    strncpy(namebuf,plugin->name,buf_size);
    return 1;
  }
#endif
  return 0;
}

// ----------------------------------------------------------------------
// utility functions
// ----------------------------------------------------------------------

/** Encode three integer image flags into a single imageint.
 *
\verbatim embed:rst

The prototype for this function when compiling with ``-DLAMMPS_BIGBIG``
is:

.. code-block:: c

   int64_t lammps_encode_image_flags(int ix, int iy, int iz);

This function performs the bit-shift, addition, and bit-wise OR
operations necessary to combine the values of three integers
representing the image flags in x-, y-, and z-direction.  Unless
LAMMPS is compiled with -DLAMMPS_BIGBIG, those integers are
limited 10-bit signed integers [-512, 511].  Otherwise the return
type changes from ``int`` to ``int64_t`` and the valid range for
the individual image flags becomes [-1048576,1048575],
i.e. that of a 21-bit signed integer.  There is no check on whether
the arguments conform to these requirements.

\endverbatim
 *
 * \param  ix  image flag value in x
 * \param  iy  image flag value in y
 * \param  iz  image flag value in z
 * \return     encoded image flag integer */

imageint lammps_encode_image_flags(int ix, int iy, int iz)
{
  imageint image = ((imageint) (ix + IMGMAX) & IMGMASK) |
    (((imageint) (iy + IMGMAX) & IMGMASK) << IMGBITS) |
    (((imageint) (iz + IMGMAX) & IMGMASK) << IMG2BITS);
  return image;
}

/* ---------------------------------------------------------------------- */

/** Decode a single image flag integer into three regular integers
 *
\verbatim embed:rst

The prototype for this function when compiling with ``-DLAMMPS_BIGBIG``
is:

.. code-block:: c

   void lammps_decode_image_flags(int64_t image, int *flags);

This function does the reverse operation of
:cpp:func:`lammps_encode_image_flags` and takes an image flag integer
does the bit-shift and bit-masking operations to decode it and stores
the resulting three regular integers into the buffer pointed to by
*flags*.

\endverbatim
 *
 * \param  image  encoded image flag integer
 * \param  flags  pointer to storage where the decoded image flags are stored. */

void lammps_decode_image_flags(imageint image, int *flags)
{
  flags[0] = (image & IMGMASK) - IMGMAX;
  flags[1] = (image >> IMGBITS & IMGMASK) - IMGMAX;
  flags[2] = (image >> IMG2BITS) - IMGMAX;
}

/* ----------------------------------------------------------------------
   find fix external with given ID and set the callback function
   and caller pointer
------------------------------------------------------------------------- */

void lammps_set_fix_external_callback(void *handle, char *id, FixExternalFnPtr callback_ptr, void * caller)
{
  LAMMPS *lmp = (LAMMPS *) handle;
  FixExternal::FnPtr callback = (FixExternal::FnPtr) callback_ptr;

  BEGIN_CAPTURE
  {
    int ifix = lmp->modify->find_fix(id);
    if (ifix < 0) {
      char str[128];
      snprintf(str, 128, "Can not find fix with ID '%s'!", id);
      lmp->error->all(FLERR,str);
    }

    Fix *fix = lmp->modify->fix[ifix];

    if (strcmp("external",fix->style) != 0) {
      char str[128];
      snprintf(str, 128, "Fix '%s' is not of style external!", id);
      lmp->error->all(FLERR,str);
    }

    FixExternal * fext = (FixExternal*) fix;
    fext->set_callback(callback, caller);
  }
  END_CAPTURE
}

/* set global energy contribution from fix external */
void lammps_fix_external_set_energy_global(void *handle, char *id,
                                           double energy)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
    int ifix = lmp->modify->find_fix(id);
    if (ifix < 0)
      lmp->error->all(FLERR,fmt::format("Can not find fix with ID '{}'!", id));

    Fix *fix = lmp->modify->fix[ifix];

    if (strcmp("external",fix->style) != 0)
      lmp->error->all(FLERR,fmt::format("Fix '{}' is not of style external!", id));

    FixExternal * fext = (FixExternal*) fix;
    fext->set_energy_global(energy);
  }
  END_CAPTURE
}

/* set global virial contribution from fix external */
void lammps_fix_external_set_virial_global(void *handle, char *id,
                                           double *virial)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
    int ifix = lmp->modify->find_fix(id);
    if (ifix < 0)
      lmp->error->all(FLERR,fmt::format("Can not find fix with ID '{}'!", id));

    Fix *fix = lmp->modify->fix[ifix];

    if (strcmp("external",fix->style) != 0)
      lmp->error->all(FLERR,fmt::format("Fix '{}' is not of style external!", id));

    FixExternal * fext = (FixExternal*) fix;
    fext->set_virial_global(virial);
  }
  END_CAPTURE
}

/* ---------------------------------------------------------------------- */

/** Free memory buffer allocated by LAMMPS.
 *
\verbatim embed:rst

Some of the LAMMPS C library interface functions return data as pointer
to a buffer that has been allocated by LAMMPS or the library interface.
This function can be used to delete those in order to avoid memory
leaks.

\endverbatim
 *
 * \param  ptr  pointer to data allocated by LAMMPS */

void lammps_free(void *ptr)
{
  free(ptr);
}

/* ---------------------------------------------------------------------- */

/** Check if LAMMPS is currently inside a run or minimization
 *
 * This function can be used from signal handlers or multi-threaded
 * applications to determine if the LAMMPS instance is currently active.
 *
 * \param  handle pointer to a previously created LAMMPS instance cast to ``void *``.
 * \return        0 if idle or >0 if active */

int lammps_is_running(void *handle)
{
  LAMMPS *  lmp = (LAMMPS *) handle;
  return lmp->update->whichflag;
}

/** Force a timeout to cleanly stop an ongoing run
 *
 * This function can be used from signal handlers or multi-threaded
 * applications to cleanly terminate an ongoing run.
 *
 * \param  handle pointer to a previously created LAMMPS instance cast to ``void *`` */

void lammps_force_timeout(void *handle)
{
  LAMMPS *  lmp = (LAMMPS *) handle;
  return lmp->timer->force_timeout();
}

// ----------------------------------------------------------------------
// Library functions for error handling with exceptions enabled
// ----------------------------------------------------------------------

/** Check if there is a (new) error message available

\verbatim embed:rst
This function can be used to query if an error inside of LAMMPS
has thrown a :ref:`C++ exception <exceptions>`.

.. note::

   This function will always report "no error" when the LAMMPS library
   has been compiled without ``-DLAMMPS_EXCEPTIONS`` which turns fatal
   errors aborting LAMMPS into a C++ exceptions. You can use the library
   function :cpp:func:`lammps_config_has_exceptions` to check if this is
   the case.
\endverbatim
 *
 * \param handle   pointer to a previously created LAMMPS instance cast to ``void *``.
 * \return 0 on no error, 1 on error.
 */
int lammps_has_error(void *handle) {
#ifdef LAMMPS_EXCEPTIONS
  LAMMPS *  lmp = (LAMMPS *) handle;
  Error * error = lmp->error;
  return (error->get_last_error().empty()) ? 0 : 1;
#else
  return 0;
#endif
}

/* ---------------------------------------------------------------------- */

/** Copy the last error message into the provided buffer

\verbatim embed:rst
This function can be used to retrieve the error message that was set
in the event of an error inside of LAMMPS which resulted in a
:ref:`C++ exception <exceptions>`.  A suitable buffer for a C-style
string has to be provided and its length.  If the internally stored
error message is longer, it will be truncated accordingly.  The return
value of the function corresponds to the kind of error: a "1" indicates
an error that occurred on all MPI ranks and is often recoverable, while
a "2" indicates an abort that would happen only in a single MPI rank
and thus may not be recoverable as other MPI ranks may be waiting on
the failing MPI ranks to send messages.

.. note::

   This function will do nothing when the LAMMPS library has been
   compiled without ``-DLAMMPS_EXCEPTIONS`` which turns errors aborting
   LAMMPS into a C++ exceptions.  You can use the library function
   :cpp:func:`lammps_config_has_exceptions` to check if this is the case.
\endverbatim
 *
 * \param  handle    pointer to a previously created LAMMPS instance cast to ``void *``.
 * \param  buffer    string buffer to copy the error message to
 * \param  buf_size  size of the provided string buffer
 * \return           1 when all ranks had the error, 2 on a single rank error. */

int lammps_get_last_error_message(void *handle, char *buffer, int buf_size) {
#ifdef LAMMPS_EXCEPTIONS
  LAMMPS *lmp = (LAMMPS *) handle;
  Error *error = lmp->error;
  buffer[0] = buffer[buf_size-1] = '\0';

  if (!error->get_last_error().empty()) {
    int error_type = error->get_last_error_type();
    strncpy(buffer, error->get_last_error().c_str(), buf_size-1);
    error->set_last_error("", ERROR_NONE);
    return error_type;
  }
#endif
  return 0;
}

// Local Variables:
// fill-column: 72
// End:
