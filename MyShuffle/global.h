#include <emp-tool/emp-tool.h>

#define BLOCKS_FOR_HASH (emp::Hash::DIGEST_SIZE / sizeof(emp::block))

#define IS_ZERO_BLOCK(x) (_mm_movemask_epi8(_mm_cmpeq_epi32(x,emp::zero_block)) == 0xFFFF)

// statistical security parameter
#define STATISTICAL_LAMBDA 40

#define MAXN 40

using typename emp::block;

#define DEBUG

