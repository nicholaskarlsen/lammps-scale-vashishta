# Test of SW potential for Si system

units		metal
boundary	p p p

atom_style	atomic

read_data	data_sw

pair_style	quip
pair_coeff	* * sw_example.xml "IP SW" 14

neighbor	0.3 bin
neigh_modify	delay 10

fix		1 all nve
thermo		10
timestep	0.001

dump		1 all custom 10 dump.sw id fx fy fz
dump_modify     1 format "%d %20.15g  %20.15g %20.15g"

run		1
