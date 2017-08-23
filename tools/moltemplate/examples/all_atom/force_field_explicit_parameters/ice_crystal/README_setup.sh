# Use these commands to generate the LAMMPS input script and data file
# (and other auxilliary files):


# Create LAMMPS input files this way:
cd moltemplate_files

  # run moltemplate

  moltemplate.sh -atomstyle full system.lt

  # This will generate various files with names ending in *.in* and *.data.
  # These files are the input files directly read by LAMMPS.  Move them to
  # the parent directory (or wherever you plan to run the simulation).

  mv -f system.in* system.data ../

  # Optional:
  # The "./output_ttree/" directory is full of temporary files generated by
  # moltemplate.  They can be useful for debugging, but are usually thrown away.
  rm -rf output_ttree/

cd ../
