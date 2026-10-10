/*  test_vigesimal.c — base-20 parse/format/convert, the sex60 shape.
   Golden values:
   - Wikipedia's worked examples (Maya numerals): 23 = [1,3],
   33 = [1,13], 429 = [1,1,9], read top-down in 20s.
   - 8800 = 1*20^3 + 2*20^2 (the codex page depth, four places).
   - 1 + 12/20 + 9/400 = 1.6225: "1;12,9" in modern transcription,
   a fractional example (an anachronism, kept for the type).
*/

#include "check.h"

int main(void) {
    pn_base20 v;
    pn_base20 w;
    double d;
    uint64_t u;
    char buf[PN_BASE20_FORMAT_MAX_LEN];

    /* the codex page's worked examples */

    CHECK_VALUE(v = pn_base20_parse("1;12,9", &err_), err_);
    CHECK(v.n_places == 3 && v.radix == 1 && !v.neg);
    CHECK(v.places[0] == 1 && v.places[1] == 12 && v.places[2] == 9);
    CHECK_OK(pn_base20_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 1.6225, 1e-15);

    /* 0;5 is a quarter: exact in binary, unlike sexagesimal's 0;20 = 1/3 */
    CHECK_VALUE(v = pn_base20_parse("0;5", &err_), err_);
    CHECK(v.n_places == 2 && v.radix == 1);
    CHECK_OK(pn_base20_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 0.25, 0.0); /* exact: 1/4 is dyadic */

    CHECK_OK(pn_base20_from_u64(8800, &v, &err_), err_);
    CHECK_PLACES(v, 4, 1, 2, 0, 0);
    CHECK_OK(pn_base20_to_u64(v, &u, &err_), err_);
    CHECK(u == 8800);

    /* separators: comma, colon — three spellings, one value */
    CHECK_VALUE(v = pn_base20_parse("1,2,0,0", &err_), err_);
    CHECK_VALUE(w = pn_base20_parse("1:2:0:0", &err_), err_);
    CHECK(v.n_places == 4 && v.radix == 4);
    CHECK(memcmp(&v, &w, sizeof v) == 0);
    CHECK_OK(pn_base20_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 8800.0, 0.0);

    /* zero and the domain edges */

    CHECK_OK(pn_base20_from_u64(0, &v, &err_), err_);
    CHECK_PLACES(v, 1, 0);
    CHECK_OK(pn_base20_to_u64(v, &u, &err_), err_);
    CHECK(u == 0);

    CHECK_OK(pn_base20_from_u64(20, &v, &err_), err_);
    CHECK_PLACES(v, 2, 1, 0);
    CHECK_OK(pn_base20_from_u64(8000, &v, &err_), err_); /* 20^3 */
    CHECK_PLACES(v, 4, 1, 0, 0, 0);

    /* UINT64_MAX: fifteen places, no padding row */
    CHECK_OK(pn_base20_from_u64(UINT64_MAX, &v, &err_), err_);
    CHECK(v.n_places == 15);
    CHECK_OK(pn_base20_to_u64(v, &u, &err_), err_);
    CHECK(u == UINT64_MAX);

    /* round trip, exhaustive */

    for (uint64_t n = 0; n < 160000u; n++) {
        CHECK_OK(pn_base20_from_u64(n, &v, &err_), err_);
        CHECK_OK(pn_base20_to_u64(v, &u, &err_), err_);
        CHECK(u == n);
        if (pn_fails) {
            break; /* the first failure names n */
        }
    }

    /* format */

    CHECK_OK(pn_base20_from_u64(8800, &v, &err_), err_);
    CHECK_OK(pn_base20_format(v, ',', buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "1,2,0,0") == 0);
    CHECK_OK(pn_base20_format(v, ':', buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "1:2:0:0") == 0);
    CHECK_ERROR(pn_base20_format(v, ',', buf, strlen("1,2,0,0"), &err_), err_, PN_ERROR_BUFFER);

    CHECK_VALUE(v = pn_base20_parse("1;12,9", &err_), err_);
    CHECK_OK(pn_base20_format(v, ',', buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "1;12,9") == 0); /* interior radix writes ';' */

    /*  the first place drops its leading zero exactly as sex60's rule:
	   {0,5} radix 1 neg prints "-0;5": the 0 keeps its units digit
	*/
    pn_base20 hand = {{0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 2, 1, true};
    CHECK_OK(pn_base20_format(hand, ',', buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, "-0;5") == 0);

    /* conversions: negatives, fractions, rounding */

    CHECK_VALUE(v = pn_base20_parse("-1,2,0,0", &err_), err_);
    CHECK(v.neg);
    /*  to_u64 reads the magnitude: the same quirk pn_base60_to_u64 carries
	   (neg is the caller's business; see src/sex60.c)
	*/
    CHECK_OK(pn_base20_to_u64(v, &u, &err_), err_);
    CHECK(u == 8800);

    /*  exact round trips through double on dyadic sixteenths-of-sixty-
	   fourths: dyadics have finite vigesimal expansions (20^k carries
	   2^(2k)), and the radix follows the integer peel - 0 below 1
	*/
    for (unsigned i = 0; i < 64u; i++) {
        double x = (double) i / 64.0; /* six places carry the halves */
        CHECK_OK(pn_base20_from_double(x, 6, &v, &err_), err_);
        CHECK(!v.neg && v.n_places == 6 && v.radix == (x >= 1.0 ? 1 : 0));
        CHECK_OK(pn_base20_to_double(v, &d, &err_), err_);
        CHECK_DBL(d, x, 0.0); /* exact: halves of halves */
        if (pn_fails) {
            break;
        }
    }

    CHECK_OK(pn_base20_from_double(4.25, 1, &v, &err_), err_);
    CHECK_PLACES(v, 1, 4, 5); /* 0.25 x 20 = 5 exactly */
    CHECK_OK(pn_base20_to_double(v, &d, &err_), err_);
    CHECK_DBL(d, 4.25, 0.0);

    CHECK_OK(pn_base20_from_double(-1.6, 1, &v, &err_), err_);
    CHECK(v.neg);
    CHECK_PLACES(v, 1, 1, 12);

    /* round half away from zero at the last kept place: 9.8 -> 10 */
    CHECK_OK(pn_base20_from_double(0.49, 1, &v, &err_), err_);
    CHECK_PLACES(v, 0, 10); /* purely fractional */
                            /*  0.98 at one place: 19.6 -> 20 carries onto a whole 1 off an empty
	   integer chain: {1,0} radix 1
	*/
    CHECK_OK(pn_base20_from_double(0.98, 1, &v, &err_), err_);
    CHECK(!v.neg && v.n_places == 2 && v.radix == 1);
    CHECK(v.places[0] == 1 && v.places[1] == 0);

    /* everything rounded to nought with no places kept is the single {0} */
    CHECK_OK(pn_base20_from_double(0.01, 0, &v, &err_), err_);
    CHECK(v.n_places == 1 && v.places[0] == 0);
    /* and 0.7 at zero places rounds up to 1: the surviving fraction speaks */
    CHECK_OK(pn_base20_from_double(0.7, 0, &v, &err_), err_);
    CHECK(v.n_places == 1 && v.radix == 1 && v.places[0] == 1);

    /* NaN, infinity, too many places: domain guards */
    CHECK_ERROR(pn_base20_from_double(NAN, 2, &v, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_ERROR(pn_base20_from_double(INFINITY, 2, &v, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_ERROR(pn_base20_from_double(1.0, 17, &v, &err_), err_, PN_ERROR_DOMAIN);

    /*  errors: malformed structs
	   The poke-at-the-struct defence: pn_base60's well_formed rule book,
	   rebased. *out / buf must stay untouched — checked by sentinel.
	*/
    pn_base20 bad;
    memset(&bad, 0, sizeof bad);
    bad.n_places = 4;
    bad.radix = 4;
    bad.places[0] = 1;
    bad.places[1] = 2;
    bad.places[2] = 20;
    bad.places[3] = 0;
    u = 256u;
    CHECK_ERROR(pn_base20_to_u64(bad, &u, &err_), err_, PN_ERROR_DOMAIN); /* place > 19 */
    CHECK(u == 256u);                                                     /* untouched */
    CHECK_ERROR(pn_base20_to_double(bad, &d, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_ERROR(pn_base20_format(bad, ',', buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);

    memset(&bad, 0, sizeof bad);
    bad.n_places = 2;
    bad.radix = 1;
    bad.places[0] = 1;
    bad.places[1] = 2;
    u = 256u; /* interior radix: fractional */
    CHECK_ERROR(pn_base20_to_u64(bad, &u, &err_), err_, PN_ERROR_DOMAIN);
    CHECK(u == 256u);

    memset(&bad, 0, sizeof bad); /* n_places 0: the void */
    CHECK_ERROR(pn_base20_to_u64(bad, &u, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_ERROR(pn_base20_to_double(bad, &d, &err_), err_, PN_ERROR_DOMAIN);
    CHECK_ERROR(pn_base20_format(bad, ',', buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);

    /* 16 maxed places = 20^16 − 1: no uint64_t on the bench */
    pn_base20 maxed;
    memset(&maxed, 0, sizeof maxed);
    maxed.n_places = PN_BASE20_MAX_PLACES;
    maxed.radix = PN_BASE20_MAX_PLACES;
    for (unsigned i = 0; i < PN_BASE20_MAX_PLACES; i++) {
        maxed.places[i] = 19;
    }
    u = 256u;
    CHECK_ERROR(pn_base20_to_u64(maxed, &u, &err_), err_, PN_ERROR_RANGE);
    CHECK(u == 256u);

    /* errors: transcription */

    static const char* const bad_strings[] = {
        "",  "-",   "1;",   "1,",   "1;2;3", " 1", "1 ",  "1 2",
        "x", "1.x", "1,,2", "1;2:", "1:",    ";;", "1-1",
    };
    for (size_t i = 0; i < sizeof bad_strings / sizeof bad_strings[0]; i++) {
        CHECK_VALUE_ERROR(v = pn_base20_parse(bad_strings[i], &err_), err_, PN_ERROR_SYNTAX);
    }

    /* value-shaped but out-of-range rows */
    CHECK_VALUE_ERROR(v = pn_base20_parse("25", &err_), err_, PN_ERROR_RANGE);   /* 25 > 19 */
    CHECK_VALUE_ERROR(v = pn_base20_parse("100", &err_), err_, PN_ERROR_RANGE);  /* 3 digits */
    CHECK_VALUE_ERROR(v = pn_base20_parse("0;45", &err_), err_, PN_ERROR_RANGE); /* 45 > 19 */
    /* 17 places is past the struct */
    CHECK_VALUE_ERROR(v = pn_base20_parse("0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0", &err_), err_,
                      PN_ERROR_RANGE);

    return CHECK_RUN();
}
