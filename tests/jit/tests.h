/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
// Every test: unsigned f(unsigned a, unsigned b, unsigned* m). m points at a scratch buffer of 256 words.
#define TEST_LIST(X) \
    X(t_arith) X(t_flags) X(t_shifts) X(t_loops) X(t_mem) X(t_sort) X(t_bits) X(t_mul64) X(t_div) X(t_calls) X(t_bytes) X(t_cond)
