#pragma once
#ifndef GLOBAL_H
#define GLOBAL_H
#include "Math/gfp.hpp"
#include "Protocols/Share.h"

// bit length of prime
const int prime_length = 128;

// compute number of 64-bit words needed
const int n_limbs = (prime_length + 63) / 64;

typedef Share<gfp_<0, n_limbs>> ShareType;
typedef typename ShareType::clear ClearType;

// statistical security parameter
#define STATISTICAL_LAMBDA 30

#define MAXN 40

// using typename emp::block;

#define DEBUG

#define DEFAULT_EXPAND_SIZE 500

#define FAIL_INFO __FILE__ << __FUNCTION__ << ", " << __LINE__ << " : "

#endif

