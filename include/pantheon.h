/*  pantheon.h — libPantheon: on beyond math.h.

    Documented functions from the history of mathematics and science,
    efficient enough for demanding applications.

    Success costs nothing: no heap allocation, small by-value types,
    straight-line code.  A failure is the one moment the library
    allocates: it hangs a pn_error note on the caller's error slot,
    and the caller frees the note when done reading it.

    Every function that can fail takes a 'pn_error **error' slot as
    its last argument and returns true on success.  Before calling,
    the caller sets its own 'pn_error *' variable to NULL.  On
    success the slot stays NULL and the result lands in the out
    parameters.  On failure the function returns false, the note
    lands in the slot, and the out parameters keep their old values.

    This revision: Babylonian sexagesimal (base-60) arithmetic and
    cuneiform numerals; Egyptian hieroglyphic numerals; Chinese hanzi
    numerals and counting rods; Indian Brahmi numerals.  See README.md
    for the roadmap.
*/
#ifndef PANTHEON_H
#define PANTHEON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wc23-extensions"

// MARK: - Errors

/*
  A failed call leaves behind an error report that explains itself.
  The report carries three things: a code that names the kind of failure,
  a message that explains it in a sentence a person can read.

  A caller reads the report with pn_error_describe and hands it
  back with pn_error_free.
*/

/// @enum `pn_error_code`: the five ways a call can fail
typedef enum pn_error_code {
    PN_ERROR_NULL,   /* a required resource was missing (a NULL is an
                        assert, not a note)                           */
    PN_ERROR_SYNTAX, /* an input string breaks the writing rules      */
    PN_ERROR_RANGE,  /* a number is too big for this number system    */
    PN_ERROR_DOMAIN, /* a number of the wrong kind for this job       */
    PN_ERROR_BUFFER, /* the caller's buffer is too small              */
} pn_error_code;

/// @struct `pn_error`: the report a failed call leaves behind
/// @field code - the error code, one of the `PN_ERROR_` values above
/// @field message - a human readable sentence describing the error,
///     may be NULL when the code alone says everything
/// @field prior - a pointer to a prior error that caused this error,
///     may be NULL
typedef struct pn_error {
    pn_error_code code;
    char* _Nullable message;
    struct pn_error* _Nullable prior;
} pn_error;

/// @returns an allocated `pn_error*` with the
/// @param code provided and
/// @param format for the error message
[[nodiscard]]
pn_error* _Nonnull pn_error_init(pn_error_code code, const char* _Nonnull format, ...);

/// Frees the
/// @param error provided
void pn_error_free(pn_error* _Nullable error);

/// A printable sentence for printf("%s", ...) and for test output.
/// @returns the @param error's own message when it has one, the
/// name of the code (like "range") when it has none, and "no error"
/// when the @param error is NULL.  Never NULL; the sentence stays
/// alive exactly as long as the @param error does, so read it before
/// freeing.
const char* _Nonnull pn_error_describe(const pn_error* _Nullable error);

// MARK: - Base 60

/*  Babylonian sexagesimal (base-60) values.
   A value is a string of "places", each holding a digit 0..59, most
   significant first.  The Babylonians (c. 1800 BCE onward) wrote these
   with two cuneiform signs and - crucially - had NO zero placeholder
   and NO radix point: 𒁹 (one vertical wedge) alone could mean 1, 60
   or 1/60; context decided.  See
   https://en.wikipedia.org/wiki/Sexagesimal and
   https://en.wikipedia.org/wiki/Babylonian_numerals .
   pn_base60 therefore carries an explicit radix position, a MODERN
   convention added so this type is a number and not a riddle:
   value = (-1)^neg * sum_i  places[i] * 60^(radix - 1 - i)
   In period style, use radix == n_places (a pure integer).
   The representation is a fixed-size array of bytes inside a ~20-byte
   struct passed by value: no allocations, safe for real-time and
   embedded callers.  double is offered only as a conversion utility,
   never the canonical form - many exact sexagesimal values (0;20 is
   1/3) have no finite binary expansion, and 60^9 already exceeds 2^53,
   beyond the exact range of a double.
*/

#define PN_BASE60_MAX_PLACES 16 /* 60^16 ~ 2.8e28: plenty; keeps the struct tiny */

typedef struct pn_base60 {
    uint8_t places[PN_BASE60_MAX_PLACES]; /* each 0..59, most significant first */
    uint8_t n_places;                    /* number of significant places, 1..16 */
    uint8_t radix;                       /* places left of the radix point
                                             (a modern addition - the Babylonians
                                             wrote none); 0 means purely fractional */
    bool neg;                            /* modern nicety; Old Babylonian texts
                                             carry no sign */
} pn_base60;

/*  Old Babylonian values survive in print as comma- or colon-separated
   places, e.g. the famous YBC 7289 diagonal:  1,24,51,10  (= about 1.41421,
   an excellent approximation of sqrt(2)).  Following Neugebauer, we let a
   semicolon mark the radix point - the convention for readings like
   1;24,51,10 - though no Babylonian tablet ever wrote one.
*/

/// Read a modern transcription of a sexagesimal value: an optional
/// '-', then one or more places of one or two decimal digits (each
/// 0 to 59) joined by ',' ':' or ';'.  At most one ';' may appear,
/// and it marks where the radix point sits:
///
///      "1,24,51,10"  -> places {1,24,51,10}, radix 4(period-style whole number)
///      "1;24,51,10"  -> the same places, radix 1(about 1.414...)
///      "0;45"        -> places {0,45}, radix 1(equals 3/4; the 0 is read literally)
///
/// The reading is strict: no spaces, no empty places, no trailing
/// separator.  One trap: an unbroken "3599" is NOT the decimal number
/// 3599 — it reads as the places 35 and 99, and 99 is above 59, so it
/// is refused.  Bring ordinary whole numbers in through
/// pn_base60_from_u64 instead.
///
/// @returns a pn_base60 number parsed from the
/// @param text provided, or a meaningless value and the
/// @param error encountered (read the note; a value-returning call
///     reports failure through the note, not through the value):
///     PN_ERROR_SYNTAX when the grammar is broken,
///     PN_ERROR_RANGE when a place tops 59, a place wears more than 2 digits,
///     or the value grows past PN_BASE60_MAX_PLACES places.
pn_base60 pn_base60_parse(const char* _Nonnull text, pn_error* _Nullable * _Nullable error);

/// @returns an allocated null-terminated `char*` formatted with the
/// @param value provided, and the
/// @param place_sep provided, or NULL and the
/// @param error encountered
[[nodiscard]]
char* _Nullable pn_base60_format(pn_base60 value, char place_sep,
                                               pn_error* _Nullable * _Nullable error);

/// Write the value back out as a transcription.  @param sep must be
/// ',' ':' or ';'.  Places are written with two digits — only the
/// first place drops its leading zero — joined with @param sep,
/// except the radix boundary, which always writes ';' when
/// 0 < radix < n_places:
///
///       {1,24,51,10} radix 1  with sep ','   ->     "1;24,51,10"
///       {2,23,3}     radix 3  with sep ','   ->     "2,23,03"
///
/// One caveat: with sep ';' and an interior radix the output carries
/// two ';' and will not parse back — use ',' or ':' when the string
/// has to survive the round trip.  A negative value grows a leading
/// '-'.  The write ends with a NUL and needs at most
/// PN_BASE60_FORMAT_MAX_LEN bytes.
///
/// On failure the provided buffer is left untouched.
///
/// @returns the caller's `char*` formatted with the
/// @param value to write out,
/// @param place_sep to join places, into the
/// @param buffer provided, with the
/// @param buflen provided; or NULL and the
/// @param error encountered
#define PN_BASE60_FORMAT_MAX_LEN (1 + 3 * PN_BASE60_MAX_PLACES + 1) /* '-' + places+seps + NUL */
char* _Nullable pn_base60_format_into(pn_base60 value, char place_sep, char* _Nonnull buffer,
                                      size_t buflen, pn_error* _Nullable * _Nullable error);

/// Turn an ordinary unsigned integer into sexagesimal places by
/// dividing by 60 again and again:
///
///        8583 -> places {2,23,3}, radix 3    (8583 = 2*60^2 + 23*60 + 3;
///                                           Wikipedia's worked example)
///        0     -> a single place {0}
/// @param n - the whole number to convert
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when the number would need
///     more than PN_BASE60_MAX_PLACES places (in practice unreachable:
///     2^64 needs only 11 places).
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
pn_base60* _Nullable pn_base60_from_u64(uint64_t n, pn_base60* _Nonnull out,
                                        pn_error* _Nullable* _Nullable error);

/// Turn a sexagesimal whole number back into an ordinary unsigned
/// integer with Horner's scheme (acc = acc*60 + place).  The value
/// must have radix == n_places, that is: no fraction.
/// @param v - the value to convert; it must be well-formed
/// @param out - where the number lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN when v is malformed or
///     carries any fractional place (a place above 59 counts as
///     malformed too), PN_ERROR_RANGE when the value does not fit a
///     uint64_t.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_base60_to_u64(pn_base60 v, uint64_t* _Nonnull out,
                                     pn_error* _Nullable* _Nullable error);

/// Turn a sexagesimal value into a double.  The fold runs
/// right-to-left, val = (val + place) / 60, so whole values stay
/// exact while the whole part fits inside 2^53 (radix <= 8, because
/// 60^8 ~ 1.7e14).
/// Like every jump to binary floating point this cannot keep every
/// sexagesimal value: 0;20 (which is 1/3) has no finite binary
/// expansion, and past 2^53 answers are correctly rounded yet not
/// exact.
/// @param v - the value to convert; it must be well-formed
/// @param out - where the double lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN when v is malformed.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
double* _Nullable pn_base60_to_double(pn_base60 v, double* _Nonnull out,
                                      pn_error* _Nullable* _Nullable error);

/// Turn a double into sexagesimal places with exactly
/// @param frac_places fractional places (0 to PN_BASE60_MAX_PLACES),
/// rounded half away from zero at the last kept place with carries
/// walking left.  Trailing zero places are KEPT, never trimmed, so
/// every answer is reproducible:
///
///        0.75 with 1 frac place  ->  {45}      radix 0    (0;45 = 3/4)
///        1.5  with 1 frac place  ->  {1,30}    radix 1    (1;30)
///
/// The Babylonians would have dropped trailing empty places — and
/// could afford to, since context told the reader the size anyway.
/// NaN and infinity are refused; see pn_base60_to_double for the
/// general exactness caveats.
/// @param x - the number to convert
/// @param frac_places - how many fractional places to keep
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN for NaN, infinity, or
///     frac_places above the maximum, PN_ERROR_RANGE when the whole
///     part or the rounded result needs more than
///     PN_BASE60_MAX_PLACES places.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
pn_base60* _Nullable pn_base60_from_double(double x, uint8_t frac_places, pn_base60* _Nonnull out,
                                           pn_error* _Nullable* _Nullable error);

// MARK: - Cuneiform Numbers

/*  The two positional signs of the Neo-Babylonian system, per the
   Unicode Standard names list:
   U+12079  CUNEIFORM SIGN DISH   𒁹   vertical wedge, value 1
   UTF-8: F0 92 81 B9
   U+1230B  CUNEIFORM SIGN U      𒌋   corner wedge,   value 10
   UTF-8: F0 92 8C 8B
   Both live in the main Cuneiform block (U+12000..U+123FF); the
   precomposed signs of the "Cuneiform Numbers and Punctuation" block
   (U+12400..) are metrology signs, not school-text positional digits,
   so we do NOT use them - each place is written as (d/10) copies of U
   followed by (d%10) copies of DISH, e.g. 23 = 𒌋𒌋𒁹𒁹𒁹.
   Unicode encodes NO zero placeholder for the sexagesimal gap.  An
   empty place is rendered as two spaces - faithfully reproducing the
   original place-value ambiguity: on the clay tablet a lone 𒁹 was 1 or
   60 or 1/60, and only context told the reader which.
*/

#define PN_CUNEIFORM_DISH 0x12079u /* 𒁹 vertical wedge = 1  */
#define PN_CUNEIFORM_U    0x1230Bu /* 𒌋 corner wedge    = 10 */

/*  write for a well-formed v.  Pure and total — it never fails; on a
   malformed v its answer is simply meaningless, since render rejects
*/
size_t pn_babylonian_render_size(pn_base60 v);

/// Write @param v in positional cuneiform style into @param buf as
/// UTF-8, ending with a NUL.  Each place becomes a group of (d/10) U
/// signs plus (d%10) DISH signs; groups sit one space apart; a place
/// of zero is the two-space gap the scribes left; a negative value
/// grows a leading '-', which is an anachronism.  Any well-formed
/// value renders — the caller decides which places are "really"
/// there.  No radix point is drawn (no tablet ever wore one), and
/// fractional places render exactly like whole ones.
/// @param v - the value to render
/// @param buf - where the cuneiform lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN when v is malformed
///     (n_places == 0 or above the max, a place past 59, or radix
///     past n_places), PN_ERROR_BUFFER when buflen does not beat
///     pn_babylonian_render_size(v).  On any failure buf is left
///     untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_babylonian_render(pn_base60 v, char* _Nonnull buf, size_t buflen,
                                     pn_error* _Nullable* _Nullable error);

// Parse a rendered cuneiform string - the inverse of
// pn_babylonian_render.  @param buf holds exactly @param buflen bytes
// of UTF-8 built from '-', the two signs, and separating spaces:
//
//          𒁹𒁹 𒌋𒌋𒁹𒁹𒁹 𒁹𒁹𒁹             ->    {2,23,3}
//
// A place group is (d/10) U signs then (d%10) DISH signs; groups join
// with one space, and the two-space gap of a zero place reads back as
// a zero place.  The spacing is strict - between signs it must run 1,
// 4, 7, … spaces (3k+1: joiner plus k gaps); leading and trailing
// runs are multiples of 3; an all-space string has length 3k-1.  Any
// other spacing, and any other byte, is malformed.  Round-trip law:
// pn_babylonian_parse_from(buf, strlen(buf), &w, &err) after a successful
// pn_babylonian_render(v, buf, …) recovers v's places exactly.
//
// The cuneiform itself carries no radix point ('-' notwithstanding),
// so the parsed value gets the period-style reading: radix ==
// n_places.  A lone 𒁹 parses as 1 - whether the scribe meant 60 or
// 1/60, only context knew.
/// @param buf - the rendered cuneiform to read
/// @param buflen - bytes to read at buf
/// @param out - where the value lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure:
///     PN_ERROR_SYNTAX for any foreign byte
///     or illegal spacing, including an empty string and a DISH
///     before a U in one group, PN_ERROR_RANGE for a group with more
///     than 5 U or 9 DISH signs, or for a value past
///     PN_BASE60_MAX_PLACES places.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
pn_base60* _Nullable pn_babylonian_parse_from(const char* _Nonnull buffer, size_t buflen,
                                              pn_base60* _Nonnull out,
                                              pn_error* _Nullable* _Nullable error);

// MARK: - Vigesimal

/*  Vigesimal — the base-20 core value: places, radix, conversions.
   The pn_base20 counterpart of pn_base60: a fixed-size array of
   bytes inside a ~20-byte struct passed by value - no allocations,
   safe for real-time and embedded callers.
   value = (-1)^neg * sum_i  places[i] * 20^(radix - 1 - i)
   places[i] runs 0..19, most significant first.  In period style,
   use radix == n_places.  The radix position and the sign are MODERN
   niceties, added so this type is a number and not a riddle: no
   book of the codices wrote a radix point, and the Dresden Codex
   carries no negative numbers.
   double is offered only as a conversion utility, never the canonical
   form: 0;5 in this notation is 1/4 and exact, but 0;1 is 1/20 and
   has no finite binary expansion (20 = 4 x 5 keeps a factor five),
   and 20^13 = 8.192e16 already exceeds 2^53 - one whole row later
   than sexagesimal's 60^8, because base twenty is half a square.
*/

#define PN_BASE20_MAX_PLACES 16 /* 20^16 ~ 6.6e20: plenty; keeps the struct tiny */

typedef struct pn_base20 {
    uint8_t places[PN_BASE20_MAX_PLACES]; /* each 0..19, most significant first */
    uint8_t n_places;                        /* number of significant places, 1..16 */
    uint8_t radix;                           /* places left of the radix point
	                                         (a modern addition - the Maya
	                                         wrote none); 0 means purely fractional */
    bool neg;                                /* modern nicety; the codices
	                                         carry no sign */
} pn_base20;

/// Read a modern transcription of a vigesimal value — the same
/// reading convention as the sexagesimal transcriptions above:
/// decimal places joined by ',' or ':', with at most one ';' marking
/// where the radix point sits.  No Maya scribe ever wrote any of it.
///
///        "1,2,0,0"   -> places {1,2,0,0}, radix 4  (period-style = 8800)
///        "1;12,9"    -> the same places, radix 1   (= 1 + 12/20 + 9/400)
///        "0;5"       -> places {0,5}, radix 1      (= 1/4; the 0 is
///                                                   read literally)
///
/// The reading is strict: no spaces, no empty places, no trailing
/// separator.  One trap: an unbroken "399" is NOT the decimal number
/// 399 — it reads three digits deep and is refused as out of range
/// (a longer run of digits is refused the same way).  Bring ordinary
/// whole numbers in through pn_base20_from_u64 instead.
/// @param text - the transcription to read
/// @param out - where the value lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure:
///     PN_ERROR_SYNTAX when the grammar is
///     broken, PN_ERROR_RANGE when a place tops 19, a place wears
///     more than 2 digits, or the value grows past
///     PN_BASE20_MAX_PLACES places.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
pn_base20* _Nullable pn_base20_parse(const char* _Nonnull text, pn_base20* _Nonnull out,
                                     pn_error* _Nullable* _Nullable error);

#define PN_BASE20_FORMAT_MAX_LEN \
    (1 + 3 * PN_BASE20_MAX_PLACES + 1) /* '-' + places+seps + NUL */

/// Write @param v out as a transcription joined with @param sep
/// (',' or ':'; the radix boundary itself writes ';' whenever the
/// radix sits in the middle of the value).  The write ends with a
/// NUL and stays within PN_BASE20_FORMAT_MAX_LEN bytes.
/// @param v - the value to write out
/// @param sep - the joiner between places
/// @param buf - where the text lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure:
///     PN_ERROR_SYNTAX when sep is not one of the
///     allowed joiners, PN_ERROR_DOMAIN when v is malformed
///     (n_places == 0 or past the max, a place above 19, radix past
///     n_places), PN_ERROR_BUFFER when buflen is too small.  On any
///     failure buf is left untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_base20_format(pn_base20 v, char sep, char* _Nonnull buf, size_t buflen,
                                 pn_error* _Nullable* _Nullable error);

/// Turn an ordinary unsigned integer into vigesimal places exactly
/// how the sexagesimal from_u64 does it, rebased: peel 20s off the
/// bottom and flip the order.   @param n == 0 becomes a single place
/// of 0.
/// @param n - the whole number to convert
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when the number would need
///     more than PN_BASE20_MAX_PLACES places (in practice
///     unreachable: UINT64_MAX takes 15 places).
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
pn_base20* _Nullable pn_base20_from_u64(uint64_t n, pn_base20* _Nonnull out,
                                        pn_error* _Nullable* _Nullable error);

/// Build a vigesimal whole number back up with Horner's scheme.
/// @param v - the value to convert; well-formed and whole
/// @param out - where the number lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN when v is malformed or its
///     radix is not n_places (that is: not a whole number),
///     PN_ERROR_RANGE when the value does not fit a uint64_t
///     (16 places maxed out = 20^16 - 1 would not).
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_base20_to_u64(pn_base20 v, uint64_t* _Nonnull out,
                                     pn_error* _Nullable* _Nullable error);

/// Turn a vigesimal value into a double: the whole part runs through
/// Horner's scheme in exact uint64_t arithmetic while it fits, then
/// one correctly-rounded division F / 20^k while k <= 14.  Results
/// are exact whenever a double can hold them at all.   (A value that
/// is purely fractional past 14 places drops to the iterated
/// division loop; harmless.)
/// @param v - the value to convert; it must be well-formed
/// @param out - where the double lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN when v is malformed.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
double* _Nullable pn_base20_to_double(pn_base20 v, double* _Nonnull out,
                                      pn_error* _Nullable* _Nullable error);

/// Turn a double into vigesimal places, rounding half away from zero
/// at the @param frac_places-th place (each place 0..19), just like
/// pn_base60_from_double: places kept past the rounding are zeros,
/// 0.98 rounded at one place steps up to a whole 1, NaN and infinity
/// are refused.
/// @param x - the number to convert
/// @param frac_places - how many fractional places to keep
/// @param out - where the value lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN for NaN, infinity, or
///     frac_places past PN_BASE20_MAX_PLACES, PN_ERROR_RANGE when
///     the carries grow the value past 16 places.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
pn_base20* _Nullable pn_base20_from_double(double x, uint8_t frac_places, pn_base20* _Nonnull out,
                                           pn_error* _Nullable* _Nullable error);

/*  Egyptian hieroglyphic numerals (UTF-8 rendering).
   The Egyptian system (c. 3000 BCE onward) was decimal but ADDITIVE: a
   distinct hieroglyph for each power of ten, repeated as many times as
   the digit demands - no positional notation at all.  See
   https://en.wikipedia.org/wiki/Egyptian_numerals .  The seven signs,
   per the Unicode Standard names list (all in the Egyptian Hieroglyphs
   block U+13000..U+1342F, all 4 bytes of UTF-8):
   U+133E4  EGYPTIAN HIEROGLYPH Z001   𓏤  single stroke,      value 1
   U+13386  EGYPTIAN HIEROGLYPH V020   𓎆  cattle hobble,        value 10
   U+13362  EGYPTIAN HIEROGLYPH V001   𓍢  coil of rope,         value 100
   U+131BC  EGYPTIAN HIEROGLYPH M012   𓆼  lotus,                value 1 000
   U+130AD  EGYPTIAN HIEROGLYPH D050   𓂭  bent finger,          value 10 000
   U+13190  EGYPTIAN HIEROGLYPH I008   𓆐  tadpole,              value 100 000
   U+13068  EGYPTIAN HIEROGLYPH C011   𓁨  Heh (god, arms raised), value 1 000 000
   There is NO sign above the Heh and NO zero: the scribe simply left a
   blank where our 0 sits, and a lone blank on an accounting ostracon
   meant "nothing owed".  So the renderable domain is the scribe's full
   set, 0..PN_EGYPTIAN_MAX_VALUE - every sign at most nine times - and
   zero renders as the empty string.  Values beyond the Heh (10 000 000
   would need ten Heh signs, 10^8 a hundred) are out of range, not
   because Egypt could not count them but because this rendering insists
   each sign be written out, and a nine-million-sign tablet fits no
   caller's buffer.
   Canonical form: signs grouped in DESCENDING powers of ten, one
   contiguous run per sign, with no separators - unlike cuneiform, the
   distinct signs are self-delimiting and there is no gap to preserve.
   4622 (the Karnak carving) is thus four lotus + six coils + two
   hobbles + two strokes, largest first, as any scribe would carve it.
*/

// MARK: - Egyptian Numerals

#define PN_EGYPTIAN_STROKE    0x133E4u   /* 𓏤 single stroke   = 1          */
#define PN_EGYPTIAN_HOBBLE    0x13386u   /* 𓎆 cattle hobble   = 10         */
#define PN_EGYPTIAN_COIL      0x13362u   /* 𓍢 coil of rope    = 100        */
#define PN_EGYPTIAN_LOTUS     0x131BCu   /* 𓆼 lotus           = 1 000      */
#define PN_EGYPTIAN_FINGER    0x130ADu   /* 𓂭 bent finger     = 10 000     */
#define PN_EGYPTIAN_TADPOLE   0x13190u   /* 𓆐 tadpole         = 100 000    */
#define PN_EGYPTIAN_HEH       0x13068u   /* 𓁨 Heh god         = 1 000 000  */
#define PN_EGYPTIAN_MAX_VALUE 9999999ull /* nine of each sign: the scribe's full set */

/// Count the bytes pn_egyptian_render will write for @param n, not
/// counting the NUL: four per sign drawn, which is 4 times the sum of
/// the decimal digits for @param n up to PN_EGYPTIAN_MAX_VALUE.  This
/// answer never fails; past the maximum the number is only
/// meaningless, because render turns down such values with a
/// PN_ERROR_RANGE note anyway.  @param n == 0 gives 0: the blank.
size_t pn_egyptian_render_size(uint64_t n);

/// Render @param n in hieroglyphic numerals into @param buf as UTF-8,
/// ending with a NUL: for each decimal digit d at 10^k, write d copies
/// of that power's sign, with the runs stepping down in power (10^6
/// first) and no separators between them.  @param n == 0 renders the
/// empty string — the faithful blank.  The buffer needs exactly
/// pn_egyptian_render_size(n) bytes plus the NUL.
/// @param buf - where the numeral is written; must not be NULL
/// @param buflen - bytes of room at buf; must exceed the render size
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when n is past the scribe's
///     full set, PN_ERROR_BUFFER when buflen is too small.  On any
///     failure buf is left untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_egyptian_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                   pn_error* _Nullable* _Nullable error);

/// Read back a rendered hieroglyphic string — the exact inverse of
/// pn_egyptian_render.  @param buf holds @param buflen bytes of UTF-8
/// built from the seven signs only, in canonical form: one unbroken
/// run per sign, the runs stepping down in power, each run holding 1
/// to 9 copies.  The value is the sum of what every sign is worth, so
/// the round trip is exact:  @param n = 0 (an empty string) reads back
/// as the nothing it was.  Any byte that is not a sign, a run that
/// climbs instead of steps down, or one sign split into two runs is
/// malformed — no sign was ever carved bottom-up on a temple wall.
/// @param buf - the string to read; NULL is refused with a note
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success; left untouched on
///     any failure
/// @param error - where to hang the note on failure:
///     PN_ERROR_SYNTAX for a foreign byte,
///     a cut-off sign, or runs out of order, PN_ERROR_RANGE for a run
///     of ten or more copies of one sign — a scribe would have
///     carried to the next power, so that string is no canonical
///     number.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_egyptian_parse(const char* _Nonnull buf, size_t buflen,
                                      uint64_t* _Nonnull out, pn_error* _Nullable* _Nullable error);

// MARK: - Hanzi Numerals

/*  Chinese hanzi numerals (UTF-8 rendering).
   Chinese numerals are decimal, but they write no bare place-value
   digits: the number is two sections around 万, value = A * 10^4 + B,
   and each section spells its nonzero digits high to low as
   (digit + 千/百/十), the section's units digit bare and last:
   四千六百二十二          = 4622      (Wikipedia's worked example)
   九千九百九十九万
   九千九百九十九            = 99 999 999  (the maximum)
   The signs, per the Unicode Standard names list (all in the CJK
   Unified Ideographs block, all 3 bytes of UTF-8):
   U+4E00 一 = 1      U+4E8C 二 = 2      U+4E09 三 = 3
   U+56DB 四 = 4      U+4E94 五 = 5      U+516D 六 = 6
   U+4E03 七 = 7      U+516B 八 = 8      U+4E5D 九 = 9
   U+5341 十 = 10     U+767E 百 = 100    U+5343 千 = 1000
   U+4E07 万 = 10 000
   Zero follows the Egyptian blank: a skipped digit is simply not
   written, and the skip is acknowledged by exactly one U+0020 space
   between the surrounding terms - 1005 is 一千 五 and 1050 is
   一千 五十, where later China would write 一千零五 and 一千零五十.
   The space never carries value, because each digit's place is
   already pinned by its multiplier (a bare digit is always its
   section's units digit), so the round trip stays exact - unlike
   the load-bearing gap of the counting rods below.
   Canonical form is the full formal reading; colloquial elisions are
   not canonical: 14 is 一十四, never the spoken 十四.  亿 (10^8) and
   traditional 萬 are outside this rendering - the domain stops at
   万, the same ruling as Egypt's missing sign above the Heh - so
   PN_HANZI_MAX_VALUE is 99 999 999, 九千九百九十九万九千九百九十九.
*/

#define PN_HANZI_MAX_VALUE 99999999ull /* nine 九 in every place of both sections */

/// Exact bytes (excluding the NUL) pn_hanzi_render will write for a
/// well-formed @param n: three per sign drawn — every digit, every
/// multiplier used, and the 万 — plus one for each blank space
/// acknowledging a skipped digit.  This answer never fails; for
/// @param n past PN_HANZI_MAX_VALUE it simply means nothing, since
/// render turns such values down.  render_size(0) == 0: the blank.
/// @param n - the number whose rendered size to measure
/// @returns the byte count render would write, NUL not counted.
size_t pn_hanzi_render_size(uint64_t n);

/// Write @param n (0 .. PN_HANZI_MAX_VALUE) as canonical hanzi into
/// @param buf as UTF-8, ending with a NUL: two sections split at
/// 万, each reading its live digits high to low with 千/百/十
/// multipliers, the units digit bare and last.  A skipped digit
/// writes nothing and earns exactly one blank space.    0 renders the
/// empty string.
/// @param n - the whole number to render
/// @param buf - where the string lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when n tops
///     PN_HANZI_MAX_VALUE (no sign above 万 lives here),
///     PN_ERROR_BUFFER when buflen does not beat
///     pn_hanzi_render_size(n).  On any failure buf is left
///     untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_hanzi_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read back a canonical hanzi string — the exact inverse of
/// pn_hanzi_render.    Strict canonical form: terms of
/// (digit [千|百|十]) or a bare digit, strictly descending in place
/// within each section, at most one 万 right after the top
/// section's last term, one space between terms if and only if they
/// sit more than one place apart.  Foreign bytes, a cut-off sign,
/// 十四 without its 一, an unearned space, or a missing blank all
/// give PN_ERROR_SYNTAX.    An empty string (buflen == 0) reads back
/// as 0.
/// @param buf - the string to read
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_hanzi_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error);

/*  Counting rod numerals (UTF-8 rendering).
   Rod numerals (算筹, in use c. 400 BCE onward in China, Japan,
   Korea and Vietnam) are true positional decimal: one bundle of
   rods per column, the columns alternating between two forms of the
   digits so that a lone rod can never be confused with its rotated
   twin.  The Unicode block encodes both forms (Supplementary
   Multilingual Plane, all 4 bytes of UTF-8):
   U+1D360..U+1D368  COUNTING ROD UNIT DIGIT ONE..NINE
   𝍠 𝍡 𝍢 𝍣 𝍤 𝍥 𝍦 𝍧 𝍨
   U+1D369..U+1D371  COUNTING ROD TENS DIGIT ONE..NINE
   𝍩 𝍪 𝍫 𝍬 𝍭 𝍮 𝍯 𝍰 𝍱
   The units column takes the unit set, the tens column the tens set,
   the hundreds the unit set again, and so on - the Song-dynasty
   orientation, which is also what the Unicode names assume.
   The rods had NO zero - an empty column was literally left empty,
   and the reader counted columns to know each place - so the blank
   is rendered as space here, the cuneiform gap generalised to base
   ten: every place boundary contributes exactly one space, and a
   blank place contributes one of its own.  A well-formed string
   therefore holds exactly (places − 1) spaces, which is what makes
   the faithful blank round-trip:
   𝍪 𝍢       = 23        (adjacent places: one joiner)
   𝍠  𝍠      = 101       (the blank tens place: joiner plus one)
   𝍠            = 100       (𝍠 plus two blanks, not the lone 1)
   A column's form must match the parity of its place: writing 11 as
   𝍠 𝍠 instead of 𝍩 𝍠 is the rod equivalent of a DISH before
   a U, and pn_rods_parse rejects it.  There is no sign of negative
   values (the red/black rods of later Japanese counting are outside
   this rendering), and modern transcriptions' 〇 U+3007 is not
   written - a zero is a blank, as on the counting board.
*/

// MARK: - Rod Numerals

#define PN_ROD_UNIT_DIGIT_ONE 0x1D360u               /* …through U+1D368 (digit d: +d-1)  */
#define PN_ROD_TENS_DIGIT_ONE 0x1D369u               /* …through U+1D371 (digit d: +d-1) */
#define PN_ROD_MAX_PLACES     19                     /* 10^19 − 1 still fits uint64_t     */
#define PN_ROD_MAX_VALUE      9999999999999999999ull /* 10^19 − 1: nineteen nines   */

/*  for a well-formed n: four per rod drawn, plus one space per place
   boundary - i.e. 4*(count of nonzero digits) + (count of digits − 1).
   Pure and total - it never fails; for n > PN_ROD_MAX_VALUE its answer
   is simply meaningless, since render rejects such values with
*/
size_t pn_rods_render_size(uint64_t n);

/// Write @param n as rod numerals into @param buf as UTF-8, ending
/// with a NUL: one rod glyph per nonzero digit (unit set at even
/// places, tens set at odd ones), most significant first.  Each
/// place boundary from the top place down to the units writes
/// exactly one space — it joins columns that touch and stands as the
/// blank for columns that are empty (trailing blanks included).
/// @param n == 0 renders the empty string; the write needs exactly
/// pn_rods_render_size(@param n) bytes plus the NUL.  The old error
/// block above says which notes come back; kept here in short:
/// PN_ERROR_RANGE when n tops
/// PN_ROD_MAX_VALUE, PN_ERROR_BUFFER when buflen is smaller than the
/// exact byte count — and on any failure buf is left untouched.
/// @param n - the whole number to render
/// @param buf - where the board lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_rods_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                               pn_error* _Nullable* _Nullable error);

/// Read back a rendered rod string — the exact inverse of
/// pn_rods_render.    The string is just the two rod sets and spaces,
/// in canonical spacing: it never starts with a space, it holds at
/// least one glyph, and the spaces in it count exactly one less than
/// the number of places the value needs.  The parser sets places =
/// spaces + 1, reads the leftmost glyph at place places - 1, and each
/// run of r spaces moves the place along by r:
///
///     rod-nine space rod-three ->  23
///     rod-one  two spaces rod-one -> 101   (the blank tens column)
///     rod-tens-one        ->     10        (the blank units column)
///
/// The round trip is exact: parsing the string after a successful
/// render recovers n.    An empty buffer (buflen == 0) reads back
/// as 0.  PN_ERROR_SYNTAX for
/// a leading space, all spaces, two glyphs with no space between, a
/// glyph whose set argues with its place's parity, any other byte, or
/// a sign cut off short; PN_ERROR_RANGE for more than
/// PN_ROD_MAX_PLACES places (spaces + 1 > 19).  On any failure *out
/// is left untouched.
/// @param buf - the board to read
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_rods_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                  pn_error* _Nullable* _Nullable error);

/*  Indian Brahmi numerals (UTF-8 rendering).
   Brahmi numerals (attested from the 3rd century BCE, on Ashoka's edicts
   and the coins of the Western Satraps) are the direct graphic ancestors
   of the modern Hindu-Arabic digits - but conceptually a different
   system: a non-positional decimal notation with NO zero.  See
   https://en.wikipedia.org/wiki/Brahmi_numerals .  The twenty signs, per
   the Unicode Standard names list (all in the Brahmi block
   U+11000..U+1107F, all 4 bytes of UTF-8):
   U+11052..U+1105A  BRAHMI NUMBER ONE..NINE      𑁒 𑁓 𑁔 𑁕 𑁖 𑁗 𑁘 𑁙 𑁚
   U+1105B..U+11063  BRAHMI NUMBER TEN..NINETY    𑁛 𑁜 𑁝 𑁞 𑁟 𑁠 𑁡 𑁢 𑁣
   U+11064           BRAHMI NUMBER ONE HUNDRED    𑁤
   U+11065           BRAHMI NUMBER ONE THOUSAND   𑁥
   Unlike Egypt, no digit is written by repetition: every unit 1..9 and
   every ten 10..90 has its own dedicated sign, so 47 needs two signs,
   not eleven.  There is NO sign above the thousand (the Jain infinities
   counted in words, not signs) and NO zero - a lone blank is the
   nothing owed - so the renderable domain is 0..PN_BRAHMI_MAX_VALUE
   and zero renders as the empty string, the scribe's blank again.
   Hundreds and thousands are MULTIPLICATIVE, per Unicode 14.x: a digit
   2..9 ligatured with the power sign - the power first, then the
   U+1107F BRAHMI NUMBER JOINER, then the multiplier - so 400 is
   <𑁤 𑁿 𑁕> (100 JOINER 4) rendered as one stacked ligature, while an
   additive component stays unligated: 104 is <𑁤 𑁕> (100 4).  The joiner
   marks that semantic distinction, so it is never dropped, never
   doubled, and legal only between number signs - 1 × 100 is written
   with the bare power sign, as 𑁥𑁥 is no number any more than 𓏤..𓏤 ten
   strokes were.
   Canonical form: up to four terms descending - thousands, hundreds,
   tens, ones - each at most once, with no separators; a signs table
   and the four slots make every string self-delimiting:
   𑁤𑁿𑁓𑁟𑁗  = 256   (the Sasaram edict: 200 ligature + fifty + six)
   𑁤𑁟𑁔         = 153   (the Damasena coin: 100-50-3)
   𑁥𑁿𑁚𑁤𑁿𑁚𑁣𑁚 = 9999  (the full set; 𑁚 appears three times, in three
   slots - slots, not sign identity, rule)
*/

// MARK: - Brahmi Numerals

#define PN_BRAHMI_NUMBER_ONE      0x11052u /* 𑁒 …through U+1105A NINE  = 1..9 */
#define PN_BRAHMI_NUMBER_TEN      0x1105Bu /* 𑁛 …through U+11063 NINETY = 10..90 */
#define PN_BRAHMI_NUMBER_HUNDRED  0x11064u /* 𑁤 one hundred                 */
#define PN_BRAHMI_NUMBER_THOUSAND 0x11065u /* 𑁥 one thousand                */
#define PN_BRAHMI_NUMBER_JOINER   0x1107Fu /* 𑁿 ligature join, numbers only */
#define PN_BRAHMI_MAX_VALUE       9999u    /* four filled slots: no sign above 𑁥 */

/*  write for a well-formed n: four per sign drawn, joiner included —
   a thousands or hundreds term costs four bytes with its digit 1
   (bare power sign) and twelve with its digit 2..9 (power, joiner,
   multiplier), a tens or ones term four bytes when present.  Pure and
   total - it never fails; for n > PN_BRAHMI_MAX_VALUE its answer is
   simply meaningless, since render rejects such values with
*/
size_t pn_brahmi_render_size(uint64_t n);

/// Write @param n as Brahmi numerals into @param buf as UTF-8,
/// ending with a NUL: the terms climb down thousands, hundreds,
/// tens, ones.  Each nonzero digit takes its own dedicated sign
/// (1..9, or the tens signs for the tens digit), and a hundreds or
/// thousands digit ligates: d == 1 is the bare power sign, d == 2..9
/// is the power sign, then U+1107F the joiner, then the units sign
/// for d.  @param n == 0 renders the empty string — the blank.  The
/// write needs exactly pn_brahmi_render_size(@param n) bytes plus
/// the NUL.  PN_ERROR_RANGE when n
/// tops PN_BRAHMI_MAX_VALUE, PN_ERROR_BUFFER when buflen is smaller
/// — and on any failure buf is left untouched.
/// @param n - the whole number to render
/// @param buf - where the signs land, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_brahmi_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                 pn_error* _Nullable* _Nullable error);

/// Read back a rendered Brahmi string — the exact inverse of
/// pn_brahmi_render.    Strict canonical form: the twenty number
/// signs and the joiner, in at most four terms climbing down —
/// thousands, hundreds, tens, ones — each term at most once, with
/// nothing between them:
///
///     hundred join two fifty six ->   256   (two hundreds ligatured, then 50, 6)
///     hundred four               ->   104   (additive: 100 then 4)
///     thousand one               ->  1001   (the thousand then a bare one)
///
/// The round trip is exact: parsing the string after a successful
/// render recovers n.    An empty buffer (buflen == 0) reads back as
/// 0 — the blank reads back as the nothing it was.  The grammar lets
/// in no value past PN_BRAHMI_MAX_VALUE, so overflow needs no check.
/// Everything off the path gives PN_ERROR_SYNTAX: a second term of
/// one slot, an ascending term, a joiner doubled, leading, trailing,
/// or joining anything but a multiplier after its power sign, a
/// ligatured multiplier of 1 (write the power sign bare), a ligature
/// left unligated ("hundred four four" would read 100+4+4 and no slot
/// allows the second four), any other byte, or a sign cut off short.
/// On any failure *out is left untouched.
/// @param buf - the string to read
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_brahmi_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                    pn_error* _Nullable* _Nullable error);

/*  Mayan bar-and-dot numerals over pn_base20 — the renderer.
   The Maya had what no other ancient place-value script here can
   claim, and what Unicode's cuneiform block pointedly lacks: a real
   zero.  Dots for units, bars for fives, a shell for nought, and the
   block composites them into one sign per place 0..19:
   U+1D2E0..U+1D2E4   MAYAN NUMERAL ZERO..FOUR      shell, dots   = 0..4
   U+1D2E5..U+1D2E9   MAYAN NUMERAL FIVE..NINE      bars + dots   = 5..9
   U+1D2EA..U+1D2EE   MAYAN NUMERAL TEN..FOURTEEN   two bars      = 10..14
   U+1D2EF..U+1D2F3   MAYAN NUMERAL FIFTEEN..NINETEEN three bars  = 15..19
   (Unicode block Mayan Numerals, U+1D2E0..U+1D2F3; Unicode 10.0, 2017
   after proposal L2/16-264, Jameson Quinn.  All twenty are SMP: four
   bytes of UTF-8 each under the stem F0 9D 8B.)
   The codex stacks places vertically, least significant at the
   bottom; we transcribe the stack top-down made left-to-right -
   most significant place first, one space per place boundary, exactly
   the linearisation the rod columns get:
   \t *      𝋡 𝋣   ->   23     (three dots under one dot: 1x20 + 3)
   \t *      𝋡 𝋭   ->   33     (dot above three dots atop two bars)
   \t *      𝋡 𝋡 𝋩   ->   429     (dot dot nine: 1x400 + 1x20 + 9)
   \t *      𝋡 𝋢 𝋠 𝋠   ->   8800     (the shells hold rows three and four)
   \t *      𝋠   ->   0     (the shell itself: no blank needed)
   Each place carries exactly ONE precomposed sign, so a zero place
   writes its shell instead of the Babylonian's two-space gap: the
   string is self-counting, no context tells a reader where the rows
   are.  pn_mayan_render draws every place that well_formed allows,
   including fractional ones (no radix point is carved; the '-' is an
   anachronism, as in the cuneiform renderer above), and pn_mayan_parse
   answers period style: radix == n_places.
   References: https://en.wikipedia.org/wiki/Maya_numerals ,
   https://en.wikipedia.org/wiki/Vigesimal
*/

// MARK: - Mayan Numerals

#define PN_MAYAN_DIGIT_ZERO 0x1D2E0u /* SHELL …through U+1D2F3 (digit d: +d) */

/*  the terminating NUL: one sign per place and one space per place
   boundary, plus a '-' when neg.  Pure and total - it never fails; on
   a malformed v its answer is simply meaningless, since render rejects
*/
size_t pn_mayan_render_size(pn_base20 v);

/// Write @param v as Maya bar-and-dot numerals into @param buf as
/// UTF-8, ending with a NUL: one sign per place, one space per
/// boundary.  A zero place writes the shell, not a gap.
/// @param v - the value to render
/// @param buf - where the signs land, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure:
///     PN_ERROR_DOMAIN when v is malformed
///     (n_places == 0 or past the max, a place above 19, radix past
///     n_places), PN_ERROR_BUFFER when buflen does not beat
///     pn_mayan_render_size(v).  On any failure buf is left
///     untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_mayan_render(pn_base20 v, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read back a rendered bar-and-dot string — the exact inverse of
/// pn_mayan_render.   ,@param buf holds @param buflen bytes of UTF-8
/// using '-', the twenty signs, and single joining spaces; the
/// grammar is '-? (sign ' ')* sign' and it is strict: a leading,
/// trailing or doubled space, two signs touching inside one row, or
/// any foreign byte (the block's void past U+1D2F3 included —
/// U+1D2F4..1D2FF carry no sign and never parse), or a sign cut off
/// in the middle of its UTF-8 is PN_ERROR_SYNTAX.  More rows than
/// PN_BASE20_MAX_PLACES is the page overrun — PN_ERROR_RANGE, the
/// ruling the scribe's fifteenth row gets — not bad grammar.  An
/// empty string (buflen == 0) is syntax, not zero: the shell is
/// zero's one true spelling, the way an empty tablet is no number.
/// The round trip law holds: parsing buf after a successful render
/// recovers v's places exactly (the parsed radix == n_places: period
/// style — a carved stack carries no point a reader could place
/// back).
/// @param buf - the string to read
/// @param buflen - bytes to read at buf
/// @param out - where the value lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
pn_base20* _Nullable pn_mayan_parse(const char* _Nonnull buf, size_t buflen,
                                    pn_base20* _Nonnull out, pn_error* _Nullable* _Nullable error);

/*  Inca quipu — positional decimal knot notation, transcribed in UTF-8.
   The quipucamayuq tied numbers onto pendant cords hanging from a
   main cord: base ten by POSITION along the cord, the units line
   nearest the free end and each higher place marching up toward the
   main cord (L. Leland Locke established the place value in 1912;
   Ascher & Ascher read the archives).  Our transcription lays the
   pendant down, most significant place first - the linearisation the
   counting-board rods get in rods.c:
   Unicode encodes NO quipu signs - there is no knot block to borrow,
   unlike the cuneiform wedges of babylonian.c - so the cord is
   transcribed with two generic glyphs:
   U+2022 •  BULLET     one simple knot          (Locke's s): d of
   them mark digit d in every line
   above the units
   U+221E ∞  INFINITY   the figure-eight knot (Locke's E) at the
   units line, and d of them in a row
   the long knot of d turns (d L)
   Both are BMP: three bytes of UTF-8 each, stems E2 80 A2 and
   E2 88 9E.  In Locke's own shorthand, the article's worked cords:
   ••••••• ••• ∞      ->      731      (7s, 3s, E)
   ••••••••  ∞∞∞∞      ->      804      (8s, X, 4L: the blank tens keeps two boundaries)
   • •••• ••••••••• ∞∞∞      ->      1493      (1s, 4s, 9s, 3L)
   •       ->      10      (one knot over the blank units)
   ∞      ->      1      (figure-eight: units never say bullet)
   ->      0      (the empty cord: no knot tied)
   The Inca zero was the ABSENCE of a knot in the right position - the
   truest blank in this library, kept literally: one space per place
   boundary and a blank place writes no sign of its own, so a
   well-formed cord holds exactly (places − 1) spaces and the parser
   reads places = spaces + 1 off the cord unaided.  The figure-eight
   is the reader's anchor - it lives at the units line and nowhere
   else, as good as the distinctive units knot on the real cord - so
   ∞ above the units line is no cord, and a lone simple knot is no
   number (the tens digit of 10 hangs over the blank units with its
   boundary space kept).
   One cord, one number: color, ply, S/Z twist, subsidiary cords and
   the chroniclers' multi-number pendants are not the number system,
   and no transcription pretends at them.
   References: https://en.wikipedia.org/wiki/Quipu ,
   Locke, The Ancient Quipu or Peruvian Knot Record (1912);
   Ascher & Ascher, Mathematics of the Incas: Code of the Quipu (1997).
*/

// MARK: - Quipu Numerals

#define PN_QUIPU_SIMPLE_KNOT 0x2022u                /*   BULLET  UTF-8: E2 80 A2         */
#define PN_QUIPU_LONG_KNOT   0x221Eu                /* ∞ INFINITY UTF-8: E2 88 9E         */
#define PN_QUIPU_MAX_PLACES  19                     /* 10^19 − 1 still fits uint64_t     */
#define PN_QUIPU_MAX_VALUE   9999999999999999999ull /* nineteen nines               */

/*  terminating NUL.  Pure and total — it never fails; for n >
   PN_QUIPU_MAX_VALUE its answer simply runs on and is meaningless,
*/
size_t pn_quipu_render_size(uint64_t n);

/// Write @param n (0 .. PN_QUIPU_MAX_VALUE) as a quipu cord into
/// @param buf as UTF-8, ending with a NUL.    Each place gets one
/// column of knots: @param n's digit d for that place shown as d
/// bullets above the units line, or d infinities right at it (the
/// long knot's turns; the figure-eight is its first turn).  A blank
/// place writes no knots but keeps its boundary space, and 0 renders
/// the empty cord — the Inca zero is no knot tied.
/// @param n - the whole number to render
/// @param buf - where the cord lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when n tops PN_QUIPU_MAX_VALUE
///      (a twentieth line no pendant holds), PN_ERROR_BUFFER when
///     buflen does not beat pn_quipu_render_size(n).  On any failure
///     buf is left untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_quipu_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read back a rendered cord — the exact inverse of
/// pn_quipu_render.    The reading demands the canonical form:
/// places = spaces + 1, the leftmost column sitting at place
/// places - 1 and each space stepping one place down; every column a
/// run of one to nine IDENTICAL knots — bullets in the lines above
/// the units, infinities at the units line alone (the figure-eight
/// is where the cord ends).  A lone bullet at the units line, an
/// infinity anywhere above it, a column that mixes families, a
/// leading or trailing space, columns touching with no boundary, any
/// foreign byte (the chronicler's minus included), or a knot cut off
/// in the middle of its UTF-8 — all give PN_ERROR_SYNTAX.
/// Ten identical knots in one column, or more than
/// PN_QUIPU_MAX_PLACES places, is more turns and more cord than a
/// digit and a uint64_t hold: PN_ERROR_RANGE, the ruling the
/// scribe's twentieth row gets.  An empty cord (buflen == 0) reads
/// back as 0 — the blank reads back as the nothing no knot was.
/// @param buf - the cord to read
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_quipu_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error);

/*  Greek alphabetic numerals — the Milesian (Ionic) system, in UTF-8.
   No place values, no zero: the market and the chancery took the
   alphabet itself as the digit set - the first three letters of each
   row are its unit: α (1), ι (10), ρ (100) - and filled the gaps with
   three letters the classical alphabet had lost but the numerals
   kept: ϛ stigma (6), ϟ koppa (90), ϡ sampi (900).  Above 999 the
   thousands get their own run of the same signs, announced by the
   low keraia ͵ (U+0375) in front of them, and the whole number wears
   the trailing keraia ʹ (U+0374) behind it - the horn-mark of the
   stenographers, a keraia being to Greek numerals what the decimal
   point is to ours: the reader's proof that a number is being read.
   So every nonempty rendering carries exactly one ʹ at the end, and
   the parser demands it where the culture did.
   A spelling is a descending SCALE RUN - at most one sign from the
   hundreds row ρ σ τ υ φ χ ψ ω ϡ, then the tens row ι κ λ μ ν ξ ο π
   ϟ, then the units row α β γ δ ε ϛ ζ η θ - nothing backward, no
   sign twice: 999 is never ροο and 150 is never νρ, for exactly the
   reason no scribe stacked nine 𓏥 coils.  A legal run is the ONLY
   run spelling its value: the rows are worth 100/10/1, so a run is
   decimal notation in letters and the descent is forced.
   Above 999 the cord is two runs under one low keraia - thousands
   first, then units - and the culture let them butt together with
   nothing between: ͵ιαʹ then reads BOTH 11000 (ια = 11 thousands)
   and 10001 (ι = 10 thousands, α = 1 unit); an exhaustive sweep of
   the domain counts 972 of those shared cords, so a number whose
   thousands run AND units run are both live separates them with ONE
   space (the stenographers' own gutter): 10001 renders ͵ι␣αʹ and
   11000 renders ͵ιαʹ, and the space, like the quipu's, is part of
   the canonical cord.  Whole thousands carry no space (nothing to
   separate); numbers below 1000 carry no low keraia at all.  Two
   runs may share a letter - 1001 is ͵α␣αʹ - the space, not
   uniqueness of letters, is what tells the runs apart.  The
   article's table, worked through this transcription:
   αʹ           ->    1
   ιθʹ          ->    19
   σνϛʹ         ->    256
   χϛʹ          ->    606
   ϡϟθʹ         ->    999
   ͵αʹ          ->    1000
   ͵α␣αʹ        ->    1001
   ͵β␣ιθʹ       ->    2019
   ͵ι␣αʹ        ->    10001
   ͵ιαʹ         ->    11000
   ͵ϡϟθ␣ϡϟθʹ    ->    999999
   (999999 = 999 thousands over 999 units - the culture wrote no
   millions in these signs, and PN_GREEK_MAX_VALUE stops where the
   alphabet does.)
   The majuscule of printed editions (͵Β␣ΙΘʹ for 2019) is typography,
   not notation: this module renders and parses lowercase only, and
   capitals are syntax errors, like every foreign letter - Roman I V
   X among them (the Roman numerals keep their own block U+2160.. and
   will get their own module).
   One trap before normalising these cords: the trailing keraia
   U+0374 is NFC-UNSTABLE - NFD unfolds it to U+02B9 MODIFIER LETTER
   PRIME and NFC never brings it back (every cord of this module,
   1..999999, falls to that trap; the low keraia U+0375 is stable).
   The module writes the canonical CD B4 bytes and accepts nothing
   else: the prime, the tonos ΄ U+0384 and the dakia are near-miss
   impostors - syntax errors, like the Mayan dot on a quipu cord.
   6/90/900 take the archaic ϛ ϟ ϡ (U+03DB, U+03DF, U+03E1, Greek-and-
   Coptic extensions, NFC-stable); 200 takes plain σ U+03C3 - the
   lunate ϲ of Byzantine hands and the final ς of living Greek are
   BOTH refused, the final sigma being a sandhi rule of the language,
   not a numeral sign, and ϲ U+03F2 colliding with san.
   Zero had no glyph (the culture counted what was there); like the
   Egyptian scribe's blank, the rods' empty column and the unstrung
   quipu cord, n == 0 renders the empty string and buflen == 0 reads
   back as 0 - this section follows the uint64-direct rule book.
   References: https://en.wikipedia.org/wiki/Greek_numerals ,
   https://en.wikipedia.org/wiki/Keraia
*/

// MARK: - Greek Numerals

#define PN_GREEK_MAX_VALUE \
    999999ull /* two full runs, 999 of 1000 over
                                        999 - written millions needed
                                        fresh marks no standard survived */

/*  write: TWO per sign — every sign is a 2-byte BMP code point, both
   keraia included - plus ONE for the run-separating space when both
   runs are live (n >= 1000 with units above it).  n == 0 renders the
*/
size_t pn_greek_render_size(uint64_t n);

/// Write @param n (1 .. PN_GREEK_MAX_VALUE) as a Milesian cord into
/// @param buf as UTF-8, ending with a NUL: low keraia and thousands
/// run when @param n has them, one separating space when both runs
/// are alive, the units run, always the trailing keraia.     0 renders
/// the empty cord.
/// @param n - the whole number to render
/// @param buf - where the cord lands, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when n tops
///     PN_GREEK_MAX_VALUE, PN_ERROR_BUFFER when buflen cannot hold
///     pn_greek_render_size(n) plus the NUL.  On any failure buf is
///     left untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_greek_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read back a rendered cord — the exact inverse of
/// pn_greek_render.    The reading is strict about the canonical
/// form: the trailing keraia at the very end and nowhere else, the
/// low keraia at the very front at most once, at most one separating
/// space, never touching an edge, and only under the low keraia.
/// Each run walks its rows strictly downward — at most one sign of
/// the hundreds row, then one of the tens, then one of the units.
/// A code point with no table entry (capitals, the lunate sigma, the
/// final sigma, the digamma, the tonos, the prime twin of the dexia,
/// any ASCII byte including an embedded NUL, a byte sequence whose
/// UTF-8 shape is wrong) is PN_ERROR_SYNTAX — the culture's own
/// verdict on a misspelled number.  An empty cord (buflen == 0)
/// reads back as 0.
/// @param buf - the cord to read
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_greek_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error);

/*  Roman numerals — the canonical form, in ordinary ASCII letters.
   No zero, no place values: the ledger kept seven capital letters -
   I (1), V (5), X (10), L (50), C (100), D (500), M (1000) - and the
   subtractive habits of the Middle Ages: a unit written BEFORE the
   sign it undershoots stands for the gap - IV is 5 less 1, IX is 10
   less 1.  The grammar this module renders and reads is the tallied
   canonical form - the per-digit rows of thousands, hundreds, tens
   and units, always in full: 3888 is MMMDCCCLXXXVIII, the longest
   numeral there is, and 1999 is MCMXCIX - never the shortcuts the
   clocks and ink allowed (IIII for 4, IC for 99, VX, and the
   overlined vincula that meant 5000 would be other cultures'
   invention; the abacist editors settled on these letters alone).
   A canonical numeral is the ONLY spelling of its value: the rows
   descend, each run of I, X, C or M holds at most three, the six
   subtractive digraphs IV IX XL XC CD CM appear at most once and
   belong to their digit - that descent is forced, the same way the
   Greek run's is, so a numeral reads back exactly one number.  The
   culture's own proofs agree: the same year is MCMXCIX in every
   inscription that spells it full.
   The article's table, worked through this transcription:
   I                ->   1
   IV               ->   4
   V                ->   5
   IX               ->   9
   XIV              ->   14
   XLIV             ->   44
   XC               ->   90
   CD               ->   400
   CM               ->   900
   M                ->   1000
   MCMXCIX          ->   1999
   MMMDCCCLXXXVIII  ->   3888
   MMMCMXCIX        ->   3999
   (3999 = MMM | CM | XC | IX, the ceiling; the Romans wrote larger
   with overlines and fresh marks no standard survived.)
   Zero had no letter (the ledger counted what was there - Bede's
   abbey later wrote nulla, the nothing, in its place); like the
   Egyptian blank, the rods' gap, the unstrung cord and the silent
   keraia, n == 0 renders the empty string and buflen == 0 reads
   back as 0 - this section follows the uint64-direct rule book.
   Lowercase letters, Greek, whacked-out spaced variants and every
   foreign byte are syntax errors - a numeral in the abacist's sense
   is capital ASCII letters only.
   References: https://en.wikipedia.org/wiki/Roman_numerals
*/

// MARK: - Roman Numerals

#define PN_ROMAN_MAX_VALUE \
    3999ull /* MMM CM XC IX: three thousands and
                                      the alphabet's full hundreds-tens-
                                      units row - vinculum numbers had no
                                      standard to keep them */

#define PN_ROMAN_FORMAT_MAX_LEN \
    16 /* 15 letters - the canon of 3888 -
                                      plus the NUL                   */

/*  write: the letters of the four per-digit rows, at most four per
*/
size_t pn_roman_render_size(uint64_t n);

/// Write @param n (1 .. PN_ROMAN_MAX_VALUE) as canonical Roman
/// letters into @param buf, ending with a NUL: the thousands run,
/// the hundreds row, the tens row, the units row — subtractive
/// digraphs where the canon puts them (IV IX XL XC CD CM, once per
/// digit).    0 renders the empty string.
/// @param n - the whole number to render
/// @param buf - where the letters land, with room for the NUL
/// @param buflen - bytes of room at buf
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when n tops
///     PN_ROMAN_MAX_VALUE, PN_ERROR_BUFFER when buflen cannot hold
///     pn_roman_render_size(n) plus the NUL.  On any failure buf is
///     left untouched.
/// @returns the caller's `buf` on success, or NULL and the
/// @param error encountered.
char* _Nullable pn_roman_render(uint64_t n, char* _Nonnull buf, size_t buflen,
                                pn_error* _Nullable* _Nullable error);

/// Read canonical Roman letters back — the exact inverse of
/// pn_roman_render.    The walk is greedy over the canon table, and
/// whatever it accepts must render back to itself byte for byte:
/// IIII, IL, VX, XCX, MDCCCCLXXXXVIIII, lowercase, spaces, embedded
/// NULs and any non-ASCII byte all fail with PN_ERROR_SYNTAX.  An
/// empty string (buflen == 0) reads back as 0.
/// @param buf - the letters to read
/// @param buflen - bytes to read at buf
/// @param out - where the number lands on success; untouched on any
///     failure
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_roman_parse(const char* _Nonnull buf, size_t buflen, uint64_t* _Nonnull out,
                                   pn_error* _Nullable* _Nullable error);

/*  Early mathematical theory (Elements VII-IX, the sieve of
   Eratosthenes, the gnomon figures).
   The reckoning that sat underneath the numerals: Euclid's ladder
   of remainders, the papyrus of struck rows, the Pythagoreans'
   triples, the abacist's ledger of factors.  Nothing here renders a
   notation - these are the working functions of the culture, on the
   whole uint64 ledger 0..18446744073709551615.
   Primality is proved, never merely probable: below 2^32 Euclid's
   own trial division by the sieve's unstruck rows is the complete
   argument, and above it Miller's seven certified witnesses (2,
   325, 9375, 28178, 450775, 9780504, 1795265022) decide every case
   of the ledger before the water clock runs out.
*/

// MARK: - Math

#define PN_MATH_SIEVE_MAX_VALUE 1000000ull
#define PN_MATH_M_MAX_VALUE     65535ull
#define PN_MATH_SIDE_MAX_VALUE  4294967295ull
#define PN_MATH_MAX_FACTORS     64u

size_t pn_math_sieve_size(uint64_t limit);

/// Lay the sieve board in @param out in ascending order and report
/// the rows it set through @param count.  PN_ERROR_RANGE when @param limit passes
/// PN_MATH_SIEVE_MAX_VALUE, PN_ERROR_BUFFER when @param cap falls
/// short of the count — and on any failure the caller's rows and
/// @param count keep their word.
/// @param limit - strike the primes from 2 to this
/// @param out - where the primes land, @param cap slots of room
/// @param cap - slots available at @param out
/// @param count - how many primes were written, on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_math_sieve(uint64_t limit, uint64_t* _Nonnull out, size_t cap,
                                  size_t* _Nonnull count, pn_error* _Nullable* _Nullable error);

/// Euclid's ladder (Elements VII.1-2): the greatest common measure
/// of @param a and @param b.  gcd(1071, 462) = 21; gcd(0, 0) = 0 by
/// the abacist's convention.
/// @param a - one number
/// @param b - the other number
/// @param out - where the answer lands on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_math_gcd(uint64_t a, uint64_t b, uint64_t* _Nonnull out,
                                pn_error* _Nullable* _Nullable error);

/// The least common multiple: the smallest number both @param a and
/// @param b measure.  lcm(1071, 462) = 23562; a blank row makes the
/// ratio blank — lcm(0, n) = 0.
/// PN_ERROR_RANGE when the product leaves the ledger.
/// @param a - one number
/// @param b - the other number
/// @param out - where the answer lands on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_math_lcm(uint64_t a, uint64_t b, uint64_t* _Nonnull out,
                                pn_error* _Nullable* _Nullable error);

/// Is @param n a square number?  The Pythagoreans' gnomon figures:
/// is_square(999950884) is true, the square of 31622.
/// @param n - the number to weigh
/// @param out - where the verdict lands on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
bool* _Nullable pn_math_is_square(uint64_t n, bool* _Nonnull out,
                                  pn_error* _Nullable* _Nullable error);

/// Is @param n prime?  Seven Miller-Rabin witnesses answer for the
/// whole ledger, so Carmichael's fools — is_prime(561) says false —
/// cannot slip past.
/// @param n - the number to weigh
/// @param out - where the verdict lands on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
bool* _Nullable pn_math_is_prime(uint64_t n, bool* _Nonnull out,
                                 pn_error* _Nullable* _Nullable error);

/// The next prime at or after @param n — inclusive, the culture
/// counting the number itself when it is already prime:
/// next_prime(1) = 2, next_prime(2) = 2, next_prime(46340) = 46349.
/// @param n - weigh from here upward
/// @param out - where the answer lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE past the last prime of the
///     ledger.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_math_next_prime(uint64_t n, uint64_t* _Nonnull out,
                                       pn_error* _Nullable* _Nullable error);

size_t pn_math_factorize_size(uint64_t n);

/// The abacist's ledger of factors: @param n = f[0] x f[1] x ...
/// ascending, repeats kept — the culture wrote 90 as 2 x 3 x 3 x 5
/// and 649 as 11 x 59.
/// PN_ERROR_BUFFER when @param cap falls short of the count; on any
/// failure the caller's rows and @param count keep their word.
/// @param n - split this number
/// @param out - where the factors land, @param cap slots of room
/// @param cap - slots available at @param out
/// @param count - how many factors were written, on success
/// @param error - where to hang the note on failure.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_math_factorize(uint64_t n, uint64_t* _Nonnull out, size_t cap,
                                      size_t* _Nonnull count, pn_error* _Nullable* _Nullable error);

/// Is @param n equal to its own parts (Elements IX.36)?  6 = 1 + 2
/// + 3, then 28, 496, 8128 — the even form Euclid built and Euler
/// proved necessary.  The culture weighed proper divisors from 1 up,
/// so the blank and the unit sit outside the domain:
/// perfect_check(0) earns a PN_ERROR_RANGE note while
/// perfect_check(90) answers cleanly with false.
/// @param n - the number to weigh
/// @param out - where the verdict lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when n is below 2.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
bool* _Nullable pn_math_perfect_check(uint64_t n, bool* _Nonnull out,
                                      pn_error* _Nullable* _Nullable error);

// MARK: - Pythagoreas

/// The Plimpton tablet's loom (Elements X.29): from @param m >
/// @param n >= 1 with m <= PN_MATH_M_MAX_VALUE, the legs are m*m -
/// n*n and 2*m*n and the hypotenuse m*m + n*n.   m = 2, n = 1 weaves
/// 3, 4, 5; m = 12, n = 5 weaves 119, 120, 169.  The gnomon leg
/// comes first, Plimpton's order.
/// @param m - the longer generator
/// @param n - the shorter generator
/// @param leg1 - where the gnomon leg lands
/// @param leg2 - where the even leg lands
/// @param hypotenuse - where the hypotenuse lands
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when the
///     generators break 1 <= n < m <= 65535 or any side runs past
///     PN_MATH_SIDE_MAX_VALUE.
/// @returns the caller's `leg1` on success, or NULL and the
/// @param error encountered.
uint64_t* _Nullable pn_math_pythagorean_triple(uint64_t m, uint64_t n, uint64_t* _Nonnull leg1,
                                               uint64_t* _Nonnull leg2,
                                               uint64_t* _Nonnull hypotenuse,
                                               pn_error* _Nullable* _Nullable error);

/// Weigh the rod: does @param x*x + @param y*y equal @param z*z?
/// (119, 120, 169) stands.  Sides outside the rod's foot earn a
/// PN_ERROR_RANGE note.
/// @param x - one side
/// @param y - the other side
/// @param z - the longest side
/// @param out - where the verdict lands on success
/// @param error - where to hang the note on failure:
///     PN_ERROR_RANGE when a side runs under 1 or
///     past PN_MATH_SIDE_MAX_VALUE.
/// @returns the caller's `out` on success, or NULL and the
/// @param error encountered.
bool* _Nullable pn_math_pythagorean_check(uint64_t x, uint64_t y, uint64_t z, bool* _Nonnull out,
                                          pn_error* _Nullable* _Nullable error);

#pragma clang diagnostic pop

#ifdef __cplusplus
}
#endif
#endif /* PANTHEON_H */
