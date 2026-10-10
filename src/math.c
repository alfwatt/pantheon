/*
    math.c — early number theory on whole numbers up to 2^64 - 1.

    Greatest common divisors (Euclid), the sieve of Eratosthenes,
    prime testing, factoring, square numbers and Pythagorean
    triples. See the Math notes in pantheon.h for the background and
    further reading.

    One practical problem shows up throughout this file: multiplying
    two 64-bit numbers can need up to 128 bits, and plain C11 has no
    128-bit integer. The helpers pn_addmod and pn_mulmod work around
    that so the prime test never overflows.
*/

#include <assert.h>
#include <string.h>

#include "pantheon.h"

static const uint8_t small_primes[] = {
    2u,  3u,  5u,  7u,  11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u, 41u,
    43u, 47u, 53u, 59u, 61u, 67u, 71u, 73u, 79u, 83u, 89u, 97u,
};
#define N_SMALL ((sizeof small_primes) / (sizeof small_primes[0]))

/*
    The sieve sheet covers 0 to 65535 (2^16 - 1). That is enough to
    test any n below 2^32 by trial division, because a composite n
    always has a factor no bigger than its square root, and the
    square root of a number below 2^32 is below 2^16.
*/
#define PN_MATH_SHEET 65536u

/*
    Jim Sinclair's seven Miller-Rabin bases (2011). A computer search
    showed that no composite number below 2^64 passes the test for
    all seven, so together they make the test exact for every
    uint64_t. They are not proven to work past 2^64.
    https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test
*/
static const uint64_t witnesses[] = {
    2ull, 325ull, 9375ull, 28178ull, 450775ull, 9780504ull, 1795265022ull,
};
#define N_WITNESSES ((sizeof witnesses) / (sizeof witnesses[0]))

/*
    Modular arithmetic ("clock arithmetic") keeps only the remainder
    after dividing by m, so every result stays between 0 and m - 1.
    https://en.wikipedia.org/wiki/Modular_arithmetic
*/
static uint64_t pn_addmod(uint64_t x, uint64_t y, uint64_t m) {
    /*
        (x + y) mod m, for x and y already below m. Adding them
        directly could overflow 64 bits, so compare first: if
        y >= m - x, the sum is at least m, and x - (m - y) is the
        sum minus m, computed without ever going past m.
    */
    return (y >= m - x) ? x - (m - y) : (x + y);
}

/*
    (a x b) mod m without overflow, by "double and add": go through
    the bits of b, doubling a at each step and adding it in whenever
    the bit is 1. This is long multiplication in base 2, and also
    how ancient Egyptian scribes multiplied.
    https://en.wikipedia.org/wiki/Ancient_Egyptian_multiplication
*/
static uint64_t pn_mulmod(uint64_t a, uint64_t b, uint64_t m) {
    uint64_t r = 0u;

    a %= m;
    while (b != 0u) {
        if ((b & 1u) != 0u) {
            r = pn_addmod(r, a, m);
        }
        b >>= 1;
        if (b != 0u) {
            a = pn_addmod(a, a, m);
        }
    }
    return r;
}

/*
    (a ^ e) mod m by "square and multiply": the same doubling idea as
    pn_mulmod, with multiplying in place of adding. It takes about
    64 steps instead of e steps.
    https://en.wikipedia.org/wiki/Modular_exponentiation
*/
static uint64_t pn_powmod(uint64_t a, uint64_t e, uint64_t m) {
    uint64_t r = 1ull % m;

    a %= m;
    while (e != 0u) {
        if ((e & 1u) != 0u) {
            r = pn_mulmod(r, a, m);
        }
        e >>= 1;
        if (e != 0u) {
            a = pn_mulmod(a, a, m);
        }
    }
    return r;
}

/*
    The Miller-Rabin test with Sinclair's bases: 1 when n is prime, 0
    when it is not, exact for every uint64_t.

    The idea: write n - 1 as 2^s x d with d odd. For a prime n and
    any base a, Fermat's little theorem says a^(n-1) mod n is 1. And
    for a prime, the only numbers whose square is 1 (mod n) are 1 and
    n - 1. So starting from x = a^d and squaring s times, a prime n
    shows either x = 1 at the start, or n - 1 at some step. When
    neither happens, n cannot be prime, and the base a is called a
    "witness" that n is composite.
    https://en.wikipedia.org/wiki/Fermat%27s_little_theorem
*/
static int pn_witness_prime(uint64_t n) {
    uint64_t d = n - 1u;
    unsigned s = 0u, k, r, composite;

    if (n < 2u) {
        return 0;
    }
    if ((n & 1u) == 0u) {
        return n == 2u;
    }
    while ((d & 1u) == 0u) {
        d >>= 1;
        s++;
    }
    for (k = 0u; k < N_WITNESSES; k++) {
        uint64_t a = witnesses[k] % n, x;
        if (a == 0u) {
            continue; /* n divides the base: this base tells us nothing */
        }
        x = pn_powmod(a, d, n);
        if (x == 1u || x == n - 1u) {
            continue;
        }
        composite = 1u;
        for (r = 1u; r < s; r++) {
            x = pn_mulmod(x, x, n);
            if (x == n - 1u) {
                composite = 0u;
                break;
            }
        }
        if (composite != 0u) {
            return 0;
        }
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

/*
    Build the sieve of Eratosthenes on the caller's stack: start with
    every number unmarked, then for each unmarked odd i, mark its odd
    multiples from i x i up as composite (1). What stays 0 is prime.
    https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes
*/
static void pn_sheet(uint8_t* composite) {
    uint64_t i, j;

    memset(composite, 0, PN_MATH_SHEET);
    for (i = 3u; i * i < PN_MATH_SHEET; i += 2u) {
        if (composite[i] == 0u) {
            for (j = i * i; j < PN_MATH_SHEET; j += 2u * i) {
                composite[j] = 1u;
            }
        }
    }
}

/*
    Prime test using a sieve sheet from pn_sheet. Small cases come
    first; then trial division below 2^32, and the Miller-Rabin test
    from 2^32 up.
*/
static int pn_prime_on(const uint8_t* composite, uint64_t n) {
    uint64_t d;
    unsigned k;

    if (n < 2u) {
        return 0;
    }
    if ((n & 1u) == 0u) {
        return n == 2u;
    }
    for (k = 0u; k < N_SMALL; k++) {
        if (n == small_primes[k]) {
            return 1;
        }
        /* divisible by a smaller prime, so composite */
        if ((n % small_primes[k]) == 0u) {
            return 0;
        }
    }
    if (n < PN_MATH_SHEET) {
        return composite[(size_t) n] == 0u;
    }
    if (n < (1ull << 32)) {
        /*
            Trial division by every odd number from 101 (the small
            primes above already covered everything lower). This
            tries some composite divisors too, like 105; they can
            never divide n first, so it costs a little time but
            gives the same answer.
        */
        for (d = 101ull; d * d <= n; d += 2u) {
            if ((n % d) == 0u) {
                return 0;
            }
        }
        /*
            The loop went past the square root without finding a
            divisor, so n is prime. For n just under 2^32 the loop
            can end at d = 65537; those few hand off to Miller-Rabin,
            which gives the same answer.
        */
        if (d <= 0xFFFFull) {
            return 1;
        }
        return pn_witness_prime(n);
    }
    return pn_witness_prime(n);
}

static uint64_t pn_isqrt(uint64_t n) {
    uint64_t res = 0u, bit = 1ull << 62;

    while (bit > n) {
        bit >>= 2;
    }
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

/*
    Pollard's rho method finds a factor of a composite n by following
    the sequence x -> x^2 + c (mod n) until two values in it share a
    factor with n. Usually c is chosen at random; here it tries
    c = 1, 2, 3, ... in order, so the results are repeatable. Some
    choices of c loop without finding a factor; then the next c is
    tried. Returns 0 only if every c failed.
    https://en.wikipedia.org/wiki/Pollard%27s_rho_algorithm
*/
static uint64_t pn_rho_f(uint64_t x, uint64_t c, uint64_t n) {
    return pn_addmod(pn_mulmod(x, x, n), c % n, n);
}

static uint64_t pn_rho_divisor(uint64_t n) {
    uint64_t c;

    if ((n & 1u) == 0u) {
        return 2u;
    }
    for (c = 1ull; c < 128ull; c++) {
        uint64_t x = 2ull + c, y = 2ull + c, g = 1u;
        unsigned guard;
        for (guard = 0u; g == 1u && guard < (1u << 22); guard++) {
            x = pn_rho_f(x, c, n);
            y = pn_rho_f(pn_rho_f(y, c, n), c, n);
            g = pn_gcd(x > y ? x - y : y - x, n);
        }
        if (g > 1u && g < n) {
            return g;
        }
    }
    return 0u;
}

static void pn_factor_hard(uint64_t n, uint64_t* facs, size_t* w) {
    uint64_t f;

    if (n <= 1u) {
        return;
    }
    if (pn_witness_prime(n) != 0) {
        facs[(*w)++] = n;
        return;
    }
    f = pn_rho_divisor(n);
    if (f == 0u) {
        /* every c failed: try dividing by the odd numbers from 101 up to 65535 */
        uint64_t d = 101ull;
        while (d <= 0xFFFFull && (n % d) != 0u) {
            d += 2u;
        }
        f = (d <= 0xFFFFull && (n % d) == 0u) ? d : 0u;
        if (f == 0u) {
            /*
                no divisor below 2^16 and Miller-Rabin says composite: record n unsplit, a known
                limit of this method
            */
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

    if (limit < 2u || limit > PN_MATH_SIEVE_MAX_VALUE) {
        return 0u;
    }
    pn_sheet(composite);
    for (n = 2u; n <= limit; n++) {
        if (pn_prime_on(composite, n) != 0) {
            count++;
        }
    }
    return count;
}

uint64_t* _Nullable pn_math_sieve(uint64_t limit, uint64_t* _Nonnull out, size_t cap,
                                  size_t* _Nonnull count, pn_error* _Nullable* _Nullable error) {
    uint8_t composite[PN_MATH_SHEET];
    size_t need, w;
    uint64_t n;

    assert(out && count);
    if (limit > PN_MATH_SIEVE_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu runs past %llu: the sieve"
                                   " sheet holds no more rows",
                                   (unsigned long long) limit,
                                   (unsigned long long) PN_MATH_SIEVE_MAX_VALUE);
        }
        return NULL;
    }
    need = pn_math_sieve_size(limit);
    if (cap < need) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "the array holds room for %zu"
                                   " prime(s); the sieve would write"
                                   " %zu of them",
                                   cap, need);
        }
        return NULL;
    }
    if (need == 0u) {
        *count = 0u; /* no primes up to limit: nothing to write */
        return out;
    }
    pn_sheet(composite);
    w = 0u;
    for (n = 2u; n <= limit; n++) {
        if (pn_prime_on(composite, n) != 0) {
            out[w++] = n;
        }
    }
    *count = w;
    return out;
}

uint64_t* _Nullable pn_math_gcd(uint64_t a, uint64_t b, uint64_t* _Nonnull out,
                                pn_error* _Nullable* _Nullable error) {
    assert(out);
    (void) error; /* -Wunused-parameter: nothing here can fail */
    *out = pn_gcd(a, b);
    return out;
}

uint64_t* _Nullable pn_math_lcm(uint64_t a, uint64_t b, uint64_t* _Nonnull out,
                                pn_error* _Nullable* _Nullable error) {
    uint64_t g, q;

    assert(out);
    if (a == 0u || b == 0u) {
        *out = 0u; /* lcm(0, n) is 0 by convention */
        return out;
    }
    g = pn_gcd(a, b);
    q = a / g;
    if (q > UINT64_MAX / b) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu times %llu divided by"
                                   " their gcd leaves the ledger:"
                                   " no uint64_t holds the answer",
                                   (unsigned long long) a, (unsigned long long) b);
        }
        return NULL;
    }
    *out = q * b;
    return out;
}

bool* _Nullable pn_math_is_square(uint64_t n, bool* _Nonnull out,
                                  pn_error* _Nullable* _Nullable error) {
    uint64_t x;

    assert(out);
    (void) error; /* -Wunused-parameter: nothing here can fail */
    x = pn_isqrt(n);
    *out = (x * x == n); /* x < 2^32, so x * x can't overflow */
    return out;
}

bool* _Nullable pn_math_is_prime(uint64_t n, bool* _Nonnull out,
                                 pn_error* _Nullable* _Nullable error) {
    uint8_t composite[PN_MATH_SHEET];

    assert(out);
    (void) error; /* -Wunused-parameter: nothing here can fail */
    pn_sheet(composite);
    *out = (pn_prime_on(composite, n) != 0);
    return out;
}

uint64_t* _Nullable pn_math_next_prime(uint64_t n, uint64_t* _Nonnull out,
                                       pn_error* _Nullable* _Nullable error) {
    uint8_t composite[PN_MATH_SHEET];
    uint64_t cand;
    unsigned k;

    assert(out);
    for (k = 0u; k < N_SMALL; k++) {
        if (n <= small_primes[k]) {
            *out = small_primes[k];
            return out;
        }
    }
    pn_sheet(composite);
    if ((n & 1u) == 0u) {
        /* n is even: start at n + 1, since every prime above 2 is odd */
        cand = n + 1u;
    }
    else {
        cand = n; /* n is odd: test n itself, so next_prime(7) is 7 */
    }
    for (;;) {
        if (pn_prime_on(composite, cand) != 0) {
            *out = cand;
            return out;
        }
        if (cand > UINT64_MAX - 2u) {
            if (error) {
                *error = pn_error_init(PN_ERROR_RANGE,
                                       "%llu and every odd number"
                                       " above it run off the end"
                                       " of the ledger",
                                       (unsigned long long) n);
            }
            return NULL;
        }
        cand += 2u;
    }
}

/*
    Insertion sort, smallest first, so factors come out in order.
    https://en.wikipedia.org/wiki/Insertion_sort
*/
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

/*
    Factor n into facs, smallest first, and return how many factors
    there are. n is at least 2 here; 0 and 1 are handled by the
    callers. Small primes are divided out first, then Pollard's rho
    splits what is left.
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
    if (rest > 1u) {
        pn_factor_hard(rest, facs, &w);
    }
    pn_sort_ascending(facs, w);
    return w;
}

size_t pn_math_factorize_size(uint64_t n) {
    uint64_t facs[PN_MATH_MAX_FACTORS];

    if (n < 2u) {
        return 0u; /* 0 and 1 have no prime factors */
    }
    return pn_do_factorize(n, facs);
}

uint64_t* _Nullable pn_math_factorize(uint64_t n, uint64_t* _Nonnull out, size_t cap,
                                      size_t* _Nonnull count,
                                      pn_error* _Nullable* _Nullable error) {
    uint64_t facs[PN_MATH_MAX_FACTORS];
    size_t w;

    assert(out && count);
    if (n < 2u) {
        *count = 0u; /* 0 and 1 have no prime factors */
        return out;
    }
    w = pn_do_factorize(n, facs);
    if (w > cap) {
        if (error) {
            *error = pn_error_init(PN_ERROR_BUFFER,
                                   "the array holds room for %zu"
                                   " factor(s); %llu has %zu of"
                                   " them",
                                   cap, (unsigned long long) n, w);
        }
        return NULL;
    }
    for (size_t i = 0u; i < w; i++) {
        out[i] = facs[i];
    }
    *count = w;
    return out;
}

/*
    The sum of all divisors of an odd n (called sigma(n)), from its
    prime factors: for each prime power p^k in n, multiply in
    1 + p + p^2 + ... + p^k. Returns 0 if the sum doesn't fit in a
    uint64_t. n is perfect when sigma(n) = 2n.
    https://en.wikipedia.org/wiki/Divisor_function
*/
static int pn_sigma_odd_below_gap(uint64_t n, uint64_t* out) {
    uint64_t facs[PN_MATH_MAX_FACTORS];
    size_t count, i;
    uint64_t sigma = 1u;

    count = pn_do_factorize(n, facs);
    for (i = 0u; i < count;) {
        uint64_t p = facs[i], term = 1u, power = 1u;
        while (i < count && facs[i] == p) {
            if (power > UINT64_MAX / p) {
                return 0; /* the sum is too big for a uint64_t */
            }
            power *= p;
            if (term > UINT64_MAX - power) {
                return 0;
            }
            term += power;
            i++;
        }
        if (sigma > UINT64_MAX / term) {
            return 0;
        }
        sigma *= term;
    }
    *out = sigma;
    return 1;
}

bool* _Nullable pn_math_perfect_check(uint64_t n, bool* _Nonnull out,
                                      pn_error* _Nullable* _Nullable error) {
    assert(out);
    if (n < 2u) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "%llu is the blank or the unit:"
                                   " a perfect number is a sum of"
                                   " its own parts",
                                   (unsigned long long) n);
        }
        return NULL;
    }
    if ((n & 1u) == 0u) {
        /*
            The Euclid-Euler theorem: an even number is perfect exactly
            when it equals 2^(k-1) x (2^k - 1) and 2^k - 1 is prime (a
            Mersenne prime). Divide out the factors of 2, check that
            what is left is 2^k - 1, then test it with Miller-Rabin.
            https://en.wikipedia.org/wiki/Euclid%E2%80%93Euler_theorem
        */
        uint64_t twos = 0u, odd = n, mers;

        while ((odd & 1u) == 0u) {
            odd >>= 1;
            twos++;
        }
        if (twos + 1u >= 64u) {
            *out = false; /* 2^k - 1 would not fit in a uint64_t */
            return out;
        }
        mers = (1ull << (twos + 1u)) - 1u;
        if (mers != odd) {
            *out = false; /* not of the form 2^(k-1) x (2^k - 1) */
            return out;
        }
        *out = (pn_witness_prime(mers) != 0);
        return out;
    }
    /*
        Odd n: nobody knows whether any odd perfect number exists,
        but none exists below 10^1500 (Ochem and Rao, 2012), so every
        uint64_t answer here is false. The code still computes the
        divisor sum where it fits instead of relying on that result.
        https://en.wikipedia.org/wiki/Perfect_number#Odd_perfect_numbers
    */
    uint64_t sigma;

    if (!pn_sigma_odd_below_gap(n, &sigma)) {
        /* the divisor sum is too big for a uint64_t, and n is not perfect */
        *out = false;
        return out;
    }
    /*
        if n >= 2^63, 2n wraps around to less than n, which is smaller than sigma, so the answer is
        still correctly false
    */
    *out = (sigma == 2u * n);
    return out;
}

uint64_t* _Nullable pn_math_pythagorean_triple(uint64_t m, uint64_t n, uint64_t* _Nonnull leg1,
                                               uint64_t* _Nonnull leg2,
                                               uint64_t* _Nonnull hypotenuse,
                                               pn_error* _Nullable* _Nullable error) {
    uint64_t mm, a, b, c;

    assert(leg1 && leg2 && hypotenuse);
    if (n < 1u || m <= n || m > PN_MATH_M_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "m and n must satisfy 1 <= n <"
                                   " m <= %llu; m = %llu and n ="
                                   " %llu do not",
                                   (unsigned long long) PN_MATH_M_MAX_VALUE, (unsigned long long) m,
                                   (unsigned long long) n);
        }
        return NULL;
    }
    mm = m * m; /* m < 2^16, so m * m fits easily */
    a = mm - n * n;
    b = 2u * m * n;
    c = mm + n * n;
    if (a > PN_MATH_SIDE_MAX_VALUE || b > PN_MATH_SIDE_MAX_VALUE || c > PN_MATH_SIDE_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "a side of this triangle runs"
                                   " past %llu: longer than the"
                                   " rule measures",
                                   PN_MATH_SIDE_MAX_VALUE);
        }
        return NULL;
    }
    /*
        m^2 - n^2 first, then 2mn, as in Euclid's formula;
        https://en.wikipedia.org/wiki/Pythagorean_triple#Generating_a_triple
    */
    *leg1 = a;
    *leg2 = b;
    *hypotenuse = c;
    return leg1;
}

bool* _Nullable pn_math_pythagorean_check(uint64_t x, uint64_t y, uint64_t z, bool* _Nonnull out,
                                          pn_error* _Nullable* _Nullable error) {
    uint64_t xx, yy, zz, rem;

    assert(out);
    if (x < 1u || y < 1u || z < 1u || x > PN_MATH_SIDE_MAX_VALUE || y > PN_MATH_SIDE_MAX_VALUE
        || z > PN_MATH_SIDE_MAX_VALUE) {
        if (error) {
            *error = pn_error_init(PN_ERROR_RANGE,
                                   "each side must run from 1 to"
                                   " %llu; %llu, %llu, %llu do not"
                                   " all fit the rule",
                                   PN_MATH_SIDE_MAX_VALUE, (unsigned long long) x,
                                   (unsigned long long) y, (unsigned long long) z);
        }
        return NULL;
    }
    xx = x * x; /* each side is below 2^32, so each square fits */
    yy = y * y;
    zz = z * z;
    rem = UINT64_MAX - xx;
    if (yy > rem) {
        /* x^2 + y^2 is too big for a uint64_t, but z^2 fits, so they can't be equal */
        *out = false;
        return out;
    }
    *out = ((xx + yy) == zz);
    return out;
}
