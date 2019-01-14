#!/usr/bin/env python

# Install.py tool to build the CSlib library
# used to automate the steps described in the README file in this dir

from __future__ import print_function
import sys,os,re,subprocess
sys.path.append('..')
from install_helpers import get_cpus,fullpath
from argparse import ArgumentParser

parser = ArgumentParser(prog='Install.py',
                        description="LAMMPS library build wrapper script")

# help message

help = """
Syntax from src dir: make lib-message args="-m"
                 or: make lib-message args="-s -z"
Syntax from lib dir: python Install.py -m
                 or: python Install.py -s -z 

Example:

make lib-message args="-m -z"   # build parallel CSlib with ZMQ support
make lib-message args="-s"   # build serial CSlib with no ZMQ support
"""

pgroup = parser.add_mutually_exclusive_group()
pgroup.add_argument("-m", "--mpi", action="store_true",
                    help="parallel build of CSlib with MPI")
pgroup.add_argument("-s", "--serial", action="store_true",
                    help="serial build of CSlib")
parser.add_argument("-z", "--zmq", default=False, action="store_true",
                    help="build CSlib with ZMQ socket support, default ()")

args = parser.parse_args()

# print help message and exit, if neither build nor path options are given
if args.mpi == False and args.serial == False:
  parser.print_help()
  sys.exit(help)

mpiflag = args.mpi
serialflag = args.serial
zmqflag = args.zmq

# build CSlib
# copy resulting lib to cslib/src/libmessage.a
# copy appropriate Makefile.lammps.* to Makefile.lammps

print("Building CSlib ...")
srcdir = fullpath("./cslib/src")

if mpiflag and zmqflag:
  cmd = "cd %s; make lib_parallel" % srcdir
elif mpiflag and not zmqflag:
  cmd = "cd %s; make lib_parallel zmq=no" % srcdir
elif not mpiflag and zmqflag:
  cmd = "cd %s; make lib_serial" % srcdir
elif not mpiflag and not zmqflag:
  cmd = "cd %s; make lib_serial zmq=no" % srcdir
  
print(cmd)
try:
  txt = subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
  print(txt.decode('UTF-8'))
except subprocess.CalledProcessError as e:
    print("Make failed with:\n %s" % e.output.decode('UTF-8'))
    sys.exit(1)

if mpiflag: cmd = "cd %s; cp libcsmpi.a libmessage.a" % srcdir
else: cmd = "cd %s; cp libcsnompi.a libmessage.a" % srcdir
print(cmd)
txt = subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
print(txt.decode('UTF-8'))

if zmqflag: cmd = "cp Makefile.lammps.zmq Makefile.lammps"
else: cmd = "cp Makefile.lammps.nozmq Makefile.lammps"
print(cmd)
txt = subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
print(txt.decode('UTF-8'))
