/*  pantheon.h — libPantheon: on beyond math.h.
 *
 *  Well-documented implementations of functions from the history of
 *  mathematics and science, efficient enough for demanding applications:
 *  no heap allocation, small by-value types, straight-line code.
 *
 *  This revision: Babylonian sexagesimal (base-60) arithmetic and
 *  cuneiform numerals; Egyptian hieroglyphic numerals; Chinese hanzi
	*  numerals and counting rods; Indian Brahmi numerals.  See README.md
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
 *  Vigesimal — the base-20 core value: places, radix, conversions.
 *
 *  The pn_vigesimal counterpart of pn_sex60: a fixed-size array of
 *  bytes inside a ~20-byte struct passed by value — no allocations,
 *  safe for real-time and embedded callers.
 *
 *      value = (-1)^neg * sum_i  places[i] * 20^(radix - 1 - i)
 *
 *  places[i] runs 0..19, most significant first.  In period style,
 *  use radix == n_places.  The radix position and the sign are MODERN
 *  niceties, added so this type is a number and not a riddle: no
 *  book of the codices wrote a radix point, and the Dresden Codex
 *  carries no negative numbers.
 *
 *  double is offered only as a conversion utility, never the canonical
 *  form: 0;5 in this notation is 1/4 and exact, but 0;1 is 1/20 and
 *  has no finite binary expansion (20 = 4 x 5 keeps a factor five),
 *  and 20^13 = 8.192e16 already exceeds 2^53 — one whole row later
 *  than sexagesimal's 60^8, because base twenty is half a square.
 *  ---------------------------------------------------------------------- */

#define PN_VIGESIMAL_MAX_PLACES 16    /* 20^16 ~ 6.6e20: plenty; keeps the struct tiny */

typedef struct pn_vigesimal {
	uint8_t places[PN_VIGESIMAL_MAX_PLACES]; /* each 0..19, most significant first */
	uint8_t n_places;                     /* number of significant places, 1..16 */
	uint8_t radix;                        /* places left of the radix point
	                                         (a modern addition — the Maya
	                                         wrote none); 0 means purely fractional */
	bool    neg;                          /* modern nicety; the codices
	                                         carry no sign */
} pn_vigesimal;

/* --- vigesimal strings (modern scholarly transcription) -----------------
 *
 *  The same reading convention as the sexagesimal transcriptions above:
 *  comma- or colon-separated decimal places, at most one ';' marking
 *  the radix point.  No Maya scribe wrote any of it.
 *
 *      "1,2,0,0"   -> places {1,2,0,0}, radix 4   (period-style = 8800)
 *      "1;12,9"    -> same places, radix 1        (= 1 + 12/20 + 9/400)
 *      "0;5"       -> places {0,5}, radix 1       (= 1/4; parse takes the 0 literally)
 *
 *  Strict: no whitespace, no empty places, no trailing separator.  An
 *  unbroken "399" is NOT decimal 399 — it is read three digits deep and
 *  rejected as out of range (a longer run is range too).  Enter ordinary
 *  integers with pn_vigesimal_from_u64 instead.
 *
 *  Errors: pn_err_null, pn_err_syntax (grammar), pn_err_range (a place
 *  > 19, more than 2 digits per place, or more than
 *  PN_VIGESIMAL_MAX_PLACES places).  On error *out is untouched. */
pn_status pn_vigesimal_parse(const char *text, pn_vigesimal *out);

#define PN_VIGESIMAL_FORMAT_MAX_LEN (1 + 3 * PN_VIGESIMAL_MAX_PLACES + 1) /* '-' + places+seps + NUL */

/*  Format v with sep (',' or ':'; the radix boundary writes ';' when the
 *  radix is interior).  Output is NUL-terminated and at most
 *  PN_VIGESIMAL_FORMAT_MAX_LEN bytes; a smaller buffer gives
 *  pn_err_buffer.
 *
 *  Errors: pn_err_null, pn_err_syntax (bad sep), pn_err_domain
 *  (malformed v: n_places == 0 or > max, a place > 19, radix >
 *  n_places), pn_err_buffer.  On error buf is untouched. */
pn_status pn_vigesimal_format(pn_vigesimal v, char sep, char *buf, size_t buflen);

/*  Exactly how sexagesimal's from_u64 works, rebased: peel 20s off the
 *  bottom, reverse.  0 becomes one place of 0.
 *
 *  Errors: pn_err_null, pn_err_range (unreachable: UINT64_MAX takes 15
 *  places). */
pn_status pn_vigesimal_from_u64(uint64_t n, pn_vigesimal *out);

/*  Reassemble with Horner's scheme.
 *
 *  Errors: pn_err_null, pn_err_domain (malformed v, or radix !=
 *  n_places: not an integer), pn_err_range (the value does not fit a
 *  uint64_t — 16 maxed places = 20^16 − 1 would not). */
pn_status pn_vigesimal_to_u64(pn_vigesimal v, uint64_t *out);

/*  Integer part by Horner in exact uint64_t while it fits, then one
 *  correctly-rounded division F / 20^k while k <= 14 — results are
 *  exact whenever a double can hold them at all.  (A 15-place purely
 *  fractional v falls to the iterated division loop; harmless.)
 *
 *  Errors: pn_err_null, pn_err_domain (malformed v). */
pn_status pn_vigesimal_to_double(pn_vigesimal v, double *out);

/*  Round half-away-from-zero at the frac_places-th vigesimal place
 *  (each 0..19), like pn_sex60_from_double: trailing kept places are
 *  zeros, 0.98 at one place rounds onto a whole 1, NaN/inf rejected.
 *
 *  Errors: pn_err_null, pn_err_domain (NaN, inf, frac_places >
 *  PN_VIGESIMAL_MAX_PLACES), pn_err_range (carried past 16 places). */
pn_status pn_vigesimal_from_double(double x, uint8_t frac_places, pn_vigesimal *out);

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

/* ------------------------------------------------------------------------
	*  Chinese hanzi numerals (UTF-8 rendering).
	*
	*  Chinese numerals are decimal, but they write no bare place-value
	*  digits: the number is two sections around 万, value = A * 10^4 + B,
	*  and each section spells its nonzero digits high to low as
	*  (digit + 千/百/十), the section's units digit bare and last:
	*
	*      四千六百二十二          = 4622      (Wikipedia's worked example)
	*      九千九百九十九万
	*      九千九百九十九            = 99 999 999  (the maximum)
	*
	*  The signs, per the Unicode Standard names list (all in the CJK
	*  Unified Ideographs block, all 3 bytes of UTF-8):
	*
	*    U+4E00 一 = 1      U+4E8C 二 = 2      U+4E09 三 = 3
	*    U+56DB 四 = 4      U+4E94 五 = 5      U+516D 六 = 6
	*    U+4E03 七 = 7      U+516B 八 = 8      U+4E5D 九 = 9
	*    U+5341 十 = 10     U+767E 百 = 100    U+5343 千 = 1000
	*    U+4E07 万 = 10 000
	*
	*  Zero follows the Egyptian blank: a skipped digit is simply not
	*  written, and the skip is acknowledged by exactly one U+0020 space
	*  between the surrounding terms — 1005 is 一千 五 and 1050 is
	*  一千 五十, where later China would write 一千零五 and 一千零五十.
	*  The space never carries value, because each digit's place is
	*  already pinned by its multiplier (a bare digit is always its
	*  section's units digit), so the round trip stays exact — unlike
	*  the load-bearing gap of the counting rods below.
	*
	*  Canonical form is the full formal reading; colloquial elisions are
	*  not canonical: 14 is 一十四, never the spoken 十四.  亿 (10^8) and
	*  traditional 萬 are outside this rendering — the domain stops at
	*  万, the same ruling as Egypt's missing sign above the Heh — so
	*  PN_HANZI_MAX_VALUE is 99 999 999, 九千九百九十九万九千九百九十九.
	* ---------------------------------------------------------------------- */

#define PN_HANZI_MAX_VALUE 99999999ull /* nine 九 in every place of both sections */

/*  Exact number of bytes (excluding the NUL) pn_hanzi_render will
	*  write for a well-formed n: three per sign drawn — every digit, every
	*  multiplier used, and the 万 — plus one for each blank space
	*  acknowledging a skipped digit.  Pure and total — it never fails; for
	*  n > PN_HANZI_MAX_VALUE its answer is simply meaningless, since
	*  render rejects such values with pn_err_range.  render_size(0) == 0:
	*  the blank. */
size_t pn_hanzi_render_size(uint64_t n);

/*  Render n in hanzi numerals into buf as UTF-8, NUL-terminated: write
	*  n = A * 10^4 + B; when A is nonzero, its section high to low (digit +
	*  千/百/十 terms, a bare digit last for the section's units place),
	*  then 万, then B's nonzero terms likewise; a skip of one or more
	*  places between consecutive terms is marked by exactly one space.
	*  n == 0 renders the empty string — the blank.  Needs exactly
	*  pn_hanzi_render_size(n) bytes + NUL; a smaller buffer gives
	*  pn_err_buffer and leaves buf untouched.
	*
	*  Errors: pn_err_null, pn_err_range (n > PN_HANZI_MAX_VALUE),
	*  pn_err_buffer.  On error buf is untouched. */
pn_status pn_hanzi_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered hanzi string — the inverse of pn_hanzi_render.
	*  Strict canonical form: digits 一..九, each followed by its
	*  multiplier (千/百/十) except a bare units digit last in its section;
	*  multipliers strictly descending within a section; at most one 万,
	*  immediately after A's last term and never before any B term:
	*
	*      四千六百二十二  ->  4622
	*      一千 五       ->  1005   (the blank: 百 and 十 skipped)
	*      四万一千      ->  41000   (no space: 万 and 千 adjacent)
	*
	*  Round-trip law, exact: parsing buf after a successful render
	*  recovers n.  An empty buffer (buflen == 0) parses as 0 — the blank
	*  reads back as the nothing it was.  The grammar cannot express a
	*  value past PN_HANZI_MAX_VALUE, so overflow needs no check; the
	*  colloquial 十四, a stray 零 or 〇, a space where adjacent terms
	*  meet (四百 三十一), a missing blank across a skip (五千万五),
	*  repeated or ascending multipliers, a digit after a section's bare
	*  digit, a second 万, or 万 after a multiplier are all
	*  pn_err_syntax.
	*
	*  Errors: pn_err_null, pn_err_syntax (any other byte, a cut-off sign,
	*  leading/trailing/double spaces, or the canonical violations above).
	*  On error *out is untouched. */
pn_status pn_hanzi_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
	*  Counting rod numerals (UTF-8 rendering).
	*
	*  Rod numerals (算筹, in use c. 400 BCE onward in China, Japan,
	*  Korea and Vietnam) are true positional decimal: one bundle of
	*  rods per column, the columns alternating between two forms of the
	*  digits so that a lone rod can never be confused with its rotated
	*  twin.  The Unicode block encodes both forms (Supplementary
	*  Multilingual Plane, all 4 bytes of UTF-8):
	*
	*    U+1D360..U+1D368  COUNTING ROD UNIT DIGIT ONE..NINE
	*                      𝍠 𝍡 𝍢 𝍣 𝍤 𝍥 𝍦 𝍧 𝍨
	*    U+1D369..U+1D371  COUNTING ROD TENS DIGIT ONE..NINE
	*                      𝍩 𝍪 𝍫 𝍬 𝍭 𝍮 𝍯 𝍰 𝍱
	*
	*  The units column takes the unit set, the tens column the tens set,
	*  the hundreds the unit set again, and so on — the Song-dynasty
	*  orientation, which is also what the Unicode names assume.
	*
	*  The rods had NO zero — an empty column was literally left empty,
	*  and the reader counted columns to know each place — so the blank
	*  is rendered as space here, the cuneiform gap generalised to base
	*  ten: every place boundary contributes exactly one space, and a
	*  blank place contributes one of its own.  A well-formed string
	*  therefore holds exactly (places − 1) spaces, which is what makes
	*  the faithful blank round-trip:
	*
	*         𝍪 𝍢       = 23        (adjacent places: one joiner)
	*         𝍠  𝍠      = 101       (the blank tens place: joiner plus one)
	*         𝍠            = 100       (𝍠 plus two blanks, not the lone 1)
	*
	*  A column's form must match the parity of its place: writing 11 as
	*  𝍠 𝍠 instead of 𝍩 𝍠 is the rod equivalent of a DISH before
	*  a U, and pn_rods_parse rejects it.  There is no sign of negative
	*  values (the red/black rods of later Japanese counting are outside
	*  this rendering), and modern transcriptions' 〇 U+3007 is not
	*  written — a zero is a blank, as on the counting board.
	* ---------------------------------------------------------------------- */

#define PN_ROD_UNIT_DIGIT_ONE 0x1D360u /* …through U+1D368 (digit d: +d-1)  */
#define PN_ROD_TENS_DIGIT_ONE  0x1D369u /* …through U+1D371 (digit d: +d-1) */
#define PN_ROD_MAX_PLACES 19           /* 10^19 − 1 still fits uint64_t     */
	#define PN_ROD_MAX_VALUE 9999999999999999999ull /* 10^19 − 1: nineteen nines   */

/*  Exact number of bytes (excluding the NUL) pn_rods_render will write
	*  for a well-formed n: four per rod drawn, plus one space per place
	*  boundary — i.e. 4*(count of nonzero digits) + (count of digits − 1).
	*  Pure and total — it never fails; for n > PN_ROD_MAX_VALUE its answer
	*  is simply meaningless, since render rejects such values with
	*  pn_err_range.  render_size(0) == 0: the empty board. */
size_t pn_rods_render_size(uint64_t n);

/*  Render n in rod numerals into buf as UTF-8, NUL-terminated: one rod
	*  glyph per nonzero digit (unit set at even places, tens set at odd),
	*  most significant first; every place boundary from the top place
	*  down to the units writes exactly one space — joiner between
	*  adjacent columns, blank for empty ones (trailing blanks included).
	*  n == 0 renders the empty string.  Needs exactly
	*  pn_rods_render_size(n) bytes + NUL; a smaller buffer gives
	*  pn_err_buffer and leaves buf untouched.
	*
	*  Errors: pn_err_null, pn_err_range (n > PN_ROD_MAX_VALUE),
	*  pn_err_buffer.  On error buf is untouched. */
pn_status pn_rods_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered rod string — the inverse of pn_rods_render.  The
	*  string is exactly the two rod sets and spaces, in canonical
	*  spacing: it never starts with a space, holds at least one glyph,
	*  and the total count of spaces is one less than the number of
	*  places the value needs — the parser sets places = spaces + 1,
	*  reads the leftmost glyph at place places − 1, and each run of r
	*  spaces advances the place by r:
	*
	*         𝍪 𝍢       ->    23
	*         𝍠  𝍠      ->    101      (the blank tens column)
	*         𝍩             ->    10       (the blank units column)
	*
	*  Round-trip law, exact: parsing buf after a successful render
	*  recovers n.  An empty buffer (buflen == 0) parses as 0.  Errors:
	*  pn_err_null, pn_err_syntax (leading space, all spaces, two glyphs
	*  with no space between, a glyph whose set contradicts its place's
	*  parity, any other byte, a sign cut off short), pn_err_range (more
	*  than PN_ROD_MAX_PLACES places: spaces + 1 > 19).  On error *out is
	*  untouched. */
pn_status pn_rods_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
  *  Indian Brahmi numerals (UTF-8 rendering).
  *
  *  Brahmi numerals (attested from the 3rd century BCE, on Ashoka's edicts
  *  and the coins of the Western Satraps) are the direct graphic ancestors
  *  of the modern Hindu-Arabic digits — but conceptually a different
  *  system: a non-positional decimal notation with NO zero.  See
  *  https://en.wikipedia.org/wiki/Brahmi_numerals .  The twenty signs, per
  *  the Unicode Standard names list (all in the Brahmi block
  *  U+11000..U+1107F, all 4 bytes of UTF-8):
  *
  *    U+11052..U+1105A  BRAHMI NUMBER ONE..NINE      𑁒 𑁓 𑁔 𑁕 𑁖 𑁗 𑁘 𑁙 𑁚
  *    U+1105B..U+11063  BRAHMI NUMBER TEN..NINETY    𑁛 𑁜 𑁝 𑁞 𑁟 𑁠 𑁡 𑁢 𑁣
  *    U+11064           BRAHMI NUMBER ONE HUNDRED    𑁤
  *    U+11065           BRAHMI NUMBER ONE THOUSAND   𑁥
  *
  *  Unlike Egypt, no digit is written by repetition: every unit 1..9 and
  *  every ten 10..90 has its own dedicated sign, so 47 needs two signs,
  *  not eleven.  There is NO sign above the thousand (the Jain infinities
  *  counted in words, not signs) and NO zero — a lone blank is the
  *  nothing owed — so the renderable domain is 0..PN_BRAHMI_MAX_VALUE
  *  and zero renders as the empty string, the scribe's blank again.
  *
  *  Hundreds and thousands are MULTIPLICATIVE, per Unicode 14.x: a digit
  *  2..9 ligatured with the power sign — the power first, then the
  *  U+1107F BRAHMI NUMBER JOINER, then the multiplier — so 400 is
  *  <𑁤 𑁿 𑁕> (100 JOINER 4) rendered as one stacked ligature, while an
  *  additive component stays unligated: 104 is <𑁤 𑁕> (100 4).  The joiner
  *  marks that semantic distinction, so it is never dropped, never
  *  doubled, and legal only between number signs — 1 × 100 is written
  *  with the bare power sign, as 𑁥𑁥 is no number any more than 𓏤..𓏤 ten
  *  strokes were.
  *
  *  Canonical form: up to four terms descending — thousands, hundreds,
  *  tens, ones — each at most once, with no separators; a signs table
  *  and the four slots make every string self-delimiting:
  *
  *    𑁤𑁿𑁓𑁟𑁗  = 256   (the Sasaram edict: 200 ligature + fifty + six)
  *    𑁤𑁟𑁔         = 153   (the Damasena coin: 100-50-3)
  *    𑁥𑁿𑁚𑁤𑁿𑁚𑁣𑁚 = 9999  (the full set; 𑁚 appears three times, in three
  *                          slots — slots, not sign identity, rule)
  * ---------------------------------------------------------------------- */

#define PN_BRAHMI_NUMBER_ONE       0x11052u /* 𑁒 …through U+1105A NINE  = 1..9 */
#define PN_BRAHMI_NUMBER_TEN         0x1105Bu /* 𑁛 …through U+11063 NINETY = 10..90 */
#define PN_BRAHMI_NUMBER_HUNDRED     0x11064u /* 𑁤 one hundred                 */
#define PN_BRAHMI_NUMBER_THOUSAND    0x11065u /* 𑁥 one thousand                */
#define PN_BRAHMI_NUMBER_JOINER      0x1107Fu /* 𑁿 ligature join, numbers only */
#define PN_BRAHMI_MAX_VALUE          9999u    /* four filled slots: no sign above 𑁥 */

/*  Exact number of bytes (excluding the NUL) pn_brahmi_render will
  *  write for a well-formed n: four per sign drawn, joiner included —
  *  a thousands or hundreds term costs four bytes with its digit 1
  *  (bare power sign) and twelve with its digit 2..9 (power, joiner,
  *  multiplier), a tens or ones term four bytes when present.  Pure and
  *  total — it never fails; for n > PN_BRAHMI_MAX_VALUE its answer is
  *  simply meaningless, since render rejects such values with
  *  pn_err_range.  render_size(0) == 0: the blank. */
size_t pn_brahmi_render_size(uint64_t n);

/*  Render n in Brahmi numerals into buf as UTF-8, NUL-terminated: the
  *  terms descending thousands, hundreds, tens, ones; each nonzero digit
  *  its dedicated sign (1..9, or TEN..NINETY for the tens digit), and a
  *  hundreds or thousands digit d written ligatured: d == 1 the bare
  *  power sign, d == 2..9 the power sign, then U+1107F, then NUMBER_d.
  *  n == 0 renders the empty string — the blank.  Needs exactly
  *  pn_brahmi_render_size(n) bytes + NUL; a smaller buffer gives
  *  pn_err_buffer and leaves buf untouched.
  *
  *  Errors: pn_err_null, pn_err_range (n > PN_BRAHMI_MAX_VALUE),
  *  pn_err_buffer.  On error buf is untouched. */
pn_status pn_brahmi_render(uint64_t n, char *buf, size_t buflen);

/*  Parse a rendered Brahmi string — the inverse of pn_brahmi_render.
  *  Strict canonical form: the twenty number signs and the joiner, in
  *  at most four descending terms — thousands, hundreds, tens, ones —
  *  each term at most once, no separators:
  *
  *      𑁤𑁿𑁓𑁟𑁗  ->  256     (𑁤 𑁿 𑁓 = 200 ligatured, 𑁟 = 50, 𑁗 = 6)
  *      𑁤𑁕              ->  104     (additive component: 100 then 4)
  *      𑁥𑁒              ->  1001    (thousand then a bare one)
  *
  *  Round-trip law, exact: parsing buf after a successful render
  *  recovers n.  An empty buffer (buflen == 0) parses as 0 — the blank
  *  reads back as the nothing it was.  The grammar admits no value past
  *  PN_BRAHMI_MAX_VALUE, so overflow needs no check.  Violations are
  *  pn_err_syntax the way 𓏤.. bottom-up was: a second term of one
  *  slot, an ascending term, a joiner doubled or leading or trailing or
  *  joining anything but a multiplier after its power sign, a ligatured
  *  multiplier of 1 (𑁥𑁿𑁒 — write 𑁥 bare), a ligature left unligated
  *  (𑁤𑁕𑁕 would read 100+4+4: no slot allows the second 𑁕), any other
  *  byte, a sign cut off short.  On error *out is untouched.
  *
  *  Errors: pn_err_null, pn_err_syntax. */
pn_status pn_brahmi_parse(const char *buf, size_t buflen, uint64_t *out);

/* ------------------------------------------------------------------------
 *  Mayan bar-and-dot numerals over pn_vigesimal — the renderer.
 *
 *  The Maya had what no other ancient place-value script here can
 *  claim, and what Unicode's cuneiform block pointedly lacks: a real
 *  zero.  Dots for units, bars for fives, a shell for nought, and the
 *  block composites them into one sign per place 0..19:
 *
 *  U+1D2E0..U+1D2E4   MAYAN NUMERAL ZERO..FOUR      shell, dots   = 0..4
 *  U+1D2E5..U+1D2E9   MAYAN NUMERAL FIVE..NINE      bars + dots   = 5..9
 *  U+1D2EA..U+1D2EE   MAYAN NUMERAL TEN..FOURTEEN   two bars      = 10..14
 *  U+1D2EF..U+1D2F3   MAYAN NUMERAL FIFTEEN..NINETEEN three bars  = 15..19
 *
 *  (Unicode block Mayan Numerals, U+1D2E0..U+1D2F3; Unicode 10.0, 2017
 *  after proposal L2/16-264, Jameson Quinn.  All twenty are SMP: four
 *  bytes of UTF-8 each under the stem F0 9D 8B.)
 *
 *  The codex stacks places vertically, least significant at the
 *  bottom; we transcribe the stack top-down made left-to-right —
 *  most significant place first, one space per place boundary, exactly
 *  the linearisation the rod columns get:
 *
\t *      𝋡 𝋣   ->   23     (three dots under one dot: 1x20 + 3)
\t *      𝋡 𝋭   ->   33     (dot above three dots atop two bars)
\t *      𝋡 𝋡 𝋩   ->   429     (dot dot nine: 1x400 + 1x20 + 9)
\t *      𝋡 𝋢 𝋠 𝋠   ->   8800     (the shells hold rows three and four)
\t *      𝋠   ->   0     (the shell itself: no blank needed)
 *
 *  Each place carries exactly ONE precomposed sign, so a zero place
 *  writes its shell instead of the Babylonian's two-space gap: the
 *  string is self-counting, no context tells a reader where the rows
 *  are.  pn_mayan_render draws every place that well_formed allows,
 *  including fractional ones (no radix point is carved; the '-' is an
 *  anachronism, as in the cuneiform renderer above), and pn_mayan_parse
 *  answers period style: radix == n_places.
 *
 *  References: https://en.wikipedia.org/wiki/Maya_numerals ,
 *  https://en.wikipedia.org/wiki/Vigesimal
 * ------------------------------------------------------------------------ */

#define PN_MAYAN_DIGIT_ZERO 0x1D2E0u /* SHELL …through U+1D2F3 (digit d: +d) */

/*  Bytes needed to render v as bar-and-dot Maya numerals, NOT counting
 *  the terminating NUL: one sign per place and one space per place
 *  boundary, plus a '-' when neg.  Pure and total — it never fails; on
 *  a malformed v its answer is simply meaningless, since render rejects
 *  such values with pn_err_domain. */
size_t pn_mayan_render_size(pn_vigesimal v);

/*  Render v as Maya bar-and-dot numerals into buf as UTF-8,
 *  NUL-terminated.  One sign per place; zero places write the shell,
 *  not a gap.  Errors: pn_err_null, pn_err_domain (malformed v:
 *  n_places == 0 or > PN_VIGESIMAL_MAX_PLACES, a place > 19,
 *  radix > n_places), pn_err_buffer (buflen <= render_size(v)).  On
 *  error buf is untouched. */
pn_status pn_mayan_render(pn_vigesimal v, char *buf, size_t buflen);

/*  Parse a rendered bar-and-dot string — the inverse of
 *  pn_mayan_render.  buf holds exactly buflen bytes of UTF-8 using
 *  '-', the twenty signs, and separating single spaces; the grammar is
 *  '-? (sign ' ')* sign', strict: a leading, trailing or doubled space,
 *  two signs touching in one row, any foreign byte (the block's void
 *  past U+1D2F3 included — U+1D2F4..1D2FF are unassigned and never
 *  parse), and a sign cut off mid-UTF-8 are pn_err_syntax; more than
 *  PN_VIGESIMAL_MAX_PLACES places is pn_err_range, the page overrun,
 *  not bad grammar (the ruling the scribe's fifteenth row gets).  An
 *  empty buffer (buflen == 0) is syntax, not zero: the shell is zero's
 *  one true spelling, the way an empty tablet is not a number.
 *  Round-trip law: parsing buf after a successful render recovers v's
 *  places exactly (parsed radix == n_places: period style — the carved
 *  stack carries no point the reader could place back).
 *
 *  Errors: pn_err_null, pn_err_syntax, pn_err_range.  On error *out is
 *  untouched. */
pn_status pn_mayan_parse(const char *buf, size_t buflen, pn_vigesimal *out);

#ifdef __cplusplus
}
#endif
#endif /* PANTHEON_H */
