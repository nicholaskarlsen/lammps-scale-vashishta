/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: Richard Berger (Temple U)
------------------------------------------------------------------------- */

#ifndef LMP_TOKENIZER_H
#define LMP_TOKENIZER_H

#include <string>
#include <vector>
#include "lmptype.h"
#include <exception>

namespace LAMMPS_NS {

#define TOKENIZER_DEFAULT_SEPARATORS " \t\r\n\f"

class Tokenizer {
    std::string text;
    std::string separators;
    size_t start;
    size_t ntokens;
public:
    Tokenizer(const std::string & str, const std::string & separators = TOKENIZER_DEFAULT_SEPARATORS);
    Tokenizer(Tokenizer &&);
    Tokenizer(const Tokenizer &);
    Tokenizer& operator=(const Tokenizer&) = default;
    Tokenizer& operator=(Tokenizer&&) = default;

    void reset();
    void skip(int n);
    bool has_next() const;
    bool contains(const std::string & str) const;
    std::string next();

    size_t count();
    std::vector<std::string> as_vector();
};

class TokenizerException : public std::exception {
  std::string message;
public:
  TokenizerException(const std::string & msg, const std::string & token);

  ~TokenizerException() throw() {
  }

  virtual const char * what() const throw() {
    return message.c_str();
  }
};

class InvalidIntegerException : public TokenizerException {
public:
    InvalidIntegerException(const std::string & token) : TokenizerException("Not a valid integer number", token) {
    }
};

class InvalidFloatException : public TokenizerException {
public:
    InvalidFloatException(const std::string & token) : TokenizerException("Not a valid floating-point number", token) {
    }
};

class ValueTokenizer {
    Tokenizer tokens;
public:
    ValueTokenizer(const std::string & str, const std::string & separators = TOKENIZER_DEFAULT_SEPARATORS);
    ValueTokenizer(const ValueTokenizer &);
    ValueTokenizer(ValueTokenizer &&);
    ValueTokenizer& operator=(const ValueTokenizer&) = default;
    ValueTokenizer& operator=(ValueTokenizer&&) = default;

    std::string next_string();
    tagint next_tagint();
    bigint next_bigint();
    int    next_int();
    double next_double();

    bool has_next() const;
    bool contains(const std::string & value) const;
    void skip(int ntokens);

    size_t count();
};


}

#endif
