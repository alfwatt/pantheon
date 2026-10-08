/* check.h — dependency-free test macros for libPantheon.

    Each tests/test_*.c is its own main(); include this header once per
    binary, accumulate with CHECK/CHECK_OK/CHECK_ERROR/CHECK_DBL/
    CHECK_PLACES, and return the value of CHECK_RUN() from main
    (nonzero = failures).

    CHECK_OK and CHECK_ERROR are for the pn_error style.  Write the
    call exactly as the header declares it, ending with the extra
    argument '&err_'; the macro declares that err_ slot itself,
    proves it stayed NULL on success, and on failure prints the note
    and frees it, so notes never leak between checks.
*/
#ifndef CHECK_H
#define CHECK_H

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "pantheon.h"

static int pn_checks, pn_fails;

/* The header marks required pointers _Nonnull, so clang rightly
    refuses to compile a test that hands one a plain NULL on purpose -
    and those tests exist precisely to prove the null check works.
    Hand these to the call instead: the compiler cannot look inside
    the volatile load below, and what the function under test
    receives is still a null pointer.  A test file that never uses
    this still compiles: the unused attribute keeps -Wunused-function
    quiet where it is not called.   */
__attribute__((unused)) static void* pn_nothing(void) {
    /* A volatile load is opaque to the optimizer, so no constant
      folder here or in the caller can prove the result is null.   */
    volatile void* nothing = NULL;

    return (void*) (uintptr_t) nothing;
}

#define CHECK(cond)                                                         \
    do {                                                                    \
        pn_checks++;                                                        \
        if (!(cond)) {                                                      \
            pn_fails++;                                                     \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                   \
    } while (0)

/* CHECK_OK(call(..., &slot), slot) — the call must succeed: return
   true and leave its error slot NULL. */
#define CHECK_OK(expr, slot)                                                                \
    do {                                                                                    \
        pn_error* slot = NULL;                                                              \
        pn_checks++;                                                                        \
        if (!(expr)) {                                                                      \
            pn_fails++;                                                                     \
            fprintf(stderr, "FAIL %s:%d: %s: %s\n", __FILE__, __LINE__, #expr,              \
                    pn_error_describe(slot));                                               \
        }                                                                                   \
        else if (slot != NULL) {                                                            \
            pn_fails++;                                                                     \
            fprintf(stderr, "FAIL %s:%d: %s: left a note: %s\n", __FILE__, __LINE__, #expr, \
                    pn_error_describe(slot));                                               \
        }                                                                                   \
        pn_error_free(slot);                                                                \
    } while (0)

/* CHECK_ERROR(call(..., &slot), slot, PN_ERROR_X) — the call must
   fail with exactly code PN_ERROR_X and hang a note on the slot. */
#define CHECK_ERROR(expr, slot, want_)                                                         \
    do {                                                                                       \
        pn_error* slot = NULL;                                                                 \
        pn_checks++;                                                                           \
        if (expr) {                                                                            \
            pn_fails++;                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s: succeeded, want %s\n", __FILE__, __LINE__, #expr, \
                    #want_);                                                                   \
        }                                                                                      \
        else if (slot == NULL || slot->code != (want_)) {                                      \
            pn_fails++;                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s: %s\n", __FILE__, __LINE__, #expr,                 \
                    pn_error_describe(slot));                                                  \
        }                                                                                      \
        pn_error_free(slot);                                                                   \
    } while (0)

#define CHECK_DBL(got, want, eps)                                  \
    do {                                                           \
        double g_ = (got), w_ = (want);                            \
        int bad_ = !(fabs(g_ - w_) <= (eps));                      \
        CHECK(!bad_);                                              \
        if (bad_)                                                  \
            fprintf(stderr, "   got %.17g, want %.17g\n", g_, w_); \
    } while (0)

/* CHECK_PLACES(v, radix, place, place, ...) — compare a pn_sex60
   place-by-place, radix included, against literals. */
#define CHECK_PLACES(v, rdx_, ...)                                                                \
    do {                                                                                          \
        static const uint8_t want_[] = {__VA_ARGS__};                                             \
        pn_checks++;                                                                              \
        if ((v).n_places != (uint8_t) sizeof want_ || (v).radix != (uint8_t) (rdx_) ||            \
            memcmp((v).places, want_, sizeof want_) != 0) {                                       \
            pn_fails++;                                                                           \
            fprintf(stderr, "FAIL %s:%d: places/radix mismatch in %s\n", __FILE__, __LINE__, #v); \
        }                                                                                         \
    } while (0)

#define CHECK_RUN() (printf("%d checks, %d failed\n", pn_checks, pn_fails), pn_fails ? 1 : 0)

#endif /* CHECK_H */
