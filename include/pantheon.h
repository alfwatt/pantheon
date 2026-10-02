/*  pantheon.h — libPantheon: on beyond math.h.
 *
 *  Well-documented implementations of functions from the history of
 *  mathematics and science, efficient enough for demanding applications:
 *  no heap allocation, small by-value types, straight-line code.
 *
 *  This revision: Babylonian sexagesimal (base-60) arithmetic and
 *  cuneiform numerals; Egyptian hieroglyphic numerals.  See README.md
 *  for the roadmap.
 */
#ifndef PANTHEON_H
#define PANTHEON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * Babylonian sexagesimal (base-60) values.
 *
 * A value is a string of "places", each holding a digit 0..59, most
 * significant first.  The Babylonians (c. 1800 BCE onward) wrote these
 * with two cuneiform signs and — crucially — had NO zero placeholder
 * and NO radix point: 𒁹 (one vertical wedge) alone could mean 1, 60
 * or 1/60; context decided.  See
 * https://en.wikipedia.org/wiki/Sexagesimal and
 * https://en.wikipedia.org/wiki/Babylonian_numerals .
 *
 * pn_sex60 therefore carries an explicit radix position, a MODERN
 * convention added so this type is a number and not a riddle:
 *
 *     value = (-1)^neg * sum_i  places[i] * 60^(radix - 1 - i)
 *
 * In period style, use radix == n_places (a pure integer).
 *
 * The representation is a fixed-size array of bytes inside a ~20-byte
 * struct passed by value: no allocations, safe for real-time and
 * embedded callers.  double is offered only as a conversion utility,
 * never the canonical form — many exact sexagesimal values (0;20 is
 * 1/3) have no finite binary expansion, and 60^9 already exceeds 2^53,
 * beyond the exact range of a double.
 * ---------------------------------------------------------------------- */

#define PN_SEX60_MAX_PLACES 16  /* 60^16 ~ 2.8e28: plenty; keeps the struct tiny */

typedef struct pn_sex60 {
    uint8_t places[PN_SEX60_MAX_PLACES]; /* each 0..59, most significant first */
    uint8_t n_places;                     /* number of significant places, 1..16 */
    uint8_t radix;                        /* places left of the radix point
                                             (a modern addition — the Babylonians
                                             wrote none); 0 means purely fractional */
    bool    neg;                          /* modern nicety; Old Babylonian texts
                                             carry no sign */
} pn_sex60;

typedef enum pn_status {
    pn_ok = 0,
    pn_err_null,   /* a pointer argument was NULL                   */
    pn_err_syntax, /* malformed input string                        */
    pn_err_range,  /* too many places, a place > 59, or the value
                      does not fit the target type                  */
    pn_err_domain, /* value not representable in the requested form
                      (e.g. fractional places converted to integer) */
    pn_err_buffer, /* caller's buffer too small                     */
} pn_status;

/* Error text for printf("%s", pn_strerror(st)).  Never NULL. */
const char *pn_strerror(pn_status st);

/* --- sexagesimal strings (modern scholarly transcription) --------------
 *
 * Old Babylonian values survive in print as comma- or colon-separated
 * places, e.g. the famous YBC 7289 diagonal:  1,24,51,10  (= about 1.41421,
 * an excellent approximation of sqrt(2)).  Following Neugebauer, we let a
 * semicolon mark the radix point — the convention for readings like
 * 1;24,51,10 — though no Babylonian tablet ever wrote one.
 * -------------------------------------------------------------------- */

/* Parse a modern transcription: optional '-', then one or more places of
 * one or two decimal digits (each 0..59) separated by ',', ':' or ';'.
 * At most one ';' is allowed, and it records the radix position:
 *
 *     "1,24,51,10"  -> places {1,24,51,10}, radix 4  (period-style integer)
 *     "1;24,51,10"  -> same places, radix 1          (about 1.414...)
 *     "0;45"          -> places {0,45}, radix 1  (= 3/4; parse takes the 0 literally)
 *
 * Strict: no whitespace, no empty places, no trailing separator.  Note
 * that an unbroken "3599" is NOT decimal 3599 — it is the places 35 and
 * 99, so it fails on 99 > 59.  Enter ordinary integers with
 * pn_sex60_from_u64 instead.
 *
 * Errors: pn_err_null, pn_err_syntax (grammar), pn_err_range (a place
 * > 59, more than 2 digits per place, or more than PN_SEX60_MAX_PLACES
 * places).  On error *out is untouched. */
pn_status pn_sex60_parse(const char *text, pn_sex60 *out);

/* Format back to a transcription.  sep must be ',', ':' or ';'.  Places
 * are written as two digits — only the first place drops its leading
 * zero — joined with sep, except the radix boundary, which is always
 * ';' when 0 < radix < n_places:
 *
 * 	 {1,24,51,10} radix 1  with sep ','   ->    "1;24,51,10"
 * 	 {2,23,3}     radix 3  with sep ','   ->    "2,23,03"
 *
 * Caveat: with sep ';' and 0 < radix < n_places the output carries two
 * ';' and will not parse back — use ',' or ':' when you need a round
 * trip.  A negative value gets a leading '-'.  Writes a NUL; needs at
 * most PN_SEX60_FORMAT_MAX_LEN bytes; a smaller buffer gives
 * pn_err_buffer and leaves buf untouched.  Errors: pn_err_null,
 * pn_err_buffer, pn_err_syntax (bad sep), pn_err_domain (malformed
 * value). */
#define PN_SEX60_FORMAT_MAX_LEN (1 + 3 * PN_SEX60_MAX_PLACES + 1) /* '-' + places+seps + NUL */
pn_status pn_sex60_format(pn_sex60 v, char sep, char *buf, size_t buflen);

/* --- conversion to / from ordinary numbers ------------------------------ */

/* Ordinary unsigned integer -> sexagesimal by repeated division by 60:
 *
 *     8583 -> places {2,23,3}, radix 3   (8583 = 2*60^2 + 23*60 + 3;
 *                                         Wikipedia's worked example)
 *     0    -> one place {0}
 *
 * Errors: pn_err_null, pn_err_range (more than PN_SEX60_MAX_PLACES
 * places; in practice unreachable, since 2^64 needs only 11 places). */
pn_status pn_sex60_from_u64(uint64_t n, pn_sex60 *out);

/* Sexagesimal integer -> ordinary unsigned integer by Horner's scheme
 * (acc = acc*60 + place).  Requires radix == n_places.
 *
 * Errors: pn_err_null, pn_err_domain (any fractional place, i.e.
 * radix != n_places, or a malformed value incl. a place > 59),
 * pn_err_range (overflow of uint64_t). */
pn_status pn_sex60_to_u64(pn_sex60 v, uint64_t *out);

/* Sexagesimal -> double.  Evaluation folds right-to-left,
 * val = (val + place) / 60, so integer values are exact while the
 * integer part fits 2^53 (radix <= 8, since 60^8 ~ 1.7e14).
 *
 * Like every conversion to binary floating point this cannot preserve
 * every sexagesimal value: 0;20 (= 1/3) has no finite binary expansion,
 * and beyond 2^53 results are correctly rounded but not exact.
 *
 * Errors: pn_err_null, pn_err_domain (malformed value). */
pn_status pn_sex60_to_double(pn_sex60 v, double *out);

/* double -> sexagesimal with exactly frac_places fractional places
 * (0..PN_SEX60_MAX_PLACES), rounded half-away-from-zero at the last kept
 * place with carries propagated left.  Trailing zero places are KEPT,
 * not trimmed, so results are deterministic:
 *
 *     0.75 with 1 frac place -> {45}      radix 0   (0;45 = 3/4)
 *     1.5  with 1 frac place -> {1,30}    radix 1   (1;30)
 *
 * The Babylonians themselves would have dropped trailing empty places —
 * and could afford to, since context told the reader the magnitude
 * anyway.  NaN and infinity are rejected; see pn_sex60_to_double for
 * the general exactness caveats.
 *
 * Errors: pn_err_null, pn_err_domain (NaN/inf, frac_places > max),
 * pn_err_range (integer part needs more than PN_SEX60_MAX_PLACES
 * places). */
pn_status pn_sex60_from_double(double x, uint8_t frac_places, pn_sex60 *out);

/* --- Babylonian cuneiform numerals (UTF-8 rendering) --------------------
 *
 * The two positional signs of the Neo-Babylonian system, per the
 * Unicode Standard names list:
 *
 *   U+12079  CUNEIFORM SIGN DISH   𒁹   vertical wedge, value 1
 *            UTF-8: F0 92 81 B9
 *   U+1230B  CUNEIFORM SIGN U      𒌋   corner wedge,   value 10
 *            UTF-8: F0 92 8C 8B
 *
 * Both live in the main Cuneiform block (U+12000..U+123FF); the
 * precomposed signs of the "Cuneiform Numbers and Punctuation" block
 * (U+12400..) are metrology signs, not school-text positional digits,
 * so we do NOT use them — each place is written as (d/10) copies of U
 * followed by (d%10) copies of DISH, e.g. 23 = 𒌋𒌋𒁹𒁹𒁹.
 *
 * Unicode encodes NO zero placeholder for the sexagesimal gap.  An
 * empty place is rendered as two spaces — faithfully reproducing the
 * original place-value ambiguity: on the clay tablet a lone 𒁹 was 1 or
 * 60 or 1/60, and only context told the reader which.
 * -------------------------------------------------------------------- */

#define PN_CUNEIFORM_DISH 0x12079u /* 𒁹 vertical wedge = 1  */
#define PN_CUNEIFORM_U    0x1230Bu /* 𒌋 corner wedge    = 10 */

/* Exact number of bytes (excluding the NUL) pn_babylonian_render will
 * write for a well-formed v.  Pure and total — it never fails; on a
 * malformed v its answer is simply meaningless, since render rejects
 * such values with pn_err_domain. */
size_t pn_babylonian_render_size(pn_sex60 v);

/* Render v in positional cuneiform style into buf as UTF-8, NUL-
 * terminated.  Each place is a group of (d/10) U signs + (d%10) DISH
 * signs; groups are separated by one space; a zero place is two spaces
 * (the historical gap); negatives get a leading '-', an anachronism.
 * Any well-formed v renders — the caller decides which places are
 * "really" there; a radix point is not drawn (no Babylonian tablet had
 * one), but fractional places render the same as integer ones.
 *
 * Errors: pn_err_null, pn_err_buffer (buflen <= render_size(v)),
 * pn_err_domain (malformed value: n_places == 0, n_places > max, a
 * place > 59, or radix > n_places).  On error buf is untouched. */
pn_status pn_babylonian_render(pn_sex60 v, char *buf, size_t buflen);

/* Parse a rendered cuneiform string — the inverse of
 * pn_babylonian_render.  buf holds exactly buflen bytes of UTF-8 using
 * only '-', the two signs, and separating spaces:
 *
 * 	     𒁹𒁹 𒌋𒌋𒁹𒁹𒁹 𒁹𒁹𒁹              ->   {2,23,3}
 *
 * A place group is (d/10) U signs then (d%10) DISH signs; groups join
 * with one space, and the two-space gap of a zero place parses back as
 * a zero place.  The spacing is strict — between signs it must run 1,
 * 4, 7, … spaces (3k+1: joiner plus k gaps); leading and trailing runs
 * are multiples of 3; an all-space string has length 3k-1.  Any other
 * spacing, and any other byte, is malformed.  Round-trip law:
 *
 * 	     pn_babylonian_parse(buf, strlen(buf), &w) after a successful
 * 	     pn_babylonian_render(v, buf, …) recovers v's places exactly.
 *
 * The cuneiform itself carries no radix point ('-' notwithstanding),
 * so the parsed value gets the period-style reading: radix ==
 * n_places.  A lone 𒁹 parses as 1 — whether the scribe meant 60 or
 * 1/60, only context knew.
 *
 * Errors: pn_err_null, pn_err_syntax (any other byte, bad spacing,
 * empty string, a DISH before a U in one group), pn_err_range (more
 * than 5 U or 9 DISH signs in a group, or more than
 * PN_SEX60_MAX_PLACES places).  On error *out is untouched. */
pn_status pn_babylonian_parse(const char *buf, size_t buflen, pn_sex60 *out);

/* ------------------------------------------------------------------------
 * Egyptian hieroglyphic numerals (UTF-8 rendering).
 *
 * The Egyptian system (c. 3000 BCE onward) was decimal but ADDITIVE: a
 * distinct hieroglyph for each power of ten, repeated as many times as
 * the digit demands — no positional notation at all.  See
 * https://en.wikipedia.org/wiki/Egyptian_numerals .  The seven signs,
 * per the Unicode Standard names list (all in the Egyptian Hieroglyphs
 * block U+13000..U+1342F, all 4 bytes of UTF-8):
 *
 *   U+133E4  EGYPTIAN HIEROGLYPH Z001   𓏤  single stroke,      value 1
 *   U+13386  EGYPTIAN HIEROGLYPH V020   𓎆  cattle hobble,        value 10
 *   U+13362  EGYPTIAN HIEROGLYPH V001   𓍢  coil of rope,         value 100
 *   U+131BC  EGYPTIAN HIEROGLYPH M012   𓆼  lotus,                value 1 000
 *   U+130AD  EGYPTIAN HIEROGLYPH D050   𓂭  bent finger,          value 10 000
 *   U+13190  EGYPTIAN HIEROGLYPH I008   𓆐  tadpole,              value 100 000
 *   U+13068  EGYPTIAN HIEROGLYPH C011   𓁨  Heh (god, arms raised), value 1 000 000
 *
 * There is NO sign above the Heh and NO zero: the scribe simply left a
 * blank where our 0 sits, and a lone blank on an accounting ostracon
 * meant "nothing owed".  So the renderable domain is the scribe's full
 * set, 0..PN_EGYPTIAN_MAX_VALUE — every sign at most nine times — and
 * zero renders as the empty string.  Values beyond the Heh (10 000 000
 * would need ten Heh signs, 10^8 a hundred) are pn_err_range, not
 * because Egypt could not count them but because this rendering insists
 * each sign be written out, and a nine-million-sign tablet fits no
 * caller's buffer.
 *
 * Canonical form: signs grouped in DESCENDING powers of ten, one
 * contiguous run per sign, with no separators — unlike cuneiform, the
 * distinct signs are self-delimiting and there is no gap to preserve.
 * 4622 (the Karnak carving) is thus four lotus + six coils + two
 * hobbles + two strokes, largest first, as any scribe would carve it.
 * ---------------------------------------------------------------------- */

#define PN_EGYPTIAN_STROKE     0x133E4u  /* 𓏤 single stroke   = 1          */
#define PN_EGYPTIAN_HOBBLE     0x13386u  /* 𓎆 cattle hobble   = 10         */
#define PN_EGYPTIAN_COIL       0x13362u  /* 𓍢 coil of rope    = 100        */
#define PN_EGYPTIAN_LOTUS      0x131BCu  /* 𓆼 lotus           = 1 000      */
#define PN_EGYPTIAN_FINGER     0x130ADu  /* 𓂭 bent finger     = 10 000     */
#define PN_EGYPTIAN_TADPOLE    0x13190u  /* 𓆐 tadpole         = 100 000    */
#define PN_EGYPTIAN_HEH        0x13068u  /* 𓁨 Heh god         = 1 000 000  */
#define PN_EGYPTIAN_MAX_VALUE  9999999ull /* nine of each sign: the scribe's full set */

/* Exact number of bytes (excluding the NUL) pn_egyptian_render will
 * write: four per sign drawn, i.e. 4 * (sum of decimal digits) for
 * n <= PN_EGYPTIAN_MAX_VALUE.  Pure and total — it never fails; for
 * n > PN_EGYPTIAN_MAX_VALUE its answer is simply meaningless, since
 * render rejects such values with pn_err_range.  render_size(0) == 0:
 * the scribe's blank. */
size_t pn_egyptian_render_size(uint64_t n);

/* Render n in hieroglyphic numerals into buf as UTF-8, NUL-terminated:
 * for each decimal digit d at 10^k, d copies of that power's sign, runs
 * written in descending powers (10^6 first) with no separators.  n == 0
 * renders the empty string — the faithful blank.  Needs exactly
 * pn_egyptian_render_size(n) bytes + NUL; a smaller buffer gives
 * pn_err_buffer and leaves buf untouched.
 *
 * Errors: pn_err_null, pn_err_range (n > PN_EGYPTIAN_MAX_VALUE),
 * pn_err_buffer.  On error buf is untouched. */
pn_status pn_egyptian_render(uint64_t n, char *buf, size_t buflen);

/* Parse a rendered hieroglyphic string — the inverse of
 * pn_egyptian_render.  buf holds exactly buflen bytes of UTF-8 made of
 * the seven signs only, grouped in canonical form: one contiguous run
 * per sign, runs strictly descending in power, each run 1..9 copies.
 * The value is the sum of every sign's worth, so the round-trip law is
 * exact:
 *
 *      pn_egyptian_parse(buf, strlen(buf), &n) after a successful
 *      pn_egyptian_render(n, buf, ...) recovers n.
 *
 * An empty buffer (buflen == 0) parses as 0 — the blank reads back as
 * the nothing it was.  Any other byte, a run out of descending order or
 * the same sign written in two runs, is malformed; no sign was ever
 * written bottom-up on a temple wall.
 *
 * Errors: pn_err_null, pn_err_syntax (any other byte, a cut-off sign,
 * ascending or split runs), pn_err_range (a run of ten or more copies
 * of one sign — ten strokes are written as one hobble, so that string
 * is not a canonical number).  On error *out is untouched. */
pn_status pn_egyptian_parse(const char *buf, size_t buflen, uint64_t *out);

#ifdef __cplusplus
}
#endif
#endif /* PANTHEON_H */
