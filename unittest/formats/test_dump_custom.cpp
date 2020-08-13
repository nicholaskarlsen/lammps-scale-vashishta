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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fmt/format.h"
#include "utils.h"
#include "../testing/core.h"
#include "../testing/systems/melt.h"
#include "../testing/utils.h"

char * BINARY2TXT_BINARY = nullptr;

class DumpCustomTest : public MeltTest {
};

TEST_F(DumpCustomTest, run1)
{
    auto dump_file = "dump_custom_run1.melt";
    auto fields = "id type proc procp1 mass x y z ix iy iz xs ys zs xu yu zu xsu ysu zsu vx vy vz fx fy fz";

    if (!verbose) ::testing::internal::CaptureStdout();
    command(fmt::format("dump id all custom 1 {} {}", dump_file, fields));
    command("dump_modify id units yes");
    command("run 1");
    if (!verbose) ::testing::internal::GetCapturedStdout();


    ASSERT_FILE_EXISTS(dump_file);
    auto lines = read_lines(dump_file);
    ASSERT_EQ(lines.size(), 84);
    ASSERT_STREQ(lines[6].c_str(), "ITEM: BOX BOUNDS pp pp pp");
    ASSERT_EQ(utils::split_words(lines[7]).size(), 2);
    ASSERT_STREQ(lines[10].c_str(), fmt::format("ITEM: ATOMS {}", fields).c_str());
    ASSERT_EQ(utils::split_words(lines[11]).size(), 26);
    delete_file(dump_file);
}

TEST_F(DumpCustomTest, thresh_run0)
{
    auto dump_file = "dump_custom_thresh_run0.melt";
    auto fields = "id type x y z";

    if (!verbose) ::testing::internal::CaptureStdout();
    command(fmt::format("dump id all custom 1 {} {}", dump_file, fields));
    command("dump_modify id units yes thresh x < 1 thresh y < 1 thresh z < 1");
    command("run 0");
    if (!verbose) ::testing::internal::GetCapturedStdout();


    ASSERT_FILE_EXISTS(dump_file);
    auto lines = read_lines(dump_file);
    ASSERT_EQ(lines.size(), 15);
    ASSERT_STREQ(lines[6].c_str(), "ITEM: BOX BOUNDS pp pp pp");
    ASSERT_EQ(utils::split_words(lines[7]).size(), 2);
    ASSERT_STREQ(lines[10].c_str(), fmt::format("ITEM: ATOMS {}", fields).c_str());
    ASSERT_EQ(utils::split_words(lines[11]).size(), 5);
    delete_file(dump_file);
}

TEST_F(DumpCustomTest, compute_run0)
{
    auto dump_file = "dump_custom_compute_run0.melt";
    auto fields = "id type x y z c_comp[1] c_comp[2] c_comp[3]";

    if (!verbose) ::testing::internal::CaptureStdout();
    command("compute comp all property/atom x y z");
    command(fmt::format("dump id all custom 1 {} {}", dump_file, fields));
    command("dump_modify id units yes");
    command("run 0");
    if (!verbose) ::testing::internal::GetCapturedStdout();


    ASSERT_FILE_EXISTS(dump_file);
    auto lines = read_lines(dump_file);
    ASSERT_EQ(lines.size(), 43);
    ASSERT_STREQ(lines[6].c_str(), "ITEM: BOX BOUNDS pp pp pp");
    ASSERT_EQ(utils::split_words(lines[7]).size(), 2);
    ASSERT_STREQ(lines[10].c_str(), fmt::format("ITEM: ATOMS {}", fields).c_str());
    ASSERT_EQ(utils::split_words(lines[11]).size(), 8);
    delete_file(dump_file);
}

TEST_F(DumpCustomTest, fix_run0)
{
    auto dump_file = "dump_custom_compute_run0.melt";
    auto fields = "id x y z f_numdiff[1] f_numdiff[2] f_numdiff[3]";

    if (!verbose) ::testing::internal::CaptureStdout();
    command("fix numdiff all numdiff 1 0.0001");
    command(fmt::format("dump id all custom 1 {} {}", dump_file, fields));
    command("dump_modify id units yes");
    command("run 0");
    if (!verbose) ::testing::internal::GetCapturedStdout();


    ASSERT_FILE_EXISTS(dump_file);
    auto lines = read_lines(dump_file);
    ASSERT_EQ(lines.size(), 43);
    ASSERT_STREQ(lines[6].c_str(), "ITEM: BOX BOUNDS pp pp pp");
    ASSERT_EQ(utils::split_words(lines[7]).size(), 2);
    ASSERT_STREQ(lines[10].c_str(), fmt::format("ITEM: ATOMS {}", fields).c_str());
    ASSERT_EQ(utils::split_words(lines[11]).size(), 7);
    delete_file(dump_file);
}

TEST_F(DumpCustomTest, custom_run0)
{
    auto dump_file = "dump_custom_custom_run0.melt";
    auto fields = "id x y z i_flag1 d_flag2";

    if (!verbose) ::testing::internal::CaptureStdout();
    command("fix prop all property/atom i_flag1 d_flag2");
    command("compute 1 all property/atom i_flag1 d_flag2");
    command(fmt::format("dump id all custom 1 {} {}", dump_file, fields));
    command("dump_modify id units yes");
    command("run 0");
    if (!verbose) ::testing::internal::GetCapturedStdout();


    ASSERT_FILE_EXISTS(dump_file);
    auto lines = read_lines(dump_file);
    ASSERT_EQ(lines.size(), 43);
    ASSERT_STREQ(lines[6].c_str(), "ITEM: BOX BOUNDS pp pp pp");
    ASSERT_EQ(utils::split_words(lines[7]).size(), 2);
    ASSERT_STREQ(lines[10].c_str(), fmt::format("ITEM: ATOMS {}", fields).c_str());
    ASSERT_EQ(utils::split_words(lines[11]).size(), 6);
    delete_file(dump_file);
}

TEST_F(DumpCustomTest, binary_run1)
{
    if(!BINARY2TXT_BINARY) GTEST_SKIP();

    auto text_file = "dump_custom_text_run1.melt";
    auto binary_file = "dump_custom_binary_run1.melt.bin";
    auto converted_file = fmt::format("{}.txt", binary_file);
    auto fields = "id type proc x y z ix iy iz xs ys zs xu yu zu xsu ysu zsu vx vy vz fx fy fz";

    if (!verbose) ::testing::internal::CaptureStdout();
    command(fmt::format("dump id0 all custom 1 {} {}", text_file, fields));
    command(fmt::format("dump id1 all custom 1 {} {}", binary_file, fields));
    command("dump_modify id0 units yes");
    command("dump_modify id1 units yes");
    command("run 1");
    if (!verbose) ::testing::internal::GetCapturedStdout();

    ASSERT_FILE_EXISTS(text_file);
    ASSERT_FILE_EXISTS(binary_file);

    if (!verbose) ::testing::internal::CaptureStdout();
    std::string cmdline = fmt::format("{} {}", BINARY2TXT_BINARY, binary_file);
    system(cmdline.c_str());
    if (!verbose) ::testing::internal::GetCapturedStdout();

    ASSERT_FILE_EXISTS(converted_file);
    ASSERT_FILE_EQUAL(text_file, converted_file);
    delete_file(text_file);
    delete_file(binary_file);
    delete_file(converted_file);
}

TEST_F(DumpCustomTest, triclinic_run1)
{
    auto dump_file = "dump_custom_tri_run1.melt";
    auto fields = "id type proc x y z ix iy iz xs ys zs xu yu zu xsu ysu zsu vx vy vz fx fy fz";

    if (!verbose) ::testing::internal::CaptureStdout();

    command("change_box all triclinic");
    command(fmt::format("dump id all custom 1 {} {}", dump_file, fields));
    command("dump_modify id units yes");
    command("run 1");
    if (!verbose) ::testing::internal::GetCapturedStdout();

    ASSERT_FILE_EXISTS(dump_file);

    auto lines = read_lines(dump_file);
    ASSERT_STREQ(lines[6].c_str(), "ITEM: BOX BOUNDS xy xz yz pp pp pp");
    ASSERT_EQ(utils::split_words(lines[7]).size(), 3);

    ASSERT_EQ(lines.size(), 84);
    delete_file(dump_file);
}

TEST_F(DumpCustomTest, binary_triclinic_run1)
{
    if(!BINARY2TXT_BINARY) GTEST_SKIP();

    auto text_file = "dump_custom_tri_text_run1.melt";
    auto binary_file = "dump_custom_tri_binary_run1.melt.bin";
    auto converted_file = fmt::format("{}.txt", binary_file);
    auto fields = "id type proc x y z xs ys zs xsu ysu zsu vx vy vz fx fy fz";

    if (!verbose) ::testing::internal::CaptureStdout();
    command("change_box all triclinic");
    command(fmt::format("dump id0 all custom 1 {} {}", text_file, fields));
    command(fmt::format("dump id1 all custom 1 {} {}", binary_file, fields));
    command("dump_modify id0 units yes");
    command("dump_modify id1 units yes");
    command("run 1");
    if (!verbose) ::testing::internal::GetCapturedStdout();

    ASSERT_FILE_EXISTS(text_file);
    ASSERT_FILE_EXISTS(binary_file);

    if (!verbose) ::testing::internal::CaptureStdout();
    std::string cmdline = fmt::format("{} {}", BINARY2TXT_BINARY, binary_file);
    system(cmdline.c_str());
    if (!verbose) ::testing::internal::GetCapturedStdout();

    ASSERT_FILE_EXISTS(converted_file);
    ASSERT_FILE_EQUAL(text_file, converted_file);
    delete_file(text_file);
    delete_file(binary_file);
    delete_file(converted_file);
}

TEST_F(DumpCustomTest, with_variable_run1)
{
    auto dump_file = "dump_custom_with_variable_run1.melt";

    if (!verbose) ::testing::internal::CaptureStdout();
    command("compute         1 all property/atom proc");
    command("variable        p atom (c_1%10)+1");
    command(fmt::format("dump id all custom 1 {} id type x y z v_p", dump_file));
    command("dump_modify id units yes");
    command("run 1");
    if (!verbose) ::testing::internal::GetCapturedStdout();


    ASSERT_FILE_EXISTS(dump_file);
    auto lines = read_lines(dump_file);
    ASSERT_EQ(lines.size(), 84);
    ASSERT_STREQ(lines[6].c_str(), "ITEM: BOX BOUNDS pp pp pp");
    ASSERT_EQ(utils::split_words(lines[7]).size(), 2);
    ASSERT_STREQ(lines[10].c_str(), "ITEM: ATOMS id type x y z v_p");
    ASSERT_EQ(utils::split_words(lines[11]).size(), 6);
    delete_file(dump_file);
}


int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    ::testing::InitGoogleMock(&argc, argv);

    // handle arguments passed via environment variable
    if (const char *var = getenv("TEST_ARGS")) {
        std::vector<std::string> env = utils::split_words(var);
        for (auto arg : env) {
            if (arg == "-v") {
                verbose = true;
            }
        }
    }

    BINARY2TXT_BINARY = getenv("BINARY2TXT_BINARY");

    if ((argc > 1) && (strcmp(argv[1], "-v") == 0)) verbose = true;

    int rv = RUN_ALL_TESTS();
    MPI_Finalize();
    return rv;
}
