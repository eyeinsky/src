/*	$OpenBSD: rnd.c,v 1.160 2014/09/15 22:00:24 tedu Exp $	*/

/*
 * Copyright (c) 2011 Theo de Raadt.
 * Copyright (c) 2008 Damien Miller.
 * Copyright (c) 1996, 1997, 2000-2002 Michael Shalayeff.
 * Copyright (c) 2013 Markus Friedl.
 * Copyright Theodore Ts'o, 1994, 1995, 1996, 1997, 1998, 1999.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Computers are very predictable devices.  Hence it is extremely hard
 * to produce truly random numbers on a computer --- as opposed to
 * pseudo-random numbers, which can be easily generated by using an
 * algorithm.  Unfortunately, it is very easy for attackers to guess
 * the sequence of pseudo-random number generators, and for some
 * applications this is not acceptable.  Instead, we must try to
 * gather "environmental noise" from the computer's environment, which
 * must be hard for outside attackers to observe and use to
 * generate random numbers.  In a Unix environment, this is best done
 * from inside the kernel.
 *
 * Sources of randomness from the environment include inter-keyboard
 * timings, inter-interrupt timings from some interrupts, and other
 * events which are both (a) non-deterministic and (b) hard for an
 * outside observer to measure.  Randomness from these sources is
 * added to the "rnd states" queue; this is used as much of the
 * source material which is mixed on occasion using a CRC-like function
 * into the "entropy pool".  This is not cryptographically strong, but
 * it is adequate assuming the randomness is not chosen maliciously,
 * and it very fast because the interrupt-time event is only to add
 * a small random token to the "rnd states" queue.
 *
 * When random bytes are desired, they are obtained by pulling from
 * the entropy pool and running a MD5 hash. The MD5 hash avoids
 * exposing the internal state of the entropy pool.  Even if it is
 * possible to analyze MD5 in some clever way, as long as the amount
 * of data returned from the generator is less than the inherent
 * entropy in the pool, the output data is totally unpredictable.  For
 * this reason, the routine decreases its internal estimate of how many
 * bits of "true randomness" are contained in the entropy pool as it
 * outputs random numbers.
 *
 * If this estimate goes to zero, the MD5 hash will continue to generate
 * output since there is no true risk because the MD5 output is not
 * exported outside this subsystem.  It is next used as input to seed a
 * ChaCha20 stream cipher, which is re-seeded from time to time.  This
 * design provides very high amounts of output data from a potentially
 * small entropy base, at high enough speeds to encourage use of random
 * numbers in nearly any situation.  Before OpenBSD 5.5, the RC4 stream
 * cipher (also known as ARC4) was used instead of ChaCha20.
 *
 * The output of this single ChaCha20 engine is then shared amongst many
 * consumers in the kernel and userland via a few interfaces:
 * arc4random_buf(), arc4random(), arc4random_uniform(), randomread()
 * for the set of /dev/random nodes, the sysctl kern.arandom, and the
 * system call getentropy(), which provides seeds for process-context
 * pseudorandom generators.
 *
 * Acknowledgements:
 * =================
 *
 * Ideas for constructing this random number generator were derived
 * from Pretty Good Privacy's random number generator, and from private
 * discussions with Phil Karn.  Colin Plumb provided a faster random
 * number generator, which speeds up the mixing function of the entropy
 * pool, taken from PGPfone.  Dale Worley has also contributed many
 * useful ideas and suggestions to improve this driver.
 *
 * Any flaws in the design are solely my responsibility, and should
 * not be attributed to the Phil, Colin, or any of the authors of PGP.
 *
 * Further background information on this topic may be obtained from
 * RFC 1750, "Randomness Recommendations for Security", by Donald
 * Eastlake, Steve Crocker, and Jeff Schiller.
 *
 * Using a RC4 stream cipher as 2nd stage after the MD5 output
 * is the result of work by David Mazieres.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/event.h>
#include <sys/limits.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/timeout.h>
#include <sys/mutex.h>
#include <sys/task.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <crypto/md5.h>

#define KEYSTREAM_ONLY
#include <crypto/chacha_private.h>

#include <dev/rndvar.h>

/*
 * For the purposes of better mixing, we use the CRC-32 polynomial as
 * well to make a twisted Generalized Feedback Shift Register
 *
 * (See M. Matsumoto & Y. Kurita, 1992.  Twisted GFSR generators.  ACM
 * Transactions on Modeling and Computer Simulation 2(3):179-194.
 * Also see M. Matsumoto & Y. Kurita, 1994.  Twisted GFSR generators
 * II.  ACM Transactions on Mdeling and Computer Simulation 4:254-266)
 *
 * Thanks to Colin Plumb for suggesting this.
 *
 * We have not analyzed the resultant polynomial to prove it primitive;
 * in fact it almost certainly isn't.  Nonetheless, the irreducible factors
 * of a random large-degree polynomial over GF(2) are more than large enough
 * that periodicity is not a concern.
 *
 * The input hash is much less sensitive than the output hash.  All
 * we want from it is to be a good non-cryptographic hash -
 * i.e. to not produce collisions when fed "random" data of the sort
 * we expect to see.  As long as the pool state differs for different
 * inputs, we have preserved the input entropy and done a good job.
 * The fact that an intelligent attacker can construct inputs that
 * will produce controlled alterations to the pool's state is not
 * important because we don't consider such inputs to contribute any
 * randomness.  The only property we need with respect to them is that
 * the attacker can't increase his/her knowledge of the pool's state.
 * Since all additions are reversible (knowing the final state and the
 * input, you can reconstruct the initial state), if an attacker has
 * any uncertainty about the initial state, he/she can only shuffle
 * that uncertainty about, but never cause any collisions (which would
 * decrease the uncertainty).
 *
 * The chosen system lets the state of the pool be (essentially) the input
 * modulo the generator polynomial.  Now, for random primitive polynomials,
 * this is a universal class of hash functions, meaning that the chance
 * of a collision is limited by the attacker's knowledge of the generator
 * polynomial, so if it is chosen at random, an attacker can never force
 * a collision.  Here, we use a fixed polynomial, but we *can* assume that
 * ###--> it is unknown to the processes generating the input entropy. <-###
 * Because of this important property, this is a good, collision-resistant
 * hash; hash collisions will occur no more often than chance.
 */

/*
 * Stirring polynomials over GF(2) for various pool sizes. Used in
 * add_entropy_words() below.
 *
 * The polynomial terms are chosen to be evenly spaced (minimum RMS
 * distance from evenly spaced; except for the last tap, which is 1 to
 * get the twisting happening as fast as possible.
 *
 * The reultant polynomial is:
 *   2^POOLWORDS + 2^POOL_TAP1 + 2^POOL_TAP2 + 2^POOL_TAP3 + 2^POOL_TAP4 + 1
 */
#define POOLWORDS	2048
#define POOLBYTES	(POOLWORDS*4)
#define POOLMASK	(POOLWORDS - 1)
#define	POOL_TAP1	1638
#define	POOL_TAP2	1231
#define	POOL_TAP3	819
#define	POOL_TAP4	411

struct mutex entropylock = MUTEX_INITIALIZER(IPL_HIGH);

/*
 * Raw entropy collection from device drivers; at interrupt context or not.
 * add_*_randomness() provide data which is put into the entropy queue.
 * Almost completely under the entropylock.
 */
struct timer_rand_state {	/* There is one of these per entropy source */
	u_int	last_time;
	u_int	last_delta;
	u_int	last_delta2;
	u_int	dont_count_entropy : 1;
	u_int	max_entropy : 1;
} rnd_states[RND_SRC_NUM];

#define QEVLEN (1024 / sizeof(struct rand_event))
#define QEVSLOW (QEVLEN * 3 / 4) /* yet another 0.75 for 60-minutes hour /-; */
#define QEVSBITS 10

struct rand_event {
	struct timer_rand_state *re_state;
	u_int re_nbits;
	u_int re_time;
	u_int re_val;
} rnd_event_space[QEVLEN];
struct rand_event *rnd_event_head = rnd_event_space;
struct rand_event *rnd_event_tail = rnd_event_space;

struct timeout rnd_timeout;
struct rndstats rndstats;

u_int32_t entropy_pool[POOLWORDS] __attribute__((section(".openbsd.randomdata")));
u_int	entropy_add_ptr;
u_char	entropy_input_rotate;

void	dequeue_randomness(void *);
void	add_entropy_words(const u_int32_t *, u_int);
void	extract_entropy(u_int8_t *, int);

int	filt_randomread(struct knote *, long);
void	filt_randomdetach(struct knote *);
int	filt_randomwrite(struct knote *, long);

struct filterops randomread_filtops =
	{ 1, NULL, filt_randomdetach, filt_randomread };
struct filterops randomwrite_filtops =
	{ 1, NULL, filt_randomdetach, filt_randomwrite };

static __inline struct rand_event *
rnd_get(void)
{
	struct rand_event *p = rnd_event_tail;

	if (p == rnd_event_head)
		return NULL;

	if (p + 1 >= &rnd_event_space[QEVLEN])
		rnd_event_tail = rnd_event_space;
	else
		rnd_event_tail++;

	return p;
}

static __inline struct rand_event *
rnd_put(void)
{
	struct rand_event *p = rnd_event_head + 1;

	if (p >= &rnd_event_space[QEVLEN])
		p = rnd_event_space;

	if (p == rnd_event_tail)
		return NULL;

	return rnd_event_head = p;
}

static __inline int
rnd_qlen(void)
{
	int len = rnd_event_head - rnd_event_tail;
	return (len < 0)? -len : len;
}

/*
 * This function adds entropy to the entropy pool by using timing
 * delays.  It uses the timer_rand_state structure to make an estimate
 * of how many bits of entropy this call has added to the pool.
 *
 * The number "val" is also added to the pool - it should somehow describe
 * the type of event which just happened.  Currently the values of 0-255
 * are for keyboard scan codes, 256 and upwards - for interrupts.
 * On the i386, this is assumed to be at most 16 bits, and the high bits
 * are used for a high-resolution timer.
 */
void
enqueue_randomness(int state, int val)
{
	int	delta, delta2, delta3;
	struct timer_rand_state *p;
	struct rand_event *rep;
	struct timespec	ts;
	u_int	time, nbits;

#ifdef DIAGNOSTIC
	if (state < 0 || state >= RND_SRC_NUM)
		return;
#endif

	if (timeout_initialized(&rnd_timeout))
		nanotime(&ts);

	p = &rnd_states[state];
	val += state << 13;

	time = (ts.tv_nsec >> 10) + (ts.tv_sec << 20);
	nbits = 0;

	/*
	 * Calculate the number of bits of randomness that we probably
	 * added.  We take into account the first and second order
	 * deltas in order to make our estimate.
	 */
	if (!p->dont_count_entropy) {
		delta  = time   - p->last_time;
		delta2 = delta  - p->last_delta;
		delta3 = delta2 - p->last_delta2;

		if (delta < 0) delta = -delta;
		if (delta2 < 0) delta2 = -delta2;
		if (delta3 < 0) delta3 = -delta3;
		if (delta > delta2) delta = delta2;
		if (delta > delta3) delta = delta3;
		delta3 = delta >>= 1;
		/*
		 * delta &= 0xfff;
		 * we don't do it since our time sheet is different from linux
		 */

		if (delta & 0xffff0000) {
			nbits = 16;
			delta >>= 16;
		}
		if (delta & 0xff00) {
			nbits += 8;
			delta >>= 8;
		}
		if (delta & 0xf0) {
			nbits += 4;
			delta >>= 4;
		}
		if (delta & 0xc) {
			nbits += 2;
			delta >>= 2;
		}
		if (delta & 2) {
			nbits += 1;
			delta >>= 1;
		}
		if (delta & 1)
			nbits++;
	} else if (p->max_entropy)
		nbits = 8 * sizeof(val) - 1;

	/* given the multi-order delta logic above, this should never happen */
	if (nbits >= 32)
		return;

	mtx_enter(&entropylock);
	if (!p->dont_count_entropy) {
		/*
		 * the logic is to drop low-entropy entries,
		 * in hope for dequeuing to be more randomfull
		 */
		if (rnd_qlen() > QEVSLOW && nbits < QEVSBITS) {
			rndstats.rnd_drople++;
			goto done;
		}
		p->last_time = time;
		p->last_delta  = delta3;
		p->last_delta2 = delta2;
	}

	if ((rep = rnd_put()) == NULL) {
		rndstats.rnd_drops++;
		goto done;
	}

	rep->re_state = p;
	rep->re_nbits = nbits;
	rep->re_time = ts.tv_nsec ^ (ts.tv_sec << 20);
	rep->re_val = val;

	rndstats.rnd_enqs++;
	rndstats.rnd_ed[nbits]++;
	rndstats.rnd_sc[state]++;
	rndstats.rnd_sb[state] += nbits;

	if (rnd_qlen() > QEVSLOW/2 && timeout_initialized(&rnd_timeout) &&
	    !timeout_pending(&rnd_timeout))
		timeout_add(&rnd_timeout, 1);
done:
	mtx_leave(&entropylock);
}

/*
 * This function adds a byte into the entropy pool.  It does not
 * update the entropy estimate.  The caller must do this if appropriate.
 *
 * The pool is stirred with a polynomial of degree POOLWORDS over GF(2);
 * see POOL_TAP[1-4] above
 *
 * Rotate the input word by a changing number of bits, to help assure
 * that all bits in the entropy get toggled.  Otherwise, if the pool
 * is consistently fed small numbers (such as keyboard scan codes)
 * then the upper bits of the entropy pool will frequently remain
 * untouched.
 */
void
add_entropy_words(const u_int32_t *buf, u_int n)
{
	/* derived from IEEE 802.3 CRC-32 */
	static const u_int32_t twist_table[8] = {
		0x00000000, 0x3b6e20c8, 0x76dc4190, 0x4db26158,
		0xedb88320, 0xd6d6a3e8, 0x9b64c2b0, 0xa00ae278
	};

	for (; n--; buf++) {
		u_int32_t w = (*buf << entropy_input_rotate) |
		    (*buf >> (32 - entropy_input_rotate));
		u_int i = entropy_add_ptr =
		    (entropy_add_ptr - 1) & POOLMASK;
		/*
		 * Normally, we add 7 bits of rotation to the pool.
		 * At the beginning of the pool, add an extra 7 bits
		 * rotation, so that successive passes spread the
		 * input bits across the pool evenly.
		 */
		entropy_input_rotate =
		    (entropy_input_rotate + (i ? 7 : 14)) & 31;

		/* XOR pool contents corresponding to polynomial terms */
		w ^= entropy_pool[(i + POOL_TAP1) & POOLMASK] ^
		     entropy_pool[(i + POOL_TAP2) & POOLMASK] ^
		     entropy_pool[(i + POOL_TAP3) & POOLMASK] ^
		     entropy_pool[(i + POOL_TAP4) & POOLMASK] ^
		     entropy_pool[(i + 1) & POOLMASK] ^
		     entropy_pool[i]; /* + 2^POOLWORDS */

		entropy_pool[i] = (w >> 3) ^ twist_table[w & 7];
	}
}

/*
 * Pulls entropy out of the queue and throws merges it into the pool
 * with the CRC.
 */
/* ARGSUSED */
void
dequeue_randomness(void *v)
{
	struct rand_event *rep;
	u_int32_t buf[2];
	u_int nbits;

	mtx_enter(&entropylock);

	if (timeout_initialized(&rnd_timeout))
		timeout_del(&rnd_timeout);

	rndstats.rnd_deqs++;
	while ((rep = rnd_get())) {
		buf[0] = rep->re_time;
		buf[1] = rep->re_val;
		nbits = rep->re_nbits;
		mtx_leave(&entropylock);

		add_entropy_words(buf, 2);

		mtx_enter(&entropylock);
		rndstats.rnd_total += nbits;
	}
	mtx_leave(&entropylock);
}

/*
 * Grabs a chunk from the entropy_pool[] and slams it through MD5 when
 * requested.
 */
void
extract_entropy(u_int8_t *buf, int nbytes)
{
	static u_int32_t extract_pool[POOLWORDS];
	u_char buffer[MD5_DIGEST_LENGTH];
	MD5_CTX tmp;
	u_int i;

	add_timer_randomness(nbytes);

	while (nbytes) {
		i = MIN(nbytes, sizeof(buffer));

		/*
		 * INTENTIONALLY not protected by entropylock.  Races
		 * during bcopy() result in acceptable input data; races
		 * during MD5Update() would create nasty data dependencies.
		 */
		bcopy(entropy_pool, extract_pool,
		    sizeof(extract_pool));

		/* Hash the pool to get the output */
		MD5Init(&tmp);
		MD5Update(&tmp, (u_int8_t *)extract_pool, sizeof(extract_pool));
		MD5Final(buffer, &tmp);

		/* Copy data to destination buffer */
		bcopy(buffer, buf, i);
		nbytes -= i;
		buf += i;

		/* Modify pool so next hash will produce different results */
		add_timer_randomness(nbytes);
		dequeue_randomness(NULL);
	}

	/* Wipe data from memory */
	explicit_bzero(extract_pool, sizeof(extract_pool));
	explicit_bzero(&tmp, sizeof(tmp));
	explicit_bzero(buffer, sizeof(buffer));
}

/* random keystream by ChaCha */

struct mutex rndlock = MUTEX_INITIALIZER(IPL_HIGH);
struct timeout arc4_timeout;
struct task arc4_task;

void arc4_reinit(void *v);		/* timeout to start reinit */
void arc4_init(void *, void *);		/* actually do the reinit */

#define KEYSZ	32
#define IVSZ	8
#define BLOCKSZ	64
#define RSBUFSZ	(16*BLOCKSZ)
static int rs_initialized;
static chacha_ctx rs;		/* chacha context for random keystream */
/* keystream blocks (also chacha seed from boot) */
static u_char rs_buf[RSBUFSZ] __attribute__((section(".openbsd.randomdata")));
static size_t rs_have;		/* valid bytes at end of rs_buf */
static size_t rs_count;		/* bytes till reseed */

static inline void _rs_rekey(u_char *dat, size_t datlen);

static inline void
_rs_init(u_char *buf, size_t n)
{
	KASSERT(n >= KEYSZ + IVSZ);
	chacha_keysetup(&rs, buf, KEYSZ * 8, 0);
	chacha_ivsetup(&rs, buf + KEYSZ);
}

static void
_rs_seed(u_char *buf, size_t n)
{
	_rs_rekey(buf, n);

	/* invalidate rs_buf */
	rs_have = 0;
	memset(rs_buf, 0, RSBUFSZ);

	rs_count = 1600000;
}

static void
_rs_stir(int do_lock)
{
	struct timespec ts;
	u_int8_t buf[KEYSZ + IVSZ], *p;
	int i;

	/*
	 * Use MD5 PRNG data and a system timespec; early in the boot
	 * process this is the best we can do -- some architectures do
	 * not collect entropy very well during this time, but may have
	 * clock information which is better than nothing.
	 */
	extract_entropy((u_int8_t *)buf, sizeof buf);

	nanotime(&ts);
	for (p = (u_int8_t *)&ts, i = 0; i < sizeof(ts); i++)
		buf[i] ^= p[i];

	if (do_lock)
		mtx_enter(&rndlock);
	_rs_seed(buf, sizeof(buf));
	rndstats.arc4_nstirs++;
	if (do_lock)
		mtx_leave(&rndlock);

	explicit_bzero(buf, sizeof(buf));
}

static inline void
_rs_stir_if_needed(size_t len)
{
	if (!rs_initialized) {
		_rs_init(rs_buf, KEYSZ + IVSZ);
		rs_count = 1024 * 1024 * 1024;	/* until main() runs */
		rs_initialized = 1;
	} else if (rs_count <= len)
		_rs_stir(0);
	else
		rs_count -= len;
}

static inline void
_rs_rekey(u_char *dat, size_t datlen)
{
#ifndef KEYSTREAM_ONLY
	memset(rs_buf, 0, RSBUFSZ);
#endif
	/* fill rs_buf with the keystream */
	chacha_encrypt_bytes(&rs, rs_buf, rs_buf, RSBUFSZ);
	/* mix in optional user provided data */
	if (dat) {
		size_t i, m;

		m = MIN(datlen, KEYSZ + IVSZ);
		for (i = 0; i < m; i++)
			rs_buf[i] ^= dat[i];
	}
	/* immediately reinit for backtracking resistance */
	_rs_init(rs_buf, KEYSZ + IVSZ);
	memset(rs_buf, 0, KEYSZ + IVSZ);
	rs_have = RSBUFSZ - KEYSZ - IVSZ;
}

static inline void
_rs_random_buf(void *_buf, size_t n)
{
	u_char *buf = (u_char *)_buf;
	size_t m;

	_rs_stir_if_needed(n);
	while (n > 0) {
		if (rs_have > 0) {
			m = MIN(n, rs_have);
			memcpy(buf, rs_buf + RSBUFSZ - rs_have, m);
			memset(rs_buf + RSBUFSZ - rs_have, 0, m);
			buf += m;
			n -= m;
			rs_have -= m;
		}
		if (rs_have == 0)
			_rs_rekey(NULL, 0);
	}
}

static inline void
_rs_random_u32(u_int32_t *val)
{
	_rs_stir_if_needed(sizeof(*val));
	if (rs_have < sizeof(*val))
		_rs_rekey(NULL, 0);
	memcpy(val, rs_buf + RSBUFSZ - rs_have, sizeof(*val));
	memset(rs_buf + RSBUFSZ - rs_have, 0, sizeof(*val));
	rs_have -= sizeof(*val);
	return;
}

/* Return one word of randomness from a ChaCha20 generator */
u_int32_t
arc4random(void)
{
	u_int32_t ret;

	mtx_enter(&rndlock);
	_rs_random_u32(&ret);
	rndstats.arc4_reads += sizeof(ret);
	mtx_leave(&rndlock);
	return ret;
}

/*
 * Fill a buffer of arbitrary length with ChaCha20-derived randomness.
 */
void
arc4random_buf(void *buf, size_t n)
{
	mtx_enter(&rndlock);
	_rs_random_buf(buf, n);
	rndstats.arc4_reads += n;
	mtx_leave(&rndlock);
}

/*
 * Calculate a uniformly distributed random number less than upper_bound
 * avoiding "modulo bias".
 *
 * Uniformity is achieved by generating new random numbers until the one
 * returned is outside the range [0, 2**32 % upper_bound).  This
 * guarantees the selected random number will be inside
 * [2**32 % upper_bound, 2**32) which maps back to [0, upper_bound)
 * after reduction modulo upper_bound.
 */
u_int32_t
arc4random_uniform(u_int32_t upper_bound)
{
	u_int32_t r, min;

	if (upper_bound < 2)
		return 0;

	/* 2**32 % x == (2**32 - x) % x */
	min = -upper_bound % upper_bound;

	/*
	 * This could theoretically loop forever but each retry has
	 * p > 0.5 (worst case, usually far better) of selecting a
	 * number inside the range we need, so it should rarely need
	 * to re-roll.
	 */
	for (;;) {
		r = arc4random();
		if (r >= min)
			break;
	}

	return r % upper_bound;
}

/* ARGSUSED */
void
arc4_init(void *v, void *w)
{
	_rs_stir(1);
}

/*
 * Called by timeout to mark arc4 for stirring,
 */
void
arc4_reinit(void *v)
{
	task_add(systq, &arc4_task);
	/* 10 minutes, per dm@'s suggestion */
	timeout_add_sec(&arc4_timeout, 10 * 60);
}

/*
 * Start periodic services inside the random subsystem, which pull
 * entropy forward, hash it, and re-seed the random stream as needed.
 */
void
random_start(void)
{
#if !defined(NO_PROPOLICE)
	extern long __guard_local;

	if (__guard_local == 0)
		printf("warning: no entropy supplied by boot loader\n");
#endif

	rnd_states[RND_SRC_TIMER].dont_count_entropy = 1;
	rnd_states[RND_SRC_TRUE].dont_count_entropy = 1;
	rnd_states[RND_SRC_TRUE].max_entropy = 1;

	/* Provide some data from this kernel */
	add_entropy_words((u_int32_t *)version,
	    strlen(version) / sizeof(u_int32_t));

	/* Provide some data from this kernel */
	add_entropy_words((u_int32_t *)cfdata,
	    8192 / sizeof(u_int32_t));

	/* Message buffer may contain data from previous boot */
	if (msgbufp->msg_magic == MSG_MAGIC)
		add_entropy_words((u_int32_t *)msgbufp->msg_bufc,
		    msgbufp->msg_bufs / sizeof(u_int32_t));

	rs_initialized = 1;
	dequeue_randomness(NULL);
	arc4_init(NULL, NULL);
	task_set(&arc4_task, arc4_init, NULL, NULL);
	timeout_set(&arc4_timeout, arc4_reinit, NULL);
	arc4_reinit(NULL);
	timeout_set(&rnd_timeout, dequeue_randomness, NULL);
}

int
randomopen(dev_t dev, int flag, int mode, struct proc *p)
{
	return 0;
}

int
randomclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return 0;
}

/*
 * Maximum number of bytes to serve directly from the main ChaCha
 * pool. Larger requests are served from a discrete ChaCha instance keyed
 * from the main pool.
 */
#define ARC4_MAIN_MAX_BYTES	2048

int
randomread(dev_t dev, struct uio *uio, int ioflag)
{
	u_char		lbuf[KEYSZ+IVSZ];
	chacha_ctx	lctx;
	size_t		total = uio->uio_resid;
	u_char		*buf;
	int		myctx = 0, ret = 0;

	if (uio->uio_resid == 0)
		return 0;

	buf = malloc(POOLBYTES, M_TEMP, M_WAITOK);
	if (total > ARC4_MAIN_MAX_BYTES) {
		arc4random_buf(lbuf, sizeof(lbuf));
		chacha_keysetup(&lctx, lbuf, KEYSZ * 8, 0);
		chacha_ivsetup(&lctx, lbuf + KEYSZ);
		explicit_bzero(lbuf, sizeof(lbuf));
		myctx = 1;
	}

	while (ret == 0 && uio->uio_resid > 0) {
		int	n = min(POOLBYTES, uio->uio_resid);

		if (myctx) {
#ifndef KEYSTREAM_ONLY
			memset(buf, 0, n);
#endif
			chacha_encrypt_bytes(&lctx, buf, buf, n);
		} else
			arc4random_buf(buf, n);
		ret = uiomove(buf, n, uio);
		if (ret == 0 && uio->uio_resid > 0)
			yield();
	}
	if (myctx)
		explicit_bzero(&lctx, sizeof(lctx));
	explicit_bzero(buf, POOLBYTES);
	free(buf, M_TEMP, 0);
	return ret;
}

int
randomwrite(dev_t dev, struct uio *uio, int flags)
{
	int		ret = 0, newdata = 0;
	u_int32_t	*buf;

	if (uio->uio_resid == 0)
		return 0;

	buf = malloc(POOLBYTES, M_TEMP, M_WAITOK);

	while (ret == 0 && uio->uio_resid > 0) {
		int	n = min(POOLBYTES, uio->uio_resid);

		ret = uiomove(buf, n, uio);
		if (ret != 0)
			break;
		while (n % sizeof(u_int32_t))
			((u_int8_t *)buf)[n++] = 0;
		add_entropy_words(buf, n / 4);
		if (uio->uio_resid > 0)
			yield();
		newdata = 1;
	}

	if (newdata)
		arc4_init(NULL, NULL);

	explicit_bzero(buf, POOLBYTES);
	free(buf, M_TEMP, 0);
	return ret;
}

int
randomkqfilter(dev_t dev, struct knote *kn)
{
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &randomread_filtops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &randomwrite_filtops;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

void
filt_randomdetach(struct knote *kn)
{
}

int
filt_randomread(struct knote *kn, long hint)
{
	kn->kn_data = ARC4_MAIN_MAX_BYTES;
	return (1);
}

int
filt_randomwrite(struct knote *kn, long hint)
{
	kn->kn_data = POOLBYTES;
	return (1);
}

int
randomioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	case FIOASYNC:
		/* No async flag in softc so this is a no-op. */
		break;
	case FIONBIO:
		/* Handled in the upper FS layer. */
		break;
	default:
		return ENOTTY;
	}
	return 0;
}

int
sys_getentropy(struct proc *p, void *v, register_t *retval)
{
	struct sys_getentropy_args /* {
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
	} */ *uap = v;
	char buf[256];
	int error;

	if (SCARG(uap, nbyte) > sizeof(buf))
		return (EIO);
	arc4random_buf(buf, SCARG(uap, nbyte));
	if ((error = copyout(buf, SCARG(uap, buf), SCARG(uap, nbyte))) != 0)
		return (error);
	explicit_bzero(buf, sizeof(buf));
	retval[0] = 0;
	return (0);
}
