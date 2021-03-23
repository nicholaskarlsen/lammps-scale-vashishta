/*----------------------------------------------------------------------
  PuReMD - Purdue ReaxFF Molecular Dynamics Program

  Copyright (2010) Purdue University
  Hasan Metin Aktulga, hmaktulga@lbl.gov
  Joseph Fogarty, jcfogart@mail.usf.edu
  Sagar Pandit, pandit@usf.edu
  Ananth Y Grama, ayg@cs.purdue.edu

  Please cite the related publication:
  H. M. Aktulga, J. C. Fogarty, S. A. Pandit, A. Y. Grama,
  "Parallel Reactive Molecular Dynamics: Numerical Methods and
  Algorithmic Techniques", Parallel Computing, in press.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details:
  <https://www.gnu.org/licenses/>.
  ----------------------------------------------------------------------*/

#include "reaxc_tool_box.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "reaxc_defs.h"

#include "error.h"

int Tokenize( char* s, char*** tok )
{
  char test[MAX_LINE];
  const char *sep = (const char *)"\t \n\r\f!=";
  char *word;
  int count=0;

  strncpy( test, s, MAX_LINE-1);

  for (word = strtok(test, sep); word; word = strtok(nullptr, sep)) {
    strncpy( (*tok)[count], word, MAX_LINE );
    count++;
  }

  return count;
}



/* safe malloc */
void *smalloc( LAMMPS_NS::Error *error_ptr, rc_bigint n, const char *name )
{
  void *ptr;
  char errmsg[256];

  if (n <= 0) {
    snprintf(errmsg, 256, "Trying to allocate %ld bytes for array %s. "
              "returning NULL.", n, name);
    if (error_ptr) error_ptr->one(FLERR,errmsg);
    else fputs(errmsg,stderr);

    return nullptr;
  }

  ptr = malloc( n );
  if (ptr == nullptr) {
    snprintf(errmsg, 256, "Failed to allocate %ld bytes for array %s", n, name);
    if (error_ptr) error_ptr->one(FLERR,errmsg);
    else fputs(errmsg,stderr);
  }

  return ptr;
}


/* safe calloc */
void *scalloc( LAMMPS_NS::Error *error_ptr, rc_bigint n, rc_bigint size, const char *name )
{
  void *ptr;
  char errmsg[256];

  if (n <= 0) {
    snprintf(errmsg, 256, "Trying to allocate %ld elements for array %s. "
            "returning NULL.\n", n, name );
    if (error_ptr) error_ptr->one(FLERR,errmsg);
    else fputs(errmsg,stderr);
    return nullptr;
  }

  if (size <= 0) {
    snprintf(errmsg, 256, "Elements size for array %s is %ld. "
             "returning NULL", name, size );
    if (error_ptr) error_ptr->one(FLERR,errmsg);
    else fputs(errmsg,stderr);
    return nullptr;
  }

  ptr = calloc( n, size );
  if (ptr == nullptr) {
    char errmsg[256];
    snprintf(errmsg, 256, "Failed to allocate %ld bytes for array %s", n*size, name);
    if (error_ptr) error_ptr->one(FLERR,errmsg);
    else fputs(errmsg,stderr);
  }

  return ptr;
}


/* safe free */
void sfree( LAMMPS_NS::Error* error_ptr, void *ptr, const char *name )
{
  if (ptr == nullptr) {
    char errmsg[256];
    snprintf(errmsg, 256, "Trying to free the already NULL pointer %s", name );
    if (error_ptr) error_ptr->one(FLERR,errmsg);
    else fputs(errmsg,stderr);
    return;
  }

  free( ptr );
  ptr = nullptr;
}

