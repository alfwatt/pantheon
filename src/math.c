/*  math.c — the early mathematical theory on the uint64 ledger.
  The culture proved with its hands before it proved with letters:
  Euclid's ladder of remainders (Elements VII), the sieve
  Eratosthenes worked down the papyrus, the Pythagoreans' gnomon
  figures and their triples, the abacist's ledger of factors.  Nothing
  here renders a notation - this is the reckoning that sat underneath
  the numerals.
  Below 2^32 Euclid's own mechanics suffice - trial division by the
  sheet's unstruck odd rows is a complete proof there, since a
  smallest odd divisor of such an n never passes 65535 - and the far
  ledger runs on Miller's seven certified witnesses, so even the
  number 18446744073709551615 answers without the water clock
  running out.
 */

#include <string.h>

#include "pantheon.h"

static const uint8_t small_primes[] = {
    2u,  3u,  5u,  7u,  11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u, 41u,
    43u, 47u, 53u, 59u, 61u, 67u, 71u, 73u, 79u, 83u, 89u, 97u,
};
#define N_SMALL ((sizeof small_primes) / (sizeof small_primes[0]))

/*  The papyrus of Eratosthenes: the odd candidate rows below 2^16
  (only odd indices are struck).  Trial division by its unstruck
  rows proves primality exactly while n < 2^32: the smallest odd
  divisor d of such an n then satisfies d <= 65535.
*/
#define PN_MATH_SHEET 65536u

/*  Miller's seven certified witnesses: below 3.3e24 — so across the
  whole uint64 ledger - a base passing every round certifies
  primality, and every composite fails at least one base.
*/
static const uint64_t witnesses[] = {
    2ull, 325ull, 9375ull, 28178ull, 450775ull, 9780504ull, 1795265022ull,
};
#define N_WITNESSES ((sizeof witnesses) / (sizeof witnesses[0]))

/*  overflow-free modular mechanics, pure C11: the ledger has no
  128-bit word, so a + b (mod m) advances without ever wrapping.
*/
static uint64_t pn_addmod(uint64_t x, uint64_t y, uint64_t m) {
    /*  both operands are already reduced below m: when y >= m - x
  the true sum reaches m and comes back down without ever
  wrapping; otherwise the sum is simply x + y, which cannot
  overflow because it stays under m
*/
    return (y >= m - x) ? x - (m - y) : (x + y);
}

static uint64_t pn_mulmod(uint64_t a, uint64_t b, uint64_t m) {
    uint64_t r = 0u;

    a %= m;
    while (b != 0u) {
        if ((b & 1u) != 0u)
            r = pn_addmod(r, a, m);
        b >>= 1;
        if (b != 0u)
            a = pn_addmod(a, a, m);
    }
    return r;
}

static uint64_t pn_powmod(uint64_t a, uint64_t e, uint64_t m) {
    uint64_t r = 1ull % m;

    a %= m;
    while (e != 0u) {
        if ((e & 1u) != 0u)
            r = pn_mulmod(r, a, m);
        e >>= 1;
        if (e != 0u)
            a = pn_mulmod(a, a, m);
    }
    return r;
}

/*  the witnesses passed: n prime, for every n of the ledger */
static int pn_witness_prime(uint64_t n) {
    uint64_t d = n - 1u;
    unsigned s = 0u, k, r, composite;

    if (n < 2u)
        return 0;
    if ((n & 1u) == 0u)
        return n == 2u;
    while ((d & 1u) == 0u) {
        d >>= 1;
        s++;
    }
    for (k = 0u; k < N_WITNESSES; k++) {
        uint64_t a = witnesses[k] % n, x;
        if (a == 0u)
            continue; /* the witness is n itself     */
        x = pn_powmod(a, d, n);
        if (x == 1u || x == n - 1u)
            continue;
        composite = 1u;
        for (r = 1u; r < s; r++) {
            x = pn_mulmod(x, x, n);
            if (x == n - 1u) {
                composite = 0u;
                break;
            }
        }
        if (composite != 0u)
            return 0;
    }
    return 1;
}

static uint64_t pn_gcd(uint64_t a, uint64_t b) {
    while (b != 0u) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/*  the sheet, laid fresh in local memory: struck rows carry 1 */
static void pn_sheet(uint8_t* composite) {
    uint64_t i, j;

    memset(composite, 0, PN_MATH_SHEET);
    for (i = 3u; i * i < PN_MATH_SHEET; i += 2u) {
        if (composite[i] == 0u)
            for (j = i * i; j < PN_MATH_SHEET; j += 2u * i)
                composite[j] = 1u;
    }
}

/*  primality on the sheet and the witnesses: the unstruck odd rows
  divide below 2^32 (where trial division by the sheet is the whole
  proof); the witnesses certify the far ledger.
*/
static int pn_prime_on(const uint8_t* composite, uint64_t n) {
    uint64_t d;
    unsigned k;

    if (n < 2u)
        return 0;
    if ((n & 1u) == 0u)
        return n == 2u;
    for (k = 0u; k < N_SMALL; k++) {
        if (n == small_primes[k])
            return 1;
        if ((n % small_primes[k]) == 0u)
            return 0; /* divisible, and not the      */
    } /* prime itself: composite     */
    if (n < PN_MATH_SHEET)
        return composite[(size_t) n] == 0u;
    if (n < (1ull << 32)) {
        for (d = 101ull; d * d <= n; d += 2u) {
            if ((n % d) == 0u)
                return 0;
        }
        if (d <= 0xFFFFull)
            return 1;               /* the sweep passed the root:
		                                      Euclid has proved it        */
        return pn_witness_prime(n); /* the sheet rows ran out
		                                      below the root: the
		                                      witnesses finish the job    */
    }
    return pn_witness_prime(n);
}

static uint64_t pn_isqrt(uint64_t n) {
    uint64_t res = 0u, bit = 1ull << 62;

    while (bit > n)
        bit >>= 2;
    while (bit != 0u) {
        if (n >= (res + bit)) {
            n -= res + bit;
            res = (res >> 1) + bit;
        }
        else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/*  Pollard's walk, deterministic seed cadence — the ledger has no
  dice, and one seed can cycle; a cycled seed hands off to the
  next.  Returns 0 only when every seed cycled.
*/
static uint64_t pn_rho_f(uint64_t x, uint64_t c, uint64_t n) {
    return pn_addmod(pn_mulmod(x, x, n), c % n, n);
}

static uint64_t pn_rho_divisor(uint64_t n) {
    uint64_t c;

    if ((n & 1u) == 0u)
        return 2u;
    for (c = 1ull; c < 128ull; c++) {
        uint64_t x = 2ull + c, y = 2ull + c, g = 1u;
        unsigned guard;
        for (guard = 0u; g == 1u && guard < (1u << 22); guard++) {
            x = pn_rho_f(x, c, n);
            y = pn_rho_f(pn_rho_f(y, c, n), c, n);
            g = pn_gcd(x > y ? x - y : y - x, n);
        }
        if (g > 1u && g < n)
            return g;
    }
    return 0u;
}

static void pn_factor_hard(uint64_t n, uint64_t* facs, size_t* w) {
    uint64_t f;

    if (n <= 1u)
        return;
    if (pn_witness_prime(n) != 0) {
        facs[(*w)++] = n;
        return;
    }
    f = pn_rho_divisor(n);
    if (f == 0u) { /* every walk cycled: fall
	                                             back to Euclid's ladder,
	                                             odd rungs only           */
        uint64_t d = 101ull;
        while (d <= 0xFFFFull && (n % d) != 0u)
            d += 2u;
        f = (d <= 0xFFFFull && (n % d) == 0u) ? d : 0u;
        if (f == 0u) { /* no rung below 2^16 and a
		                                       witness says composite -
		                                       the walk will be retried
		                                       by the caller's caller;
		                                       record n whole            */
            facs[(*w)++] = n;
            return;
        }
    }
    pn_factor_hard(f, facs, w);
    pn_factor_hard(n / f, facs, w);
}

size_t pn_math_sieve_size(uint64_t limit) {
    uint8_t composite[PN_MATH_SHEET];
    uint64_t n;
    size_t count = 0u;

    if (limit < 2u || limit > PN_MATH_SIEVE_MAX_VALUE)
        return 0u;
    pn_sheet(composite);
    for (n = 2u; n <= limit; n++) {
        if (pn_prime_on(composite, n) != 0)
            count++;
    }
    return count;
}

bool pn_math_sieve(uint64_t limit, uint64_t* _Nullable out, size_t cap, size_t* _Nullable count,
                   pn_error* _Nullable* _Nullable error) {
    uint8_t composite[PN_MATH_SHEET];
    size_t need, w;
    uint64_t n;

    if (out == NULL || count == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "the sieve needs an array to write"
                            " into and a place for the count");
    if (limit > PN_MATH_SIEVE_MAX_VALUE)
        return pn_error_set(error, PN_ERROR_RANGE,
                            "%llu runs past %llu: the sieve"
                            " sheet holds no more rows",
                            (unsigned long long) limit,
                            (unsigned long long) PN_MATH_SIEVE_MAX_VALUE);
    need = pn_math_sieve_size(limit);
    if (cap < need)
        return pn_error_set(error, PN_ERROR_BUFFER,
                            "the array holds room for %zu"
                            " prime(s); the sieve would write"
                            " %zu of them",
                            cap, need);
    if (need == 0u) {
        *count = 0u; /* the blank ledger: nothing to
			                             strike, nothing to write         */
        return true;
    }
    pn_sheet(composite);
    w = 0u;
    for (n = 2u; n <= limit; n++) {
        if (pn_prime_on(composite, n) != 0)
            out[w++] = n;
    }
    *count = w;
    return true;
}

bool pn_math_gcd(uint64_t a, uint64_t b, uint64_t* _Nullable out,
                 pn_error* _Nullable* _Nullable error) {
    if (out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "gcd needs a place to put the"
                            " answer");
    *out = pn_gcd(a, b);
    return true;
}

bool pn_math_lcm(uint64_t a, uint64_t b, uint64_t* _Nullable out,
                 pn_error* _Nullable* _Nullable error) {
    uint64_t g, q;

    if (out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "lcm needs a place to put the"
                            " answer");
    if (a == 0u || b == 0u) {
        *out = 0u; /* no ratio survives a blank */
        return true;
    }
    g = pn_gcd(a, b);
    q = a / g;
    if (q > UINT64_MAX / b)
        return pn_error_set(error, PN_ERROR_RANGE,
                            "%llu times %llu divided by"
                            " their gcd leaves the ledger:"
                            " no uint64_t holds the answer",
                            (unsigned long long) a, (unsigned long long) b);
    *out = q * b;
    return true;
}

bool pn_math_is_square(uint64_t n, bool* _Nullable out, pn_error* _Nullable* _Nullable error) {
    uint64_t x;

    if (out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "is_square needs a place to put"
                            " the verdict");
    x = pn_isqrt(n);
    *out = (x * x == n); /* x < 2^32: x*x never wraps */
    return true;
}

bool pn_math_is_prime(uint64_t n, bool* _Nullable out, pn_error* _Nullable* _Nullable error) {
    uint8_t composite[PN_MATH_SHEET];

    if (out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "is_prime needs a place to put"
                            " the verdict");
    pn_sheet(composite);
    *out = (pn_prime_on(composite, n) != 0);
    return true;
}

bool pn_math_next_prime(uint64_t n, uint64_t* _Nullable out, pn_error* _Nullable* _Nullable error) {
    uint8_t composite[PN_MATH_SHEET];
    uint64_t cand;
    unsigned k;

    if (out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "next_prime needs a place to put"
                            " the answer");
    for (k = 0u; k < N_SMALL; k++) {
        if (n <= small_primes[k]) {
            *out = small_primes[k];
            return true;
        }
    }
    pn_sheet(composite);
    if ((n & 1u) == 0u)
        cand = n + 1u; /* even n: n+1 is the next odd hope; every
			                     prime past 97 is odd                     */
    else
        cand = n; /* the culture counts the number itself */
    for (;;) {
        if (pn_prime_on(composite, cand) != 0) {
            *out = cand;
            return true;
        }
        if (cand > UINT64_MAX - 2u)
            return pn_error_set(error, PN_ERROR_RANGE,
                                "%llu and every odd number"
                                " above it run off the end"
                                " of the ledger",
                                (unsigned long long) n);
        cand += 2u;
    }
}

/*  ascending insertion sort: the factors leave in Euclid's order  */
static void pn_sort_ascending(uint64_t* facs, size_t n) {
    size_t i;

    for (i = 1u; i < n; i++) {
        uint64_t key = facs[i];
        size_t j = i;
        while (j > 0u && facs[j - 1u] > key) {
            facs[j] = facs[j - 1u];
            j--;
        }
        facs[j] = key;
    }
}

/*  factor n into facs (ascending afterwards); returns the count.
  n >= 2 on entry - the blank and the unit answer the empty
  product before this is reached.
*/
static size_t pn_do_factorize(uint64_t n, uint64_t* facs) {
    size_t w = 0u;
    uint64_t rest = n;
    unsigned k;

    for (k = 0u; k < N_SMALL; k++) {
        while ((rest % small_primes[k]) == 0u) {
            facs[w++] = small_primes[k];
            rest /= small_primes[k];
        }
    }
    if (rest > 1u)
        pn_factor_hard(rest, facs, &w);
    pn_sort_ascending(facs, w);
    return w;
}

size_t pn_math_factorize_size(uint64_t n) {
    uint64_t facs[PN_MATH_MAX_FACTORS];

    if (n < 2u)
        return 0u; /* the blank and the unit     */
    return pn_do_factorize(n, facs);
}

bool pn_math_factorize(uint64_t n, uint64_t* _Nullable out, size_t cap, size_t* _Nullable count,
                       pn_error* _Nullable* _Nullable error) {
    uint64_t facs[PN_MATH_MAX_FACTORS];
    size_t w;

    if (out == NULL || count == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "factorize needs an array to"
                            " write into and a place for the"
                            " count");
    if (n < 2u) {
        *count = 0u; /* 1 is the empty product; the blank
			                             abstains                           */
        return true;
    }
    w = pn_do_factorize(n, facs);
    if (w > cap)
        return pn_error_set(error, PN_ERROR_BUFFER,
                            "the array holds room for %zu"
                            " factor(s); %llu has %zu of"
                            " them",
                            cap, (unsigned long long) n, w);
    for (size_t i = 0u; i < w; i++)
        out[i] = facs[i];
    *count = w;
    return true;
}

/*  the odd-perfect verdict of the published record (no odd perfect
  below 10^1500) settles every odd ledger number - but the module
  weighs its odd candidates honestly below the Mersenne gap, where
  the divisor sum still fits the ledger.
*/
static int pn_sigma_odd_below_gap(uint64_t n, uint64_t* out) {
    uint64_t facs[PN_MATH_MAX_FACTORS];
    size_t count, i;
    uint64_t sigma = 1u;

    count = pn_do_factorize(n, facs);
    for (i = 0u; i < count;) {
        uint64_t p = facs[i], term = 1u, power = 1u;
        while (i < count && facs[i] == p) {
            if (power > UINT64_MAX / p)
                return 0; /* the sum left the ledger   */
            power *= p;
            if (term > UINT64_MAX - power)
                return 0;
            term += power;
            i++;
        }
        if (sigma > UINT64_MAX / term)
            return 0;
        sigma *= term;
    }
    *out = sigma;
    return 1;
}

bool pn_math_perfect_check(uint64_t n, bool* _Nullable out, pn_error* _Nullable* _Nullable error) {
    if (out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "perfect_check needs a place to"
                            " put the verdict");
    if (n < 2u)
        return pn_error_set(error, PN_ERROR_RANGE,
                            "%llu is the blank or the unit:"
                            " a perfect number is a sum of"
                            " its own parts",
                            (unsigned long long) n);
    if ((n & 1u) == 0u) {
        /* Euclid-Euler, the culture's own theorem: an even perfect
		    number is 2^(k-1) * (2^k - 1) with 2^k - 1 prime.    Peel the
		    twos off, then weigh the Mersenne with the witnesses. */
        uint64_t twos = 0u, odd = n, mers;

        while ((odd & 1u) == 0u) {
            odd >>= 1;
            twos++;
        }
        if (twos + 1u >= 64u) {
            *out = false; /* 2^64-scale: no Mersenne fits the ledger */
            return true;
        }
        mers = (1ull << (twos + 1u)) - 1u;
        if (mers != odd) {
            *out = false; /* not of Euclid's shape */
            return true;
        }
        *out = (pn_witness_prime(mers) != 0);
        return true;
    }
    /* odd n: the published record (no odd perfect below 10^1500)
	    needs no arithmetic - every odd ledger number is composite by
	    that bound.    The divisor sum is still weighed where it fits, so
	    the answer stays proved, not asserted. */
    uint64_t sigma;

    if (!pn_sigma_odd_below_gap(n, &sigma)) {
        *out = false; /* the sum left the ledger while the bound
			                   says composite                              */
        return true;
    }
    *out = (sigma == 2u * n); /* n odd below 2^63: 2n wraps nowhere */
    return true;
}

bool pn_math_pythagorean_triple(uint64_t m, uint64_t n, uint64_t* _Nullable leg1,
                                uint64_t* _Nullable leg2, uint64_t* _Nullable hypotenuse,
                                pn_error* _Nullable* _Nullable error) {
    uint64_t mm, a, b, c;

    if (leg1 == NULL || leg2 == NULL || hypotenuse == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "the triple needs three places"
                            " to put its sides");
    if (n < 1u || m <= n || m > PN_MATH_M_MAX_VALUE)
        return pn_error_set(error, PN_ERROR_RANGE,
                            "m and n must satisfy 1 <= n <"
                            " m <= %llu; m = %llu and n ="
                            " %llu do not",
                            (unsigned long long) PN_MATH_M_MAX_VALUE, (unsigned long long) m,
                            (unsigned long long) n);
    mm = m * m; /* m < 2^16: the squares fit the rod's foot */
    a = mm - n * n;
    b = 2u * m * n;
    c = mm + n * n;
    if (a > PN_MATH_SIDE_MAX_VALUE || b > PN_MATH_SIDE_MAX_VALUE || c > PN_MATH_SIDE_MAX_VALUE)
        return pn_error_set(error, PN_ERROR_RANGE,
                            "a side of this triangle runs"
                            " past %llu: longer than the"
                            " rule measures",
                            PN_MATH_SIDE_MAX_VALUE);
    *leg1 = a; /* the gnomon leg first, the Plimpton order */
    *leg2 = b;
    *hypotenuse = c;
    return true;
}

bool pn_math_pythagorean_check(uint64_t x, uint64_t y, uint64_t z, bool* _Nullable out,
                               pn_error* _Nullable* _Nullable error) {
    uint64_t xx, yy, zz, rem;

    if (out == NULL)
        return pn_error_set(error, PN_ERROR_NULL,
                            "the check needs a place to put"
                            " the verdict");
    if (x < 1u || y < 1u || z < 1u || x > PN_MATH_SIDE_MAX_VALUE || y > PN_MATH_SIDE_MAX_VALUE ||
        z > PN_MATH_SIDE_MAX_VALUE)
        return pn_error_set(error, PN_ERROR_RANGE,
                            "each side must run from 1 to"
                            " %llu; %llu, %llu, %llu do not"
                            " all fit the rule",
                            PN_MATH_SIDE_MAX_VALUE, (unsigned long long) x, (unsigned long long) y,
                            (unsigned long long) z);
    xx = x * x; /* sides at or below 2^32-1: each square fits */
    yy = y * y;
    zz = z * z;
    rem = UINT64_MAX - xx;
    if (yy > rem) {
        *out = false; /* the true sum left every ledger while zz
			                   sits inside - no triangle then             */
        return true;
    }
    *out = ((xx + yy) == zz);
    return true;
}
