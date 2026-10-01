/* check.h — dependency-free test macros for libPantheon.
 *
 * Each tests/test_*.c is its own main(); include this header once per
 * binary, accumulate with CHECK/CHECK_ST/CHECK_DBL/CHECK_PLACES, and
 * return the value of CHECK_RUN() from main (nonzero = failures).
 */
#ifndef CHECK_H
#define CHECK_H

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "pantheon.h"

static int pn_checks, pn_fails;

#define CHECK(cond) do {                                                       \
	pn_checks++;                                                           \
	if (!(cond)) {                                                         \
		pn_fails++;                                                    \
		fprintf(stderr, "FAIL %s:%d: %s\n",                            \
				__FILE__, __LINE__, #cond);                            \
	}                                                                      \
} while (0)

#define CHECK_ST(expr, want) do {                                              \
	pn_checks++;                                                           \
	pn_status got_ = (expr);                                               \
	if (got_ != (want)) {                                                  \
		pn_fails++;                                                    \
		fprintf(stderr, "FAIL %s:%d: %s -> %s\n",                      \
				__FILE__, __LINE__, #expr, pn_strerror(got_));         \
	}                                                                      \
} while (0)

#define CHECK_DBL(got, want, eps) do {                                         \
	double g_ = (got), w_ = (want);                                        \
	int bad_ = !(fabs(g_ - w_) <= (eps));                                  \
	CHECK(!bad_);                                                          \
	if (bad_)                                                              \
		fprintf(stderr, "   got %.17g, want %.17g\n", g_, w_);           \
} while (0)

/* CHECK_PLACES(v, radix, place, place, ...) — compare a pn_sex60
 * place-by-place, radix included, against literals. */
#define CHECK_PLACES(v, rdx_, ...) do {                                        \
	static const uint8_t want_[] = { __VA_ARGS__ };                        \
	pn_checks++;                                                           \
	if ((v).n_places != (uint8_t)sizeof want_                              \
			|| (v).radix != (uint8_t)(rdx_)                           \
			|| memcmp((v).places, want_, sizeof want_) != 0) {         \
		pn_fails++;                                                    \
		fprintf(stderr, "FAIL %s:%d: places/radix mismatch in %s\n",   \
				__FILE__, __LINE__, #v);                               \
	}                                                                      \
} while (0)

#define CHECK_RUN() (printf("%d checks, %d failed\n", pn_checks, pn_fails), \
	pn_fails ? 1 : 0)

#endif /* CHECK_H */
