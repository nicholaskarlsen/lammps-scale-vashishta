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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "tokenizer.h"

using namespace LAMMPS_NS;
using ::testing::Eq;

TEST(Tokenizer, empty_string) {
    Tokenizer t("", " ");
    ASSERT_EQ(t.count(), 0);
}

TEST(Tokenizer, whitespace_only) {
    Tokenizer t("    ", " ");
    ASSERT_EQ(t.count(), 0);
}

TEST(Tokenizer, single_word) {
    Tokenizer t("test", " ");
    ASSERT_EQ(t.count(), 1);
}

TEST(Tokenizer, two_words) {
    Tokenizer t("test word", " ");
    ASSERT_EQ(t.count(), 2);
}

TEST(Tokenizer, prefix_seperators) {
    Tokenizer t("  test word", " ");
    ASSERT_EQ(t.count(), 2);
}

TEST(Tokenizer, postfix_seperators) {
    Tokenizer t("test word   ", " ");
    ASSERT_EQ(t.count(), 2);
}

TEST(Tokenizer, iterate_words) {
    Tokenizer t("  test word   ", " ");
    ASSERT_THAT(t[0], Eq("test"));
    ASSERT_THAT(t[1], Eq("word"));
    ASSERT_EQ(t.count(), 2);
}

TEST(Tokenizer, default_seperators) {
    Tokenizer t(" \r\n test \t word \f");
    ASSERT_THAT(t[0], Eq("test"));
    ASSERT_THAT(t[1], Eq("word"));
    ASSERT_EQ(t.count(), 2);
}

TEST(Tokenizer, for_loop) {
    Tokenizer t(" \r\n test \t word \f");
    std::vector<std::string> list;

    for(auto word : t) {
        list.push_back(word);
    }
    ASSERT_THAT(list[0], Eq("test"));
    ASSERT_THAT(list[1], Eq("word"));
}

TEST(ValueTokenizer, empty_string) {
    ValueTokenizer values("");
    ASSERT_FALSE(values.has_next());
}

TEST(ValueTokenizer, bad_integer) {
    ValueTokenizer values("f10");
    ASSERT_THROW(values.next_int(), InvalidIntegerException);
}

TEST(ValueTokenizer, bad_double) {
    ValueTokenizer values("1a.0");
    ASSERT_THROW(values.next_double(), InvalidFloatException);
}

TEST(ValueTokenizer, valid_int) {
    ValueTokenizer values("10");
    ASSERT_EQ(values.next_int(), 10);
}

TEST(ValueTokenizer, valid_tagint) {
    ValueTokenizer values("42");
    ASSERT_EQ(values.next_tagint(), 42);
}

TEST(ValueTokenizer, valid_bigint) {
    ValueTokenizer values("42");
    ASSERT_EQ(values.next_bigint(), 42);
}

TEST(ValueTokenizer, valid_double) {
    ValueTokenizer values("3.14");
    ASSERT_DOUBLE_EQ(values.next_double(), 3.14);
}

TEST(ValueTokenizer, valid_double_with_exponential) {
    ValueTokenizer values("3.14e22");
    ASSERT_DOUBLE_EQ(values.next_double(), 3.14e22);
}
