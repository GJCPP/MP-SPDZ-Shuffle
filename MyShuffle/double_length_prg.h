#pragma once
#include <array>
#include <string>
#include <emp-tool/utils/prg.h>

#include "block_wrapper.h"

typedef unsigned char byte;
typedef block_wrapper prg_seed;
typedef std::basic_string<block_wrapper> block_string;
const size_t prg_seed_length = 16;

prg_seed random_seed();

std::array<prg_seed, 2> double_length_prg(const prg_seed& seed);

block_string arbitrary_prg(const prg_seed& seed, int length);

block_string& operator^=(block_string& a, const block_string& b);

block_string to_byte_string(const prg_seed& s);
prg_seed to_prg_seed(const block_string& s);

