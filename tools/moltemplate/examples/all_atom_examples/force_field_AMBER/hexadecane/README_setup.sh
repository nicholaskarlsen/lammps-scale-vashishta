# -------- REQUIREMENTS: ---------
#  You must define your MOLTEMPLATE_PATH environment variable
#  and set it to the "common" subdirectory of your moltemplate distribution.
#  (See the "Installation" section in the moltemplate manual.)

# Create LAMMPS input files this way:

cd moltemplate_files

  # run moltemplate

  moltemplate.sh system.lt

  # This will generate various files with names ending in *.in* and *.data. 
  # These files are the input files directly read by LAMMPS.  Move them to 
  # the parent directory (or wherever you plan to run the simulation).
  mv -f system.data system.in* ../

  # Optional:
  # The "./output_ttree/" directory is full of temporary files generated by 
  # moltemplate.  They can be useful for debugging, but are usually thrown away
  #rm -rf output_ttree/

cd ../





# Optional:
# Note: The system.data and system.in.settings files contain extra information
# for atoms defined in GAFF which you are not using in this simulation.  This
# is harmless, but if you to delete this information from your 
# system.in.settings and system.in.data files, follow the instructions in
# this script: "optional_cleanup/README_remove_irrelevant_info.sh"

