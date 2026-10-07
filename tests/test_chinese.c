/*  test_chinese.c — hanzi numeral rendering.
   The signs are written as C11 universal character names so this file
   stays pure ASCII (signs at least):
   YI   U+4E00 一 = 1      (UTF-8 E4 B8 80)
   SHI  U+5341 十 = 10     (UTF-8 E5 8D 81)
   WAN  U+4E07 万 = 10 000 (UTF-8 E4 B8 87)
   …  二 三 四 五 六 七 八 九 百 千 follow in the tables below.
   Golden rendering from https://en.wikipedia.org/wiki/Chinese_numerals:
   4622 = 四千六百二十二 — digits with their multipliers, high to low,
   the formal full reading (一十四 for 14), and one blank space
   wherever a digit is skipped (一千 五 = 1005).
*/

#include "check.h"

#define YI   "一"
#define ER   "二"
#define SAN  "三"
#define SI   "四"
#define WU   "五"
#define LIU  "六"
#define QI   "七"
#define BA   "八"
#define JIU  "九"
#define SHI  "十"
#define BAI  "百"
#define QIAN "千"
#define WAN  "万"
#define LING "零" /* 零: the later scribal zero - foreign here */
#define QUEN "〇" /* 〇: the modern circle - foreign here too */

/* Render into a generous buffer, asserting render_size == strlen. */
static void render_ok(uint64_t n, char* buf, size_t buflen) {
    size_t need = pn_hanzi_render_size(n);
    CHECK_OK(pn_hanzi_render(n, buf, need + 1, &err_), err_);
    CHECK(strlen(buf) == need);
    (void) buflen;
}

/* render(n) into buf and parse it back, checking the round trip. */
static void roundtrip(uint64_t n) {
    char buf[256];
    size_t need = pn_hanzi_render_size(n);
    CHECK_OK(pn_hanzi_render(n, buf, need + 1, &err_), err_);
    CHECK(strlen(buf) == need);
    uint64_t back = 0;
    CHECK_OK(pn_hanzi_parse(buf, strlen(buf), &back, &err_), err_);
    CHECK(back == n);
}

/*  parse(text) — a NUL-terminated literal is exactly what a caller who
   measured it would pass.
*/
static bool parse_str(const char* text, uint64_t* out, pn_error** error) {
    return pn_hanzi_parse(text, strlen(text), out, error);
}

int main(void) {
    char buf[4096];
    uint64_t v;

    /* the single-digit values and the raw bytes */

    render_ok(7u, buf, sizeof buf);
    CHECK(strcmp(buf, QI) == 0);
    /* the seven's raw bytes pinned, as the stroke's are */
    CHECK((unsigned char) buf[0] == 0xE4 && (unsigned char) buf[1] == 0xB8 &&
          (unsigned char) buf[2] == 0x83);

    /* golden renderings */

    render_ok(0u, buf, sizeof buf); /* the blank */
    CHECK(buf[0] == '\0');
    render_ok(10u, buf, sizeof buf);
    CHECK(strcmp(buf, YI SHI) == 0);
    render_ok(14u, buf, sizeof buf); /* the formal 一十四 */
    CHECK(strcmp(buf, YI SHI SI) == 0);
    render_ok(100u, buf, sizeof buf);
    CHECK(strcmp(buf, YI BAI) == 0);
    render_ok(101u, buf, sizeof buf); /* blank over the 十 */
    CHECK(strcmp(buf, YI BAI " " YI) == 0);
    render_ok(110u, buf, sizeof buf);
    CHECK(strcmp(buf, YI BAI YI SHI) == 0);
    render_ok(105u, buf, sizeof buf); /* blank over the 十 */
    CHECK(strcmp(buf, YI BAI " " WU) == 0);
    render_ok(115u, buf, sizeof buf); /* no blank: 十 present */
    CHECK(strcmp(buf, YI BAI YI SHI WU) == 0);
    render_ok(1005u, buf, sizeof buf); /* blank over 百 and 十 */
    CHECK(strcmp(buf, YI QIAN " " WU) == 0);
    render_ok(1050u, buf, sizeof buf);
    CHECK(strcmp(buf, YI QIAN " " WU SHI) == 0);
    render_ok(4622u, buf, sizeof buf); /* the worked example */
    CHECK(strcmp(buf, SI QIAN LIU BAI ER SHI ER) == 0);
    CHECK(pn_hanzi_render_size(4622u) == 21u); /* 7 signs * 3 bytes, no blanks */
    render_ok(403u, buf, sizeof buf);
    CHECK(strcmp(buf, SI BAI " " SAN) == 0);
    render_ok(40004u, buf, sizeof buf); /* blank across 千 百 十 */
    CHECK(strcmp(buf, SI WAN " " SI) == 0);
    render_ok(41000u, buf, sizeof buf); /* no blank: 万 and 千 adjacent */
    CHECK(strcmp(buf, SI WAN YI QIAN) == 0);
    render_ok(4300000u, buf, sizeof buf); /* 430 万: 四百三十万 */
    CHECK(strcmp(buf, SI BAI SAN SHI WAN) == 0);
    render_ok(50000005u, buf, sizeof buf);
    CHECK(strcmp(buf, WU QIAN WAN " " WU) == 0);
    render_ok(99999999u, buf, sizeof buf); /* the full 万 set */
    CHECK(strcmp(buf, JIU QIAN JIU BAI JIU SHI JIU WAN JIU QIAN JIU BAI JIU SHI JIU) == 0);

    /* render error cases */

    CHECK_ERROR(pn_hanzi_render(1u, pn_nothing(), 10, &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_hanzi_render(1u, buf, 0, &err_), err_, PN_ERROR_BUFFER);
    render_ok(4622u, buf, sizeof buf);
    CHECK_ERROR(pn_hanzi_render(4622u, buf, pn_hanzi_render_size(4622u), &err_), err_,
                PN_ERROR_BUFFER); /* the exact-minus-NUL cut */
    CHECK_ERROR(pn_hanzi_render(PN_HANZI_MAX_VALUE + 1u, buf, sizeof buf, &err_), err_,
                PN_ERROR_RANGE); /* 亿 is not written here */
    CHECK_OK(pn_hanzi_render(PN_HANZI_MAX_VALUE, buf, sizeof buf, &err_), err_);
    CHECK_ERROR(pn_hanzi_render(UINT64_MAX, buf, sizeof buf, &err_), err_, PN_ERROR_RANGE);

    /* parse error cases */

    CHECK_ERROR(pn_hanzi_parse(pn_nothing(), 0, &v, &err_), err_, PN_ERROR_NULL);
    CHECK_ERROR(pn_hanzi_parse(buf, 3, pn_nothing(), &err_), err_, PN_ERROR_NULL);

    uint64_t probe;
    CHECK_OK(parse_str("", &probe, &err_), err_); /* the blank reads back as 0 */
    CHECK(probe == 0u);
    CHECK_ERROR(parse_str("十四", &probe, &err_), err_, PN_ERROR_SYNTAX);   /* elided 一 */
    CHECK_ERROR(parse_str("百一十", &probe, &err_), err_, PN_ERROR_SYNTAX); /* begins with 百 */
    CHECK_ERROR(parse_str("一百百", &probe, &err_), err_, PN_ERROR_SYNTAX); /* 百 twice */
    CHECK_ERROR(parse_str("一三", &probe, &err_), err_, PN_ERROR_SYNTAX);   /* digit, digit */
    CHECK_ERROR(parse_str("四百百", &probe, &err_), err_, PN_ERROR_SYNTAX); /* ascending... */
    CHECK_ERROR(parse_str("三百二十"
                          "十",
                          &probe, &err_),
                err_, PN_ERROR_SYNTAX);                                   /* ...and twice */
    CHECK_ERROR(parse_str(" " WU, &probe, &err_), err_, PN_ERROR_SYNTAX); /* leading space */
    CHECK_ERROR(parse_str(WU " ", &probe, &err_), err_, PN_ERROR_SYNTAX); /* trailing space */
    CHECK_ERROR(parse_str(YI QIAN "  " WU, &probe, &err_), err_, PN_ERROR_SYNTAX); /* doubled */
    CHECK_ERROR(parse_str(YI QIAN WU, &probe, &err_), err_, PN_ERROR_SYNTAX); /*  the colloquial
   一千五 = "1500" is NOT 1005
   and not canonical either
*/
    CHECK_ERROR(parse_str(WU QIAN WAN WU, &probe, &err_), err_,
                PN_ERROR_SYNTAX); /* missing blank */
    CHECK_ERROR(parse_str(SI BAI " " SAN SHI YI, &probe, &err_), err_,
                PN_ERROR_SYNTAX);                                       /* unjustified blank */
    CHECK_ERROR(parse_str(LING, &probe, &err_), err_, PN_ERROR_SYNTAX); /* 零: not our zero */
    CHECK_ERROR(parse_str(QUEN, &probe, &err_), err_, PN_ERROR_SYNTAX); /* 〇: nor this one */
    CHECK_ERROR(parse_str("5", &probe, &err_), err_, PN_ERROR_SYNTAX);  /* ASCII */
    CHECK_ERROR(parse_str("\xE4\xB8", &probe, &err_), err_, PN_ERROR_SYNTAX); /* 一 cut off short */
    CHECK_ERROR(parse_str("万", &probe, &err_), err_, PN_ERROR_SYNTAX);       /* 万 alone */
    CHECK_ERROR(parse_str(WU WAN SAN WAN, &probe, &err_), err_, PN_ERROR_SYNTAX); /* second 万 */
    CHECK_ERROR(parse_str("百", &probe, &err_), err_, PN_ERROR_SYNTAX); /* bare multiplier */
    CHECK_ERROR(parse_str(SAN WAN "千", &probe, &err_), err_,
                PN_ERROR_SYNTAX); /* 千 with no digit */

    /* valid non-canonical-looking shapes that ARE canonical */

    CHECK_OK(parse_str(YI SHI YI, &probe, &err_), err_); /* 一十一 = 11 */
    CHECK(probe == 11u);
    CHECK_OK(parse_str(SI WAN " " SI, &probe, &err_), err_); /* the blank */
    CHECK(probe == 40004u);
    CHECK_OK(parse_str(SI BAI " " SAN WAN WU QIAN, &probe, &err_), err_);
    CHECK(probe == 4035000u);
    CHECK_OK(
        parse_str(JIU QIAN JIU BAI JIU SHI JIU WAN JIU QIAN JIU BAI JIU SHI JIU, &probe, &err_),
        err_);
    CHECK(probe == PN_HANZI_MAX_VALUE);

    /* round-trip sweep */

    for (uint64_t n = 0; n < 10000; n++)
        roundtrip(n); /* every units section */
    for (uint64_t p = 1; p <= 10000000; p *= 10)
        roundtrip(p);          /* one 万 at each height */
    roundtrip(100000000u - 1); /* the maximum */
    roundtrip(1010101u);       /* blank after blank */
    roundtrip(9090909u);
    roundtrip(10000001u);
    roundtrip(4300000u);
    roundtrip(4305000u);
    roundtrip(50000005u);

    return CHECK_RUN();
}
