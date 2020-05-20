/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

// unit tests for bond styles intended for molecular systems

#include "yaml_reader.h"
#include "yaml_writer.h"
#include "error_stats.h"
#include "test_config.h"
#include "test_config_reader.h"
#include "test_main.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "lammps.h"
#include "atom.h"
#include "modify.h"
#include "compute.h"
#include "force.h"
#include "bond.h"
#include "info.h"
#include "input.h"
#include "universe.h"

#include <mpi.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

using ::testing::StartsWith;
using ::testing::HasSubstr;


void cleanup_lammps(LAMMPS_NS::LAMMPS *lmp, const TestConfig &cfg)
{
    std::string name;

    name = cfg.basename + ".restart";
    remove(name.c_str());
    name = cfg.basename + ".data";
    remove(name.c_str());
    name = cfg.basename + "-coeffs.in";
    remove(name.c_str());
    delete lmp;
}

LAMMPS_NS::LAMMPS *init_lammps(int argc, char **argv,
                               const TestConfig &cfg,
                               const bool newton=true)
{
    LAMMPS_NS::LAMMPS *lmp;

    lmp = new LAMMPS_NS::LAMMPS(argc, argv, MPI_COMM_WORLD);

    // check if prerequisite styles are available
    LAMMPS_NS::Info *info = new LAMMPS_NS::Info(lmp);
    int nfail = 0;
    for (auto prerequisite : cfg.prerequisites) {
        std::string style = prerequisite.second;

        // this is a test for bond styles, so if the suffixed
        // version is not available, there is no reason to test.
        if (prerequisite.first == "bond") {
            if (lmp->suffix_enable) {
                style += "/";
                style += lmp->suffix;
            }
        }

        if (!info->has_style(prerequisite.first,style)) ++nfail;
    }
    if (nfail > 0) {
        delete info;
        cleanup_lammps(lmp,cfg);
        return nullptr;
    }

    if (newton) {
        lmp->input->one("variable newton_bond index on");
    } else {
        lmp->input->one("variable newton_bond index off");
    }

    std::string set_input_dir = "variable input_dir index ";
    set_input_dir += INPUT_FOLDER;
    lmp->input->one(set_input_dir.c_str());
    for (auto pre_command : cfg.pre_commands)
        lmp->input->one(pre_command.c_str());

    std::string input_file = INPUT_FOLDER + "/" + cfg.input_file;

    lmp->input->file(input_file.c_str());

    std::string cmd("bond_style ");
    cmd += cfg.bond_style;
    lmp->input->one(cmd.c_str());
    for (auto bond_coeff : cfg.bond_coeff) {
        cmd = "bond_coeff " + bond_coeff;
        lmp->input->one(cmd.c_str());
    }
    for (auto post_command : cfg.post_commands)
        lmp->input->one(post_command.c_str());
    lmp->input->one("run 0 post no");
    cmd = "write_restart " + cfg.basename + ".restart";
    lmp->input->one(cmd.c_str());
    cmd = "write_data " + cfg.basename + ".data";
    lmp->input->one(cmd.c_str());
    cmd = "write_coeff " + cfg.basename + "-coeffs.in";
    lmp->input->one(cmd.c_str());

    return lmp;
}

void run_lammps(LAMMPS_NS::LAMMPS *lmp)
{
    lmp->input->one("fix 1 all nve");
    lmp->input->one("compute pe all pe/atom");
    lmp->input->one("compute sum all reduce sum c_pe");
    lmp->input->one("thermo_style custom step temp pe press c_sum");
    lmp->input->one("thermo 2");
    lmp->input->one("run 4 post no");
}

void restart_lammps(LAMMPS_NS::LAMMPS *lmp, const TestConfig &cfg)
{
    lmp->input->one("clear");
    std::string cmd("read_restart ");
    cmd += cfg.basename + ".restart";
    lmp->input->one(cmd.c_str());

    if (!lmp->force->bond) {
        cmd = "bond_style " + cfg.bond_style;
        lmp->input->one(cmd.c_str());
    }
    if ((cfg.bond_style.substr(0,6) == "hybrid")
        || !lmp->force->bond->writedata) {
        for (auto bond_coeff : cfg.bond_coeff) {
            cmd = "bond_coeff " + bond_coeff;
            lmp->input->one(cmd.c_str());
        }
    }
    for (auto post_command : cfg.post_commands)
        lmp->input->one(post_command.c_str());
    lmp->input->one("run 0 post no");
}

void data_lammps(LAMMPS_NS::LAMMPS *lmp, const TestConfig &cfg)
{
    lmp->input->one("clear");
    lmp->input->one("variable bond_style delete");
    lmp->input->one("variable data_file  delete");
    lmp->input->one("variable newton_bond delete");
    lmp->input->one("variable newton_bond index on");

    for (auto pre_command : cfg.pre_commands)
        lmp->input->one(pre_command.c_str());

    std::string cmd("variable bond_style index '");
    cmd += cfg.bond_style + "'";
    lmp->input->one(cmd.c_str());

    cmd = "variable data_file index ";
    cmd += cfg.basename + ".data";
    lmp->input->one(cmd.c_str());

    std::string input_file = INPUT_FOLDER + PATH_SEP + cfg.input_file;
    lmp->input->file(input_file.c_str());

    for (auto bond_coeff : cfg.bond_coeff) {
        cmd = "bond_coeff " + bond_coeff;
        lmp->input->one(cmd.c_str());
    }
    for (auto post_command : cfg.post_commands)
        lmp->input->one(post_command.c_str());
    lmp->input->one("run 0 post no");
}

// re-generate yaml file with current settings.

void generate_yaml_file(const char *outfile, const TestConfig &config)
{
    // initialize system geometry
    const char *args[] = {"BondStyle", "-log", "none", "-echo", "screen", "-nocite" };
    char **argv = (char **)args;
    int argc = sizeof(args)/sizeof(char *);
    LAMMPS_NS::LAMMPS *lmp = init_lammps(argc,argv,config);
    if (!lmp) {
        std::cerr << "One or more prerequisite styles are not available "
            "in this LAMMPS configuration:\n";
        for (auto prerequisite : config.prerequisites) {
            std::cerr << prerequisite.first << "_style "
                      << prerequisite.second << "\n";
        }
        return;
    }

    const int natoms = lmp->atom->natoms;
    const int bufsize = 256;
    char buf[bufsize];
    std::string block("");

    YamlWriter writer(outfile);

    // lammps_version
    writer.emit("lammps_version", lmp->universe->version);

    // date_generated
    std::time_t now = time(NULL);
    block = ctime(&now);
    block = block.substr(0,block.find("\n")-1);
    writer.emit("date_generated", block);

    // epsilon
    writer.emit("epsilon", config.epsilon);

    // prerequisites
    block.clear();
    for (auto prerequisite :  config.prerequisites) {
        block += prerequisite.first + " " + prerequisite.second + "\n";
    }
    writer.emit_block("prerequisites", block);

    // pre_commands
    block.clear();
    for (auto command :  config.pre_commands) {
        block += command + "\n";
    }
    writer.emit_block("pre_commands", block);

    // post_commands
    block.clear();
    for (auto command : config.post_commands) {
        block += command + "\n";
    }
    writer.emit_block("post_commands", block);

    // input_file
    writer.emit("input_file", config.input_file);

    // bond_style
    writer.emit("bond_style", config.bond_style);

    // bond_coeff
    block.clear();
    for (auto bond_coeff : config.bond_coeff) {
        block += bond_coeff + "\n";
    }
    writer.emit_block("bond_coeff", block);

    // extract
    block.clear();
    std::stringstream outstr;
    for (auto data : config.extract) {
        outstr << data.first << " " << data.second << std::endl;
    }
    writer.emit_block("extract", outstr.str());

    // natoms
    writer.emit("natoms", natoms);

    // init_energy
    writer.emit("init_energy", lmp->force->bond->energy);

    // init_stress
    double *stress = lmp->force->bond->virial;
    snprintf(buf,bufsize,"% 23.16e % 23.16e % 23.16e % 23.16e % 23.16e % 23.16e",
             stress[0],stress[1],stress[2],stress[3],stress[4],stress[5]);
    writer.emit_block("init_stress", buf);

    // init_forces
    block.clear();
    double **f = lmp->atom->f;
    LAMMPS_NS::tagint *tag = lmp->atom->tag;
    for (int i=0; i < natoms; ++i) {
        snprintf(buf,bufsize,"% 3d % 23.16e % 23.16e % 23.16e\n",
                 (int)tag[i], f[i][0], f[i][1], f[i][2]);
        block += buf;
    }
    writer.emit_block("init_forces", block);

    // do a few steps of MD
    run_lammps(lmp);

    // run_energy
    writer.emit("run_energy", lmp->force->bond->energy);

    // run_stress
    stress = lmp->force->bond->virial;
    snprintf(buf,bufsize,"% 23.16e % 23.16e % 23.16e % 23.16e % 23.16e % 23.16e",
             stress[0],stress[1],stress[2],stress[3],stress[4],stress[5]);
    writer.emit_block("run_stress", buf);

    block.clear();
    f = lmp->atom->f;
    tag = lmp->atom->tag;
    for (int i=0; i < natoms; ++i) {
        snprintf(buf,bufsize,"% 3d % 23.16e % 23.16e % 23.16e\n",
                 (int)tag[i], f[i][0], f[i][1], f[i][2]);
        block += buf;
    }
    writer.emit_block("run_forces", block);

    cleanup_lammps(lmp,config);
    return;
}

TEST(BondStyle, plain) {
    const char *args[] = {"BondStyle", "-log", "none", "-echo", "screen", "-nocite" };
    char **argv = (char **)args;
    int argc = sizeof(args)/sizeof(char *);

    ::testing::internal::CaptureStdout();
    LAMMPS_NS::LAMMPS *lmp = init_lammps(argc,argv,test_config,true);
    std::string output = ::testing::internal::GetCapturedStdout();

    if (!lmp) {
        std::cerr << "One or more prerequisite styles are not available "
            "in this LAMMPS configuration:\n";
        for (auto prerequisite : test_config.prerequisites) {
            std::cerr << prerequisite.first << "_style "
                      << prerequisite.second << "\n";
        }
        GTEST_SKIP();
    }

    EXPECT_THAT(output, StartsWith("LAMMPS ("));
    EXPECT_THAT(output, HasSubstr("Loop time"));

    // abort if running in parallel and not all atoms are local
    const int nlocal = lmp->atom->nlocal;
    ASSERT_EQ(lmp->atom->natoms,nlocal);

    double epsilon = test_config.epsilon;
    double **f=lmp->atom->f;
    LAMMPS_NS::tagint *tag=lmp->atom->tag;
    ErrorStats stats;
    stats.reset();
    const std::vector<coord_t> &f_ref = test_config.init_forces;
    ASSERT_EQ(nlocal+1,f_ref.size());
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_ref[tag[i]].x, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_ref[tag[i]].y, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_ref[tag[i]].z, epsilon);
    }
    if (print_stats)
        std::cerr << "init_forces stats, newton on: " << stats << std::endl;

    LAMMPS_NS::Bond *bond = lmp->force->bond;
    double *stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.init_stress.xx, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.init_stress.yy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.init_stress.zz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.init_stress.xy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.init_stress.xz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.init_stress.yz, epsilon);
    if (print_stats)
        std::cerr << "init_stress stats, newton on: " << stats << std::endl;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.init_energy, epsilon);
    if (print_stats)
        std::cerr << "init_energy stats, newton on: " << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    run_lammps(lmp);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f = lmp->atom->f;
    stress = bond->virial;
    const std::vector<coord_t> &f_run = test_config.run_forces;
    ASSERT_EQ(nlocal+1,f_run.size());
    stats.reset();
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_run[tag[i]].x, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_run[tag[i]].y, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_run[tag[i]].z, 10*epsilon);
    }
    if (print_stats)
        std::cerr << "run_forces  stats, newton on: " << stats << std::endl;

    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.run_stress.xx, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.run_stress.yy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.run_stress.zz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.run_stress.xy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.run_stress.xz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.run_stress.yz, epsilon);
    if (print_stats)
        std::cerr << "run_stress  stats, newton on: " << stats << std::endl;

    stats.reset();
    int id = lmp->modify->find_compute("sum");
    double energy = lmp->modify->compute[id]->compute_scalar();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.run_energy, epsilon);
    EXPECT_FP_LE_WITH_EPS(bond->energy, energy, epsilon);
    if (print_stats)
        std::cerr << "run_energy  stats, newton on: " << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    cleanup_lammps(lmp,test_config);
    lmp = init_lammps(argc,argv,test_config,false);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f=lmp->atom->f;
    tag=lmp->atom->tag;
    stats.reset();
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_ref[tag[i]].x, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_ref[tag[i]].y, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_ref[tag[i]].z, epsilon);
    }
    if (print_stats)
        std::cerr << "init_forces stats, newton off:" << stats << std::endl;

    bond = lmp->force->bond;
    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.init_stress.xx, 2*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.init_stress.yy, 2*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.init_stress.zz, 2*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.init_stress.xy, 2*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.init_stress.xz, 2*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.init_stress.yz, 2*epsilon);
    if (print_stats)
        std::cerr << "init_stress stats, newton off:" << stats << std::endl;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.init_energy, epsilon);
    if (print_stats)
        std::cerr << "init_energy stats, newton off:" << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    run_lammps(lmp);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f = lmp->atom->f;
    stress = bond->virial;
    stats.reset();
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_run[tag[i]].x, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_run[tag[i]].y, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_run[tag[i]].z, 10*epsilon);
    }
    if (print_stats)
        std::cerr << "run_forces  stats, newton off:" << stats << std::endl;

    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.run_stress.xx, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.run_stress.yy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.run_stress.zz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.run_stress.xy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.run_stress.xz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.run_stress.yz, epsilon);
    if (print_stats)
        std::cerr << "run_stress  stats, newton off:" << stats << std::endl;

    stats.reset();
    id = lmp->modify->find_compute("sum");
    energy = lmp->modify->compute[id]->compute_scalar();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.run_energy, epsilon);
    EXPECT_FP_LE_WITH_EPS(bond->energy, energy, epsilon);
    if (print_stats)
        std::cerr << "run_energy  stats, newton off:" << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    restart_lammps(lmp, test_config);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f=lmp->atom->f;
    tag=lmp->atom->tag;
    stats.reset();
    ASSERT_EQ(nlocal+1,f_ref.size());
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_ref[tag[i]].x, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_ref[tag[i]].y, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_ref[tag[i]].z, epsilon);
    }
    if (print_stats)
        std::cerr << "restart_forces stats:" << stats << std::endl;

    bond = lmp->force->bond;
    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.init_stress.xx, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.init_stress.yy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.init_stress.zz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.init_stress.xy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.init_stress.xz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.init_stress.yz, epsilon);
    if (print_stats)
        std::cerr << "restart_stress stats:" << stats << std::endl;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.init_energy, epsilon);
    if (print_stats)
        std::cerr << "restart_energy stats:" << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    data_lammps(lmp, test_config);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f=lmp->atom->f;
    tag=lmp->atom->tag;
    stats.reset();
    ASSERT_EQ(nlocal+1,f_ref.size());
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_ref[tag[i]].x, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_ref[tag[i]].y, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_ref[tag[i]].z, epsilon);
    }
    if (print_stats)
        std::cerr << "data_forces stats:" << stats << std::endl;

    bond = lmp->force->bond;
    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.init_stress.xx, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.init_stress.yy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.init_stress.zz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.init_stress.xy, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.init_stress.xz, epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.init_stress.yz, epsilon);
    if (print_stats)
        std::cerr << "data_stress stats:" << stats << std::endl;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.init_energy, epsilon);
    if (print_stats)
        std::cerr << "data_energy stats:" << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    cleanup_lammps(lmp,test_config);
    if (!verbose) ::testing::internal::GetCapturedStdout();
};

TEST(BondStyle, omp) {
    if (!LAMMPS_NS::LAMMPS::is_installed_pkg("USER-OMP")) GTEST_SKIP();
    const char *args[] = {"BondStyle", "-log", "none", "-echo", "screen",
                          "-nocite", "-pk", "omp", "4", "-sf", "omp"};
    char **argv = (char **)args;
    int argc = sizeof(args)/sizeof(char *);

    ::testing::internal::CaptureStdout();
    LAMMPS_NS::LAMMPS *lmp = init_lammps(argc,argv,test_config,true);
    std::string output = ::testing::internal::GetCapturedStdout();

    if (!lmp) {
        std::cerr << "One or more prerequisite styles with /omp suffix\n"
            "are not available in this LAMMPS configuration:\n";
        for (auto prerequisite : test_config.prerequisites) {
            std::cerr << prerequisite.first << "_style "
                      << prerequisite.second << "\n";
        }
        GTEST_SKIP();
    }

    EXPECT_THAT(output, StartsWith("LAMMPS ("));
    EXPECT_THAT(output, HasSubstr("Loop time"));

    // abort if running in parallel and not all atoms are local
    const int nlocal = lmp->atom->nlocal;
    ASSERT_EQ(lmp->atom->natoms,nlocal);

    // relax error a bit for USER-OMP package
    double epsilon = 5.0*test_config.epsilon;
    double **f=lmp->atom->f;
    LAMMPS_NS::tagint *tag=lmp->atom->tag;
    const std::vector<coord_t> &f_ref = test_config.init_forces;
    ErrorStats stats;
    stats.reset();
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_ref[tag[i]].x, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_ref[tag[i]].y, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_ref[tag[i]].z, epsilon);
    }
    if (print_stats)
        std::cerr << "init_forces stats, newton on: " << stats << std::endl;

    LAMMPS_NS::Bond *bond = lmp->force->bond;
    double *stress = bond->virial;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.init_stress.xx, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.init_stress.yy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.init_stress.zz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.init_stress.xy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.init_stress.xz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.init_stress.yz, 10*epsilon);
    if (print_stats)
        std::cerr << "init_stress stats, newton on: " << stats << std::endl;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.init_energy, epsilon);
    if (print_stats)
        std::cerr << "init_energy stats, newton on: " << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    run_lammps(lmp);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f = lmp->atom->f;
    stress = bond->virial;
    const std::vector<coord_t> &f_run = test_config.run_forces;
    ASSERT_EQ(nlocal+1,f_run.size());
    stats.reset();
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_run[tag[i]].x, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_run[tag[i]].y, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_run[tag[i]].z, 10*epsilon);
    }
    if (print_stats)
        std::cerr << "run_forces  stats, newton on: " << stats << std::endl;

    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.run_stress.xx, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.run_stress.yy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.run_stress.zz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.run_stress.xy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.run_stress.xz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.run_stress.yz, 10*epsilon);
    if (print_stats)
        std::cerr << "run_stress  stats, newton on: " << stats << std::endl;

    stats.reset();
    int id = lmp->modify->find_compute("sum");
    double energy = lmp->modify->compute[id]->compute_scalar();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.run_energy, epsilon);
    // TODO: this is currently broken for USER-OMP with bond style hybrid
    // needs to be fixed in the main code somewhere. Not sure where, though.
    if (test_config.bond_style.substr(0,6) != "hybrid")
        EXPECT_FP_LE_WITH_EPS(bond->energy, energy, epsilon);
    if (print_stats)
        std::cerr << "run_energy  stats, newton on: " << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    cleanup_lammps(lmp,test_config);
    lmp = init_lammps(argc,argv,test_config,false);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f=lmp->atom->f;
    tag=lmp->atom->tag;
    stats.reset();
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_ref[tag[i]].x, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_ref[tag[i]].y, epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_ref[tag[i]].z, epsilon);
    }
    if (print_stats)
        std::cerr << "init_forces stats, newton off:" << stats << std::endl;

    bond = lmp->force->bond;
    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.init_stress.xx, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.init_stress.yy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.init_stress.zz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.init_stress.xy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.init_stress.xz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.init_stress.yz, 10*epsilon);
    if (print_stats)
        std::cerr << "init_stress stats, newton off:" << stats << std::endl;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.init_energy, epsilon);
    if (print_stats)
        std::cerr << "init_energy stats, newton off:" << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    run_lammps(lmp);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f = lmp->atom->f;
    stats.reset();
    for (int i=0; i < nlocal; ++i) {
        EXPECT_FP_LE_WITH_EPS(f[i][0], f_run[tag[i]].x, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][1], f_run[tag[i]].y, 10*epsilon);
        EXPECT_FP_LE_WITH_EPS(f[i][2], f_run[tag[i]].z, 10*epsilon);
    }
    if (print_stats)
        std::cerr << "run_forces  stats, newton off:" << stats << std::endl;

    stress = bond->virial;
    stats.reset();
    EXPECT_FP_LE_WITH_EPS(stress[0], test_config.run_stress.xx, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[1], test_config.run_stress.yy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[2], test_config.run_stress.zz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[3], test_config.run_stress.xy, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[4], test_config.run_stress.xz, 10*epsilon);
    EXPECT_FP_LE_WITH_EPS(stress[5], test_config.run_stress.yz, 10*epsilon);
    if (print_stats)
        std::cerr << "run_stress  stats, newton off:" << stats << std::endl;

    stats.reset();
    id = lmp->modify->find_compute("sum");
    energy = lmp->modify->compute[id]->compute_scalar();
    EXPECT_FP_LE_WITH_EPS(bond->energy, test_config.run_energy, epsilon);
    // TODO: this is currently broken for USER-OMP with bond style hybrid
    // needs to be fixed in the main code somewhere. Not sure where, though.
    if (test_config.bond_style.substr(0,6) != "hybrid")
        EXPECT_FP_LE_WITH_EPS(bond->energy, energy, epsilon);
    if (print_stats)
        std::cerr << "run_energy  stats, newton off:" << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    cleanup_lammps(lmp,test_config);
    if (!verbose) ::testing::internal::GetCapturedStdout();
};

TEST(BondStyle, single) {
    const char *args[] = {"BondStyle", "-log", "none", "-echo", "screen", "-nocite" };
    char **argv = (char **)args;
    int argc = sizeof(args)/sizeof(char *);

    // create a LAMMPS instance with standard settings to detect the number of atom types
    if (!verbose) ::testing::internal::CaptureStdout();
    LAMMPS_NS::LAMMPS *lmp = init_lammps(argc,argv,test_config);
    if (!verbose) ::testing::internal::GetCapturedStdout();
    if (!lmp) {
        std::cerr << "One or more prerequisite styles are not available "
            "in this LAMMPS configuration:\n";
        for (auto prerequisite : test_config.prerequisites) {
            std::cerr << prerequisite.first << "_style "
                      << prerequisite.second << "\n";
        }
        GTEST_SKIP();
    }

    // gather some information and skip if unsupported
    int ntypes = lmp->atom->ntypes;
    int nbondtypes = lmp->atom->nbondtypes;
    int molecular = lmp->atom->molecular;
    if (molecular != 1) {
        std::cerr << "Only simple molecular atom styles are supported\n";
        if (!verbose) ::testing::internal::CaptureStdout();
        cleanup_lammps(lmp,test_config);
        if (!verbose) ::testing::internal::GetCapturedStdout();
        GTEST_SKIP();
    }

    LAMMPS_NS::Bond *bond = lmp->force->bond;

    // now start over
    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("clear");
    lmp->input->one("variable newton_bond delete");
    lmp->input->one("variable newton_bond index on");

    std::string set_input_dir = "variable input_dir index ";
    set_input_dir += INPUT_FOLDER;
    lmp->input->one(set_input_dir.c_str());
    for (auto pre_command : test_config.pre_commands)
        lmp->input->one(pre_command.c_str());

    lmp->input->one("atom_style molecular");
    lmp->input->one("units ${units}");
    lmp->input->one("boundary p p p");
    lmp->input->one("newton ${newton_pair} ${newton_bond}");
    lmp->input->one("special_bonds lj/coul "
                    "${bond_factor} ${angle_factor} ${dihedral_factor}");

    lmp->input->one("atom_modify map array");
    lmp->input->one("region box block -10.0 10.0 -10.0 10.0 -10.0 10.0 units box");
    char buf[10];
    snprintf(buf,10,"%d",ntypes);
    std::string cmd("create_box 1 box");
    cmd += " bond/types ";
    snprintf(buf,10,"%d",nbondtypes);
    cmd += buf;
    cmd += " extra/bond/per/atom 2";
    cmd += " extra/special/per/atom 2";
    lmp->input->one(cmd.c_str());

    lmp->input->one("pair_style zero 8.0");
    lmp->input->one("pair_coeff * *");

    cmd = "bond_style ";
    cmd += test_config.bond_style;
    lmp->input->one(cmd.c_str());
    bond = lmp->force->bond;

    for (auto bond_coeff : test_config.bond_coeff) {
        cmd = "bond_coeff " + bond_coeff;
        lmp->input->one(cmd.c_str());
    }

    // create (only) four atoms and two bonds
    lmp->input->one("mass * 1.0");
    lmp->input->one("create_atoms 1 single  5.0 -0.75  0.4 units box");
    lmp->input->one("create_atoms 1 single  5.5  0.25 -0.1 units box");
    lmp->input->one("create_atoms 1 single -5.0  0.75  0.4 units box");
    lmp->input->one("create_atoms 1 single -5.5 -0.25 -0.1 units box");
    lmp->input->one("create_bonds single/bond 1 1 2");
    lmp->input->one("create_bonds single/bond 2 3 4");
    for (auto post_command : test_config.post_commands)
        lmp->input->one(post_command.c_str());
    lmp->input->one("run 0 post no");
    if (!verbose) ::testing::internal::GetCapturedStdout();

    int idx1 = lmp->atom->map(1);
    int idx2 = lmp->atom->map(2);
    int idx3 = lmp->atom->map(3);
    int idx4 = lmp->atom->map(4);
    double epsilon = test_config.epsilon;
    double **f=lmp->atom->f;
    double **x=lmp->atom->x;
    double delx1 = x[idx2][0] - x[idx1][0];
    double dely1 = x[idx2][1] - x[idx1][1];
    double delz1 = x[idx2][2] - x[idx1][2];
    double rsq1 = delx1*delx1+dely1*dely1+delz1*delz1;
    double delx2 = x[idx4][0] - x[idx3][0];
    double dely2 = x[idx4][1] - x[idx3][1];
    double delz2 = x[idx4][2] - x[idx3][2];
    double rsq2 = delx2*delx2+dely2*dely2+delz2*delz2;
    double fsingle = 0.0;
    double ebond[4], esngl[4];
    ErrorStats stats;

    ebond[0] = bond->energy;
    esngl[0] = bond->single(1, rsq1, idx1, idx2, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx1][0],-fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][1],-fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][2],-fsingle*delz1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][0], fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][1], fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][2], fsingle*delz1, epsilon);

    esngl[0] += bond->single(2, rsq2, idx3, idx4, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx3][0],-fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][1],-fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][2],-fsingle*delz2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][0], fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][1], fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][2], fsingle*delz2, epsilon);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("displace_atoms all random 0.5 0.5 0.5 23456");
    lmp->input->one("run 0 post no");
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f = lmp->atom->f;
    x = lmp->atom->x;
    idx1 = lmp->atom->map(1);
    idx2 = lmp->atom->map(2);
    idx3 = lmp->atom->map(3);
    idx4 = lmp->atom->map(4);
    delx1 = x[idx2][0] - x[idx1][0];
    dely1 = x[idx2][1] - x[idx1][1];
    delz1 = x[idx2][2] - x[idx1][2];
    rsq1 = delx1*delx1+dely1*dely1+delz1*delz1;
    delx2 = x[idx4][0] - x[idx3][0];
    dely2 = x[idx4][1] - x[idx3][1];
    delz2 = x[idx4][2] - x[idx3][2];
    rsq2 = delx2*delx2+dely2*dely2+delz2*delz2;
    fsingle = 0.0;

    ebond[1] = bond->energy;
    esngl[1] = bond->single(1, rsq1, idx1, idx2, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx1][0],-fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][1],-fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][2],-fsingle*delz1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][0], fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][1], fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][2], fsingle*delz1, epsilon);

    esngl[1] += bond->single(2, rsq2, idx3, idx4, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx3][0],-fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][1],-fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][2],-fsingle*delz2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][0], fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][1], fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][2], fsingle*delz2, epsilon);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("displace_atoms all random 0.5 0.5 0.5 456963");
    lmp->input->one("run 0 post no");
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f = lmp->atom->f;
    x = lmp->atom->x;
    idx1 = lmp->atom->map(1);
    idx2 = lmp->atom->map(2);
    idx3 = lmp->atom->map(3);
    idx4 = lmp->atom->map(4);
    delx1 = x[idx2][0] - x[idx1][0];
    dely1 = x[idx2][1] - x[idx1][1];
    delz1 = x[idx2][2] - x[idx1][2];
    rsq1 = delx1*delx1+dely1*dely1+delz1*delz1;
    delx2 = x[idx4][0] - x[idx3][0];
    dely2 = x[idx4][1] - x[idx3][1];
    delz2 = x[idx4][2] - x[idx3][2];
    rsq2 = delx2*delx2+dely2*dely2+delz2*delz2;
    fsingle = 0.0;

    ebond[2] = bond->energy;
    esngl[2] = bond->single(1, rsq1, idx1, idx2, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx1][0],-fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][1],-fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][2],-fsingle*delz1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][0], fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][1], fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][2], fsingle*delz1, epsilon);

    esngl[2] += bond->single(2, rsq2, idx3, idx4, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx3][0],-fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][1],-fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][2],-fsingle*delz2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][0], fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][1], fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][2], fsingle*delz2, epsilon);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("displace_atoms all random 0.5 0.5 0.5 9726532");
    lmp->input->one("run 0 post no");
    if (!verbose) ::testing::internal::GetCapturedStdout();

    f = lmp->atom->f;
    x = lmp->atom->x;
    idx1 = lmp->atom->map(1);
    idx2 = lmp->atom->map(2);
    idx3 = lmp->atom->map(3);
    idx4 = lmp->atom->map(4);
    delx1 = x[idx2][0] - x[idx1][0];
    dely1 = x[idx2][1] - x[idx1][1];
    delz1 = x[idx2][2] - x[idx1][2];
    rsq1 = delx1*delx1+dely1*dely1+delz1*delz1;
    delx2 = x[idx4][0] - x[idx3][0];
    dely2 = x[idx4][1] - x[idx3][1];
    delz2 = x[idx4][2] - x[idx3][2];
    rsq2 = delx2*delx2+dely2*dely2+delz2*delz2;
    fsingle = 0.0;

    ebond[3] = bond->energy;
    esngl[3] = bond->single(1, rsq1, idx1, idx2, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx1][0],-fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][1],-fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx1][2],-fsingle*delz1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][0], fsingle*delx1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][1], fsingle*dely1, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx2][2], fsingle*delz1, epsilon);

    esngl[3] += bond->single(2, rsq2, idx3, idx4, fsingle);
    EXPECT_FP_LE_WITH_EPS(f[idx3][0],-fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][1],-fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx3][2],-fsingle*delz2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][0], fsingle*delx2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][1], fsingle*dely2, epsilon);
    EXPECT_FP_LE_WITH_EPS(f[idx4][2], fsingle*delz2, epsilon);
    if (print_stats)
        std::cerr << "single_force  stats:" << stats << std::endl;

    stats.reset();
    EXPECT_FP_LE_WITH_EPS(ebond[0], esngl[0], epsilon);
    EXPECT_FP_LE_WITH_EPS(ebond[1], esngl[1], epsilon);
    EXPECT_FP_LE_WITH_EPS(ebond[2], esngl[2], epsilon);
    EXPECT_FP_LE_WITH_EPS(ebond[3], esngl[3], epsilon);
    if (print_stats)
        std::cerr << "single_energy  stats:" << stats << std::endl;

    if (!verbose) ::testing::internal::CaptureStdout();
    cleanup_lammps(lmp,test_config);
    if (!verbose) ::testing::internal::GetCapturedStdout();
}

TEST(BondStyle, extract) {
    const char *args[] = {"BondStyle", "-log", "none", "-echo", "screen", "-nocite" };
    char **argv = (char **)args;
    int argc = sizeof(args)/sizeof(char *);

    if (!verbose) ::testing::internal::CaptureStdout();
    LAMMPS_NS::LAMMPS *lmp = init_lammps(argc,argv,test_config,true);
    if (!verbose) ::testing::internal::GetCapturedStdout();

    if (!lmp) {
        std::cerr << "One or more prerequisite styles are not available "
            "in this LAMMPS configuration:\n";
        for (auto prerequisite : test_config.prerequisites) {
            std::cerr << prerequisite.first << "_style "
                      << prerequisite.second << "\n";
        }
        GTEST_SKIP();
    }
    LAMMPS_NS::Bond *bond = lmp->force->bond;
    void *ptr = nullptr;
    int dim = 0;
    for (auto extract : test_config.extract) {
        ptr = bond->extract(extract.first.c_str(),dim);
        EXPECT_NE(ptr, nullptr);
        EXPECT_EQ(dim, extract.second);
    }
    ptr = bond->extract("does_not_exist",dim);
    EXPECT_EQ(ptr, nullptr);

    for (int i=1; i <= lmp->atom->nbondtypes; ++i)
        EXPECT_GE(bond->equilibrium_distance(i), 0.0);

    if (!verbose) ::testing::internal::CaptureStdout();
    cleanup_lammps(lmp,test_config);
    if (!verbose) ::testing::internal::GetCapturedStdout();
}
