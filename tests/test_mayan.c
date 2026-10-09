/*  test_mayan.c — Maya bar-and-dot rendering over pn_base20.
   The signs are C11 universal character names so this file stays pure
   ASCII:
   SHELL        U+1D2E0  = 0     (the shell: the era's one true zero)
   ONE..FOUR    U+1D2E1..E4  = 1..4    dots
   BAR          U+1D2E5  = 5
   SIX..NINE    U+1D2E6..E9  = 6..9    bar + dots
   TEN..FOURTEEN    U+1D2EA..EE  = 10..14   two bars + dots
   FIFTEEN..NINETEEN    U+1D2EF..F3  = 15..19   three bars + dots
   all bands: 4 bytes of UTF-8 each, stem F0 9D 8B, tail A0..B3.
   Golden renderings from https://en.wikipedia.org/wiki/Maya_numerals:
   23   = three dots under one dot               [1,3]
   33   = three dots atop two bars, under a dot       [1,13]
   429 = four dots and a bar, dot, dot            [1,1,9]
   8800 = 1x20^3 + 2x20^2: dot dot shell shell        [1,2,0,0]
*/

#include "check.h"

#define SHELL    "𝋠"
#define DOT      "𝋡"
#define TWO      "𝋢"
#define THREE    "𝋣"
#define NINE     "𝋩"
#define THIRTEEN "𝋭"

static const char* const digit_signs[20] = {
    SHELL, DOT, TWO, THREE,    "𝋤", "𝋥", "𝋦", "𝋧", "𝋨", NINE,
    "𝋪",   "𝋫", "𝋬", THIRTEEN, "𝋮", "𝋯", "𝋰", "𝋱", "𝋲", "𝋳",
};

/*  Independent hand-spelling of the canonical rendering: one sign per
   place, most significant first, one space per boundary, '-' when
   neg.  Written without touching the library.
*/
static void expect_render(pn_base20 v, char* out) {
    char* p = out;
    if (v.neg)
        *p++ = '-';
    for (unsigned i = 0; i < v.n_places; i++) {
        if (i > 0)
            *p++ = ' ';
        memcpy(p, digit_signs[v.places[i]], 4);
        p += 4;
    }
    *p = '\0';
}

int main(void) {
    pn_base20 v;
    char buf[512];
    char want[512];

    /* the codex page's worked examples */

    CHECK_OK(pn_base20_from_u64(23u, &v, &err_), err_);
    CHECK_OK(pn_mayan_render(v, buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, DOT " " THREE) == 0);

    CHECK_OK(pn_base20_from_u64(33u, &v, &err_), err_);
    CHECK_OK(pn_mayan_render(v, buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, DOT " " THIRTEEN) == 0);

    CHECK_OK(pn_base20_from_u64(429u, &v, &err_), err_);
    CHECK_OK(pn_mayan_render(v, buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, DOT " " DOT " " NINE) == 0);

    /*  where the Babylonian scribe leaves two blank spaces, the shell
	   sits: the era's answer to the missing-zero tragedy
	*/
    CHECK_OK(pn_base20_from_u64(8800u, &v, &err_), err_);
    CHECK_OK(pn_mayan_render(v, buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, DOT " " TWO " " SHELL " " SHELL) == 0);

    /* zero is a sign, not a blank: the lone shell */
    CHECK_OK(pn_base20_from_u64(0u, &v, &err_), err_);
    CHECK(pn_mayan_render_size(v) == 4);
    CHECK_OK(pn_mayan_render(v, buf, sizeof buf, &err_), err_);
    CHECK(strcmp(buf, SHELL) == 0);
    /* its raw bytes pinned, as the strokes' are in the other suites */
    CHECK((unsigned char) buf[0] == 0xF0 && (unsigned char) buf[1] == 0x9D &&
          (unsigned char) buf[2] == 0x8B && (unsigned char) buf[3] == 0xA0);

    /* every whole digit renders as exactly one sign */
    for (unsigned d = 0; d < 20u; d++) {
        memset(&v, 0, sizeof v);
        v.n_places = 1;
        v.radix = 1;
        v.places[0] = (uint8_t) d;
        v.places[1] = 99; /*  poke one past: a renderer that reads
   beyond n_places shows up here
*/
        CHECK_OK(pn_mayan_render(v, buf, sizeof buf, &err_), err_);
        CHECK(strcmp(buf, digit_signs[d]) == 0);
        CHECK(pn_mayan_render_size(v) == 4);
    }

    /* a negative stack: the '-' anachronism leads, as cuneiform's does */
    memset(&v, 0, sizeof v);
    v.n_places = 2;
    v.radix = 2;
    v.neg = true;
    v.places[0] = 1;
    v.places[1] = 0;
    CHECK_OK(pn_mayan_render(v, buf, sizeof buf, &err_), err_);
    CHECK(buf[0] == '-' && strcmp(buf + 1, DOT " " SHELL) == 0);
    CHECK(pn_mayan_render_size(v) == 1 + 4 + 1 + 4);

    /* render_size exact against the hand speller, swept */

    for (uint64_t n = 0; n < 160000u; n++) {
        CHECK_OK(pn_base20_from_u64(n, &v, &err_), err_);
        (void) expect_render(v, want);
        CHECK(pn_mayan_render_size(v) == strlen(want));
        if (pn_fails)
            break;
    }

    /* errors: render — the well_formed rule book */

    CHECK_OK(pn_base20_from_u64(1u, &v, &err_), err_);

    memset(buf, 'x', 8);
    v.places[2] = 20; /* {1,0,20,0}: a place past nineteen */
    v.n_places = 4;
    v.radix = 4;
    v.neg = false;
    CHECK_ERROR(pn_mayan_render(v, buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);
    CHECK(memcmp(buf, "xxxx", 4) == 0); /* untouched */

    memset(&v, 0, sizeof v);
    CHECK_ERROR(pn_mayan_render(v, buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN); /* n_places 0 */
    v.n_places = 2;
    v.radix = 3; /* radix above the stack */
    v.places[0] = 1;
    v.places[1] = 1;
    CHECK_ERROR(pn_mayan_render(v, buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);

    /* buffer one byte short of need + NUL */
    memset(buf, 'x', 8);
    CHECK_OK(pn_base20_from_u64(1u, &v, &err_), err_);
    CHECK_ERROR(pn_mayan_render(v, buf, 4, &err_), err_, PN_ERROR_BUFFER); /* need is 4 */
    CHECK(memcmp(buf, "xxxx", 4) == 0);

    /*  parse: the strict inverse, swept
	   Every n's canonical string parses back through both the
	   NUL-terminated and the exact-byte-count spelling.
	*/
    for (uint64_t n = 0; n < 160000u; n++) {
        char rendered[5 * PN_BASE20_MAX_PLACES + 8];
        size_t need;
        CHECK_OK(pn_base20_from_u64(n, &v, &err_), err_);
        need = pn_mayan_render_size(v);
        CHECK_OK(pn_mayan_render(v, rendered, need + 1, &err_), err_);
        pn_base20 back;
        memset(&back, 0xEE, sizeof back); /* junk sentinel */
        CHECK_OK(pn_mayan_parse(rendered, need, &back, &err_), err_);
        CHECK(back.n_places == v.n_places && back.radix == v.n_places && !back.neg &&
              memcmp(back.places, v.places, v.n_places) == 0);
        back.radix = 7; /* period style, both spellings */
        CHECK_OK(pn_mayan_parse(rendered, strlen(rendered), &back, &err_), err_);
        CHECK(back.radix == v.n_places);
        if (pn_fails)
            break; /* the first failure names n */
    }

    /* the shell alone parses to {0}; period style and exact count both */
    pn_base20 back;
    memset(&back, 0xEE, sizeof back);
    CHECK_OK(pn_mayan_parse(SHELL, 4, &back, &err_), err_);
    CHECK(back.n_places == 1 && back.radix == 1 && !back.neg && back.places[0] == 0);

    /*  leading shells are kept, not rejected (a value may carry them,
	   the way pn_base20_parse keeps the 0 of "0;5" literal); three
	   whole signs of four bytes each make the exact count 14
	*/
    CHECK_OK(pn_mayan_parse(SHELL " " DOT " " THREE, 14, &back, &err_), err_);
    CHECK(back.n_places == 3 && back.radix == 3 && back.places[0] == 0 && back.places[1] == 1 &&
          back.places[2] == 3);

    /* negative: the '-' reads out of the period-style places */
    const char* negs = "-" DOT " " SHELL; /* [1,0] neg = -20 */
    CHECK_OK(pn_mayan_parse(negs, strlen(negs), &back, &err_), err_);
    CHECK(back.neg && back.n_places == 2 && back.radix == 2 && back.places[0] == 1 &&
          back.places[1] == 0);

    /*  errors: parse — foreign bytes, bad spacing, cut-offs
	   Each leaves *out untouched — checked via the sentinel.
	*/
    static const char* const bad_strings[] = {
        "",            /* the empty tablet: syntax, not zero */
        "-",           /* a sign over nothing */
        " ",           /* one space is not the shell */
        "   ",         /* the Babylonian's gap: refused here */
        "abc",         /* not signs */
        "\t" DOT,      /* tabs are not how codices are ruled */
        " " DOT,       /* the stack never starts in a gutter */
        DOT " ",       /* nor ends in one */
        DOT "   " DOT, /* a doubled gutter */
        DOT DOT,       /* two signs jammed in one row */
        "𝋴",        /* the block's void past NINETEEN */
        "𝋿",        /* still void */
        "𝍠",           /* a counting rod is no bar-and-dot */
        "𑁒",           /* a Brahmi number is no dot */
        "𒁹",           /* a cuneiform DISH is no shell */
        "𝤀",           /* same SMP plane, wrong block */
        DOT "A",       /* an ASCII byte glued on */
        "-" DOT " ",   /* trailing gutter after a neg stack */
    };
    for (size_t i = 0; i < sizeof bad_strings / sizeof bad_strings[0]; i++) {
        memset(&back, 0xFF, sizeof back); /* sentinel */
        CHECK_ERROR(pn_mayan_parse(bad_strings[i], strlen(bad_strings[i]), &back, &err_), err_,
                    PN_ERROR_SYNTAX);
        CHECK(back.radix == 255u && back.n_places == 255u); /* untouched */
    }

    /* cut-off signs: partial UTF-8 is never a sign */
    memset(&back, 0xFF, sizeof back);
    CHECK_ERROR(pn_mayan_parse(SHELL, 3, &back, &err_), err_, PN_ERROR_SYNTAX);
    CHECK(back.n_places == 255u);
    const char* half = DOT " "
                           "\xF0\x9D\x8B"; /* a stem with no tail */
    CHECK_ERROR(pn_mayan_parse(half, 4 + 1 + 3, &back, &err_), err_, PN_ERROR_SYNTAX);
    CHECK(back.n_places == 255u);

    /*  the page boundary: sixteen rows fit the struct, a seventeenth is
	   the scribe's page overrun - range, not grammar.  Count first is
	   the parse discipline: this fails before any glyph care
	*/
    char rows[18 * 5];
    size_t w = 0;
    for (unsigned i = 0; i < 16u; i++) { /* a full page of dots */
        if (i > 0)
            rows[w++] = ' ';
        memcpy(rows + w, DOT, 4);
        w += 4;
    }
    memset(&back, 0xFF, sizeof back);
    CHECK_OK(pn_mayan_parse(rows, w, &back, &err_), err_); /* 16 rows fit */
    CHECK(back.n_places == PN_BASE20_MAX_PLACES);
    rows[w++] = ' '; /* the seventeenth */
    memcpy(rows + w, DOT, 4);
    w += 4;
    memset(&back, 0xFF, sizeof back);
    CHECK_ERROR(pn_mayan_parse(rows, w, &back, &err_), err_, PN_ERROR_RANGE);
    CHECK(back.n_places == 255u);


    /*  render_size is pure and total even on a junk struct (meaningless,
	   never fatal - the sibling contract, babylonian.c's is the same)
	*/
    memset(&v, 0x7F, sizeof v);     /* poke everything */
    (void) pn_mayan_render_size(v); /* must not crash */
    CHECK_ERROR(pn_mayan_render(v, buf, sizeof buf, &err_), err_, PN_ERROR_DOMAIN);

    return CHECK_RUN();
}
