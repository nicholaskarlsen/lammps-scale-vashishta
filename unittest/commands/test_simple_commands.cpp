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

#include "fmt/format.h"
#include "info.h"
#include "input.h"
#include "lammps.h"
#include "update.h"
#include "utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mpi.h>

// whether to print verbose output (i.e. not capturing LAMMPS screen output).
bool verbose = false;

using LAMMPS_NS::utils::split_words;

namespace LAMMPS_NS {
using ::testing::MatchesRegex;
using ::testing::StrEq;

#define TEST_FAILURE(...)                \
    if (Info::has_exceptions()) {        \
        ASSERT_ANY_THROW({__VA_ARGS__}); \
    } else {                             \
        ASSERT_DEATH({__VA_ARGS__}, ""); \
    }

class SimpleCommandsTest : public ::testing::Test {
protected:
    LAMMPS *lmp;

    void SetUp() override
    {
        const char *args[] = {"SimpleCommandsTest", "-log", "none", "-echo", "screen", "-nocite"};
        char **argv        = (char **)args;
        int argc           = sizeof(args) / sizeof(char *);
        if (!verbose) ::testing::internal::CaptureStdout();
        lmp = new LAMMPS(argc, argv, MPI_COMM_WORLD);
        if (!verbose) ::testing::internal::GetCapturedStdout();
    }

    void TearDown() override
    {
        if (!verbose) ::testing::internal::CaptureStdout();
        delete lmp;
        if (!verbose) ::testing::internal::GetCapturedStdout();
    }
};

TEST_F(SimpleCommandsTest, Echo)
{
    ASSERT_EQ(lmp->input->echo_screen, 1);
    ASSERT_EQ(lmp->input->echo_log, 0);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("echo none");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_EQ(lmp->input->echo_screen, 0);
    ASSERT_EQ(lmp->input->echo_log, 0);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("echo both");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_EQ(lmp->input->echo_screen, 1);
    ASSERT_EQ(lmp->input->echo_log, 1);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("echo screen");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_EQ(lmp->input->echo_screen, 1);
    ASSERT_EQ(lmp->input->echo_log, 0);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("echo log");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_EQ(lmp->input->echo_screen, 0);
    ASSERT_EQ(lmp->input->echo_log, 1);

    ::testing::internal::CaptureStdout();
    TEST_FAILURE(lmp->input->one("echo"););
    auto mesg = ::testing::internal::GetCapturedStdout();
    ASSERT_THAT(mesg, MatchesRegex("^ERROR: Illegal echo command.*"));

    ::testing::internal::CaptureStdout();
    TEST_FAILURE(lmp->input->one("echo xxx"););
    mesg = ::testing::internal::GetCapturedStdout();
    ASSERT_THAT(mesg, MatchesRegex("^ERROR: Illegal echo command.*"));
}

TEST_F(SimpleCommandsTest, Log)
{
    ASSERT_EQ(lmp->logfile, nullptr);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("log simple_command_test.log");
    lmp->input->one("print 'test1'");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_NE(lmp->logfile, nullptr);

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("log none");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_EQ(lmp->logfile, nullptr);

    std::string text;
    std::ifstream in;
    in.open("simple_command_test.log");
    in >> text;
    in.close();
    ASSERT_THAT(text, StrEq("test1"));

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("log simple_command_test.log append");
    lmp->input->one("print 'test2'");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_NE(lmp->logfile, nullptr);
    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("log none");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_EQ(lmp->logfile, nullptr);

    in.open("simple_command_test.log");
    in >> text;
    ASSERT_THAT(text, StrEq("test1"));
    in >> text;
    ASSERT_THAT(text, StrEq("test2"));
    in.close();
    remove("simple_command_test.log");
}

TEST_F(SimpleCommandsTest, Units)
{
    const char *names[] = {"lj", "real", "metal", "si", "cgs", "electron", "micro", "nano"};
    const double dt[]   = {0.005, 1.0, 0.001, 1.0e-8, 1.0e-8, 0.001, 2.0, 0.00045};
    std::size_t num     = sizeof(names) / sizeof(const char *);
    ASSERT_EQ(num, sizeof(dt) / sizeof(double));

    ASSERT_THAT(lmp->update->unit_style, StrEq("lj"));
    for (int i = 0; i < num; ++i) {
        if (!verbose) ::testing::internal::CaptureStdout();
        lmp->input->one(fmt::format("units {}", names[i]));
        if (!verbose) ::testing::internal::GetCapturedStdout();
        ASSERT_THAT(lmp->update->unit_style, StrEq(names[i]));
        ASSERT_EQ(lmp->update->dt, dt[i]);
    }

    if (!verbose) ::testing::internal::CaptureStdout();
    lmp->input->one("clear");
    if (!verbose) ::testing::internal::GetCapturedStdout();
    ASSERT_THAT(lmp->update->unit_style, StrEq("lj"));

    ::testing::internal::CaptureStdout();
    TEST_FAILURE(lmp->input->one("units unknown"););
    auto mesg = ::testing::internal::GetCapturedStdout();
    ASSERT_THAT(mesg, MatchesRegex(".*ERROR: Illegal units command.*"));
}
} // namespace LAMMPS_NS

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    ::testing::InitGoogleMock(&argc, argv);

    // handle arguments passed via environment variable
    if (const char *var = getenv("TEST_ARGS")) {
        std::vector<std::string> env = split_words(var);
        for (auto arg : env) {
            if (arg == "-v") {
                verbose = true;
            }
        }
    }

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = true;

    int rv = RUN_ALL_TESTS();
    MPI_Finalize();
    return rv;
}
