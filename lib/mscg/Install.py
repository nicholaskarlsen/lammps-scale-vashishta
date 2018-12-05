#!/usr/bin/env python

# Install.py tool to download, unpack, build, and link to the MS-CG library
# used to automate the steps described in the README file in this dir

from __future__ import print_function
import sys,os,re,subprocess,shutil
sys.path.append('..')
from install_helpers import get_cpus,fullpath,get_cpus,geturl

from argparse import ArgumentParser

parser = ArgumentParser(prog='Install.py',
                        description="LAMMPS library build wrapper script")

# settings

version = "1.7.3.1"
machine = "g++_simple"

# help message

help = """
Syntax from src dir: make lib-mscg args="-p [path] -m [suffix] -v [version]"
                 or: make lib-mscg args="-b -m [suffix]"
Syntax from lib dir: python Install.py -p [path]  -m [suffix] -v [version]
Syntax from lib dir: python Install.py -b -m [suffix]

Example:

make lib-mscg args="-b -m serial " # download/build in lib/mscg/MSCG-release with settings compatible with "make serial"
make lib-mscg args="-b -m mpi " # download/build in lib/mscg/MSCG-release with settings compatible with "make mpi"
make lib-mscg args="-p /usr/local/mscg-release " # use existing MS-CG installation in /usr/local/mscg-release
"""

# known checksums for different MSCG versions. used to validate the download.
checksums = { \
        '1.7.3.1' : '8c45e269ee13f60b303edd7823866a91', \
        }

# parse and process arguments

pgroup = parser.add_mutually_exclusive_group()
pgroup.add_argument("-b", "--build", action="store_true",
                    help="download and build the MSCG library")
pgroup.add_argument("-p", "--path",
                    help="specify folder of existing MSCG installation")
parser.add_argument("-v", "--version", default=version, choices=checksums.keys(),
                    help="set version of MSCG to download and build (default: %s)" % version)
parser.add_argument("-m", "--machine", default=machine, choices=['g++_simple','intel_simple','lapack', 'mac'],
                    help="set machine suffix specifies which src/Make/Makefile.suffix to use. (default: %s)" % machine)

args = parser.parse_args()

# print help message and exit, if neither build nor path options are given
if args.build == False and not args.path:
  parser.print_help()
  sys.exit(help)

buildflag = args.build
pathflag = args.path != None
mscgpath= args.path
msuffix = args.machine
mscgver = args.version

# settings

url = "https://github.com/uchicago-voth/MSCG-release/archive/%s.tar.gz" % mscgver
tarfile = "MS-CG-%s.tar.gz" % mscgver
tardir = "MSCG-release-%s" % mscgver

homepath = fullpath('.')
homedir = "%s/%s" % (homepath,tardir)

if (pathflag):
    if not os.path.isdir(mscgpath): error("MS-CG path does not exist")
    homedir = mscgpath

# download and unpack MS-CG tarfile

if buildflag:
  print("Downloading MS-CG ...")
  geturl(url,"%s/%s" % (homepath,tarfile))

  print("Unpacking MS-CG tarfile ...")
  if os.path.exists("%s/%s" % (homepath,tardir)):
    shutil.rmtree("%s/%s" % (homepath,tardir))
  cmd = 'cd "%s"; tar -xzvf %s' % (homepath,tarfile)
  subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
  os.remove("%s/%s" % (homepath,tarfile))
  if os.path.basename(homedir) != tardir:
    if os.path.exists(homedir):
      shutil.rmtree(homedir)
    os.rename("%s/%s" % (homepath,tardir),homedir)

# build MS-CG

if buildflag:
  print("Building MS-CG ...")
  if os.path.exists("%s/src/Make/Makefile.%s" % (homedir,msuffix)):
    cmd = 'cd "%s/src"; cp Make/Makefile.%s .; make -f Makefile.%s' % \
        (homedir,msuffix,msuffix)
  elif os.path.exists("Makefile.%s" % msuffix):
    cmd = 'cd "%s/src"; cp ../../Makefile.%s .; make -f Makefile.%s' % \
        (homedir,msuffix,msuffix)
  else:
    error("Cannot find Makefile.%s" % msuffix)
  try:
    txt = subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
    print(txt.decode('UTF-8'))
  except subprocess.CalledProcessError as e:
    print("Make failed with:\n %s" % e.output.decode('UTF-8'))
    sys.exit(1)

  if not os.path.exists("Makefile.lammps"):
    print("Creating Makefile.lammps")
    if os.path.exists("Makefile.lammps.%s" % msuffix):
      cmd = 'cp Makefile.lammps.%s Makefile.lammps' % msuffix
    else:
      cmd = 'cp Makefile.lammps.default Makefile.lammps'
    subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
  else: print("Makefile.lammps exists. Please check its settings")

# create 2 links in lib/mscg to MS-CG src dir

print("Creating links to MS-CG include and lib files")
if os.path.isfile("includelink") or os.path.islink("includelink"):
  os.remove("includelink")
if os.path.isfile("liblink") or os.path.islink("liblink"):
  os.remove("liblink")
cmd = 'ln -s "%s/src" includelink' % homedir
subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
cmd = 'ln -s "%s/src" liblink' % homedir
subprocess.check_output(cmd,stderr=subprocess.STDOUT,shell=True)
