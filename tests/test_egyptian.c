/*  test_egyptian.c — hieroglyphic numeral rendering.
   The signs are written as C11 universal character names so this file
   stays pure ASCII (signs at least):
   STROKE   U+133E4  single stroke     = 1         (UTF-8 F0 93 8F A4)
   HOBBLE   U+13386  cattle hobble     = 10        (UTF-8 F0 93 8E 86)
   COIL     U+13362  coil of rope      = 100       (UTF-8 F0 93 8D A2)
   LOTUS    U+131BC  lotus             = 1 000     (UTF-8 F0 93 86 BC)
   FINGER   U+130AD  bent finger       = 10 000    (UTF-8 F0 93 82 AD)
   TADPOLE  U+13190  tadpole           = 100 000   (UTF-8 F0 93 86 90)
   HEH      U+13068  Heh god           = 1 000 000 (UTF-8 F0 93 81 A8)
   Golden rendering from https://en.wikipedia.org/wiki/Egyptian_numerals:
   the Karnak carving of 4622 = 4 lotus + 6 coils + 2 hobbles + 2 strokes.
*/

#include "check.h"

#define STROKE      "𓏤"
#define HOBBLE      "𓎆"
#define COIL        "𓍢"
#define LOTUS       "𓆼"
#define FINGER      "𓂭"
#define TADPOLE     "𓆐"
#define HEH         "𓁨"
#define TEN_STROKES STROKE STROKE STROKE STROKE STROKE STROKE STROKE STROKE STROKE STROKE

/* the seven signs, descending, paired with their values */
static const char* const signs[] = {HEH, TADPOLE, FINGER, LOTUS, COIL, HOBBLE, STROKE};
static const uint64_t values[] = {1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u};
#define N_SIGNS 7

/* Count whole sign occurrences in a rendered string. */
static int count_signs(const char* buf, const char* sig) {
    int n = 0;
    for (const char* p = buf; (p = strstr(p, sig)) != NULL; p += strlen(sig)) {
        n++;
    }
    return n;
}

/* 4 * (sum of decimal digits): the size any n <= max must render to. */
static size_t digit_sum_bytes(uint64_t n) {
    size_t bytes = 0;
    while (n != 0) {
        bytes += (size_t) (n % 10u) * 4u;
        n /= 10u;
    }
    return bytes;
}

/* Render into a generous buffer, asserting render_size == strlen. */
static void render_ok(uint64_t n, char* buf, size_t buflen) {
    size_t need = pn_egyptian_render_size(n);
    CHECK_OK(pn_egyptian_render(n, buf, need + 1, &err_), err_);
    CHECK(strlen(buf) == need);
    (void) buflen;
}

int main(void) {
    char buf[4096];

    /* the seven single-sign values */

    render_ok(1u, buf, sizeof buf);
    CHECK(strcmp(buf, STROKE) == 0);
    /* the stroke's raw bytes pinned, as the cuneiform DISH's are */
    CHECK((unsigned char) buf[0] == 0xF0 && (unsigned char) buf[1] == 0x93
          && (unsigned char) buf[2] == 0x8F && (unsigned char) buf[3] == 0xA4);

    render_ok(10u, buf, sizeof buf);
    CHECK(strcmp(buf, HOBBLE) == 0);
    render_ok(100u, buf, sizeof buf);
    CHECK(strcmp(buf, COIL) == 0);
    render_ok(1000u, buf, sizeof buf);
    CHECK(strcmp(buf, LOTUS) == 0);
    render_ok(10000u, buf, sizeof buf);
    CHECK(strcmp(buf, FINGER) == 0);
    render_ok(100000u, buf, sizeof buf);
    CHECK(strcmp(buf, TADPOLE) == 0);
    render_ok(1000000u, buf, sizeof buf);
    CHECK(strcmp(buf, HEH) == 0);

    /* every digit d at every power: d copies, 4d bytes, nothing else -- */

    for (unsigned k = 0; k < N_SIGNS; k++) {
        for (unsigned d = 1; d <= 9; d++) {
            uint64_t n = (uint64_t) d * values[k];
            render_ok(n, buf, sizeof buf);
            for (unsigned j = 0; j < N_SIGNS; j++) {
                CHECK(count_signs(buf, signs[j]) == (j == k ? (int) d : 0));
            }
            CHECK(strlen(buf) == (size_t) d * 4u);
        }
    }

    /* multi-sign values */

    /*  The golden Karnak rendering: 4622 = 4 lotus, 6 coils, 2 hobbles,
	   2 strokes, descending and tight.
	*/
    render_ok(4622u, buf, sizeof buf);
    CHECK(strcmp(buf,
                 LOTUS LOTUS LOTUS LOTUS COIL COIL COIL COIL COIL COIL HOBBLE HOBBLE STROKE STROKE)
          == 0);

    /* The scribe's full set: nine of each sign. */
    render_ok(9999999u, buf, sizeof buf);
    for (unsigned j = 0; j < N_SIGNS; j++) {
        CHECK(count_signs(buf, signs[j]) == 9);
    }
    CHECK(strlen(buf) == 7u * 9u * 4u);

    /*  Gaps are silent, not drawn: 1000001 is Heh + stroke and nothing
	   between — no zero sign exists to fill the blank.
	*/
    render_ok(1000001u, buf, sizeof buf);
    CHECK(strcmp(buf, HEH STROKE) == 0);

    /* zero: the blank */

    /* render_size(0) == 0 and render(0) is the empty string. */
    CHECK(pn_egyptian_render_size(0u) == 0);
    memset(buf, 'x', 4);
    CHECK_OK(pn_egyptian_render(0u, buf, 1, &err_), err_);
    CHECK(buf[0] == '\0');

    /* errors */

    /* NULL buffer. */

    /* One past the scribe's full set is range, not silence. */
    memset(buf, 'x', 8);
    CHECK_ERROR(pn_egyptian_render(PN_EGYPTIAN_MAX_VALUE + 1u, buf, sizeof buf, &err_), err_,
                PN_ERROR_RANGE);
    CHECK(memcmp(buf, "xxxx", 4) == 0);
    CHECK_ERROR(pn_egyptian_render(UINT64_MAX, buf, sizeof buf, &err_), err_, PN_ERROR_RANGE);

    /* Buffer one byte short of need + NUL fails and leaves buf alone. */
    memset(buf, 'x', 8);
    CHECK_ERROR(pn_egyptian_render(1u, buf, 4, &err_), err_, PN_ERROR_BUFFER);
    CHECK(memcmp(buf, "xxxx", 4) == 0);

    /*  parsing
	   Round trip first: whatever the renderer carved, the parser must read
	   back — through both the NUL-terminated spelling and the exact byte
	   count. Exhaustive over the scribe's whole domain.
	*/
    for (uint64_t n = 0; n <= PN_EGYPTIAN_MAX_VALUE; n++) {
        char rendered[7 * 9 * 4 + 1];
        size_t need = pn_egyptian_render_size(n);
        CHECK_OK(pn_egyptian_render(n, rendered, need + 1, &err_), err_);
        CHECK(strlen(rendered) == digit_sum_bytes(n));
        uint64_t back = UINT64_MAX;
        CHECK_OK(pn_egyptian_parse(rendered, strlen(rendered), &back, &err_), err_);
        CHECK(back == n);
        back = UINT64_MAX;
        CHECK_OK(pn_egyptian_parse(rendered, need, &back, &err_), err_);
        CHECK(back == n);
        if (pn_fails) {
            break; /* the first failure names n; no need to drown in 10^7 */
        }
    }

    /* The Karnak reading, spelled out. */
    uint64_t read;
    const char* karnak =
        LOTUS LOTUS LOTUS LOTUS COIL COIL COIL COIL COIL COIL HOBBLE HOBBLE STROKE STROKE;
    CHECK_OK(pn_egyptian_parse(karnak, strlen(karnak), &read, &err_), err_);
    CHECK(read == 4622u);

    /* The blank reads back as the nothing it was. */
    read = UINT64_MAX;
    CHECK_OK(pn_egyptian_parse("", 0, &read, &err_), err_);
    CHECK(read == 0u);

    /*  Errors: the strict canonical form. (Each leaves *out untouched —
	   checked via the sentinel below.)
	*/
    static const char* const bad_strings[] = {
        " ",                         /* a lone space is not the scribe's blank */
        "  ",                        /* two spaces are still not a number */
        "abc",                       /* not signs, not blanks */
        "\t" STROKE,                 /* tabs are not how walls are carved */
        "\x22",                      /* an embedded quote byte */
        STROKE HEH,                  /* bottom-up: the Heh leads on the wall */
        HOBBLE STROKE HOBBLE,        /* a run split in two is two runs */
        STROKE STROKE HOBBLE STROKE, /* strokes scattered about */
        HEH TADPOLE STROKE LOTUS,    /* order lost */
    };
    for (size_t i = 0; i < sizeof bad_strings / sizeof bad_strings[0]; i++) {
        read = 4622u; /* sentinel */
        CHECK_ERROR(pn_egyptian_parse(bad_strings[i], strlen(bad_strings[i]), &read, &err_), err_,
                    PN_ERROR_SYNTAX);
        CHECK(read == 4622u); /* untouched */
    }

    /*  Ten strokes overflow one run: that is arithmetic, not grammar —
	   ten strokes are a hobble, scribe.
	*/
    read = 4622u;
    CHECK_ERROR(pn_egyptian_parse(TEN_STROKES, sizeof(TEN_STROKES) - 1, &read, &err_), err_,
                PN_ERROR_RANGE);
    CHECK(read == 4622u);

    /* A run of nine is legal; the tenth copy is what fails. */
    read = 0u;
    CHECK_OK(pn_egyptian_parse(STROKE STROKE STROKE STROKE STROKE STROKE STROKE STROKE,
                               sizeof(STROKE STROKE STROKE STROKE STROKE STROKE STROKE STROKE) - 1,
                               &read, &err_),
             err_);
    CHECK(read == 8u);

    /* A cut-off sign (3 of the stroke's 4 bytes) is not a sign. */
    read = 4622u;
    CHECK_ERROR(pn_egyptian_parse(STROKE, 3, &read, &err_), err_, PN_ERROR_SYNTAX);
    CHECK(read == 4622u);

    /* A sign glued after a cut-off run: the tail byte is foreign. */
    read = 4622u;
    CHECK_ERROR(pn_egyptian_parse(STROKE "\x41", 5, &read, &err_), err_, PN_ERROR_SYNTAX);
    CHECK(read == 4622u);

    return CHECK_RUN();
}
