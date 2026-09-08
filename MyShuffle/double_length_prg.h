#pragma once
#include <array>
#include <string>
#include <span>

#include "Tools/random.h"

#include "global.h"
#include "block_wrapper.h"
#include "vectors.h"


typedef unsigned char byte;
typedef block_wrapper prg_seed;
typedef std::basic_string<block_wrapper> block_string;

prg_seed random_seed();

std::array<prg_seed, 2> double_length_prg(const prg_seed& seed);

block_string arbitrary_prg(const prg_seed& seed, int length);

void arbitrary_prg(const prg_seed& seed, vectors<ClearType>& dest);

block_string& operator^=(block_string& a, const block_string& b);

block_string to_byte_string(const prg_seed& s);
prg_seed to_prg_seed(const block_string& s);

