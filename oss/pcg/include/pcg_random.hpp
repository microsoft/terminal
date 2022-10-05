/*
 * PCG Random Number Generation for C++
 *
 * Copyright 2014-2022 Melissa O'Neill <oneill@pcg-random.org>,
 *                     and the PCG Project contributors.
 *
 * SPDX-License-Identifier: (Apache-2.0 OR MIT)
 *
 * Licensed under the Apache License, Version 2.0 (provided in
 * LICENSE-APACHE.txt and at http://www.apache.org/licenses/LICENSE-2.0)
 * or under the MIT license (provided in LICENSE-MIT.txt and at
 * http://opensource.org/licenses/MIT), at your option. This file may not
 * be copied, modified, or distributed except according to those terms.
 *
 * Distributed on an "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied.  See your chosen license for details.
 *
 * For additional information about the PCG random number generation scheme,
 * visit http://www.pcg-random.org/.
 */

/*
 * This code provides the reference implementation of the PCG family of
 * random number generators.  The code is complex because it implements
 *
 *      - several members of the PCG family, specifically members corresponding
 *        to the output functions:
 *             - XSH RR         (good for 64-bit state, 32-bit output)
 *             - XSH RS         (good for 64-bit state, 32-bit output)
 *             - XSL RR         (good for 128-bit state, 64-bit output)
 *             - RXS M XS       (statistically most powerful generator)
 *             - XSL RR RR      (good for 128-bit state, 128-bit output)
 *             - and RXS, RXS M, XSH, XSL       (mostly for testing)
 *      - at potentially *arbitrary* bit sizes
 *      - with four different techniques for random streams (MCG, one-stream
 *        LCG, settable-stream LCG, unique-stream LCG)
 *      - and the extended generation schemes allowing arbitrary periods
 *      - with all features of C++11 random number generation (and more),
 *        some of which are somewhat painful, including
 *            - initializing with a SeedSequence which writes 32-bit values
 *              to memory, even though the state of the generator may not
 *              use 32-bit values (it might use smaller or larger integers)
 *            - I/O for RNGs and a prescribed format, which needs to handle
 *              the issue that 8-bit and 128-bit integers don't have working
 *              I/O routines (e.g., normally 8-bit = char, not integer)
 *            - equality and inequality for RNGs
 *      - and a number of convenience typedefs to mask all the complexity
 *
 * The code employees a fairly heavy level of abstraction, and has to deal
 * with various C++ minutia.  If you're looking to learn about how the PCG
 * scheme works, you're probably best of starting with one of the other
 * codebases (see www.pcg-random.org).  But if you're curious about the
 * constants for the various output functions used in those other, simpler,
 * codebases, this code shows how they are calculated.
 *
 * On the positive side, at least there are convenience typedefs so that you
 * can say
 *
 *      pcg32 myRNG;
 *
 * rather than:
 *
 *      pcg_detail::engine<
 *          uint32_t,                                           // Output Type
 *          uint64_t,                                           // State Type
 *          pcg_detail::xsh_rr_mixin<uint32_t, uint64_t>, true, // Output Func
 *          pcg_detail::specific_stream<uint64_t>,              // Stream Kind
 *          pcg_detail::default_multiplier<uint64_t>            // LCG Mult
 *      > myRNG;
 *
 */

#ifndef PCG_RAND_HPP_INCLUDED
#define PCG_RAND_HPP_INCLUDED 1

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <limits>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <utility>
#include <locale>
#include <new>
#include <stdexcept>

#ifdef _MSC_VER
    #pragma warning(disable:4146)
#endif

#ifdef _MSC_VER
    #define PCG_ALWAYS_INLINE __forceinline
#elif __GNUC__
    #define PCG_ALWAYS_INLINE __attribute__((always_inline))
#else
    #define PCG_ALWAYS_INLINE inline
#endif

/*
 * The pcg_extras namespace contains some support code that is likely to
 * be useful for a variety of RNGs, including:
 *      - 128-bit int support for platforms where it isn't available natively
 *      - bit twiddling operations
 *      - I/O of 128-bit and 8-bit integers
 *      - Handling the evilness of SeedSeq
 *      - Support for efficiently producing random numbers less than a given
 *        bound
 */

#include "pcg_extras.hpp"

namespace pcg_detail {

using namespace pcg_extras;

/*
 * The LCG generators need some constants to function.  This code lets you
 * look up the constant by *type*.  For example
 *
 *      default_multiplier<uint32_t>::multiplier()
 *
 * gives you the default multiplier for 32-bit integers.  We use the name
 * of the constant and not a generic word like value to allow these classes
 * to be used as mixins.
 */

template <typename T>
struct default_multiplier {
    // Not defined for an arbitrary type
};

template <typename T>
struct default_increment {
    // Not defined for an arbitrary type
};

#define PCG_DEFINE_CONSTANT(type, what, kind, constant) \
        template <>                                     \
        struct what ## _ ## kind<type> {                \
            static constexpr type kind() {              \
                return constant;                        \
            }                                           \
        };

PCG_DEFINE_CONSTANT(uint8_t,  default, multiplier, 141U)
PCG_DEFINE_CONSTANT(uint8_t,  default, increment,  77U)

PCG_DEFINE_CONSTANT(uint16_t, default, multiplier, 12829U)
PCG_DEFINE_CONSTANT(uint16_t, default, increment,  47989U)

PCG_DEFINE_CONSTANT(uint32_t, default, multiplier, 747796405U)
PCG_DEFINE_CONSTANT(uint32_t, default, increment,  2891336453U)

PCG_DEFINE_CONSTANT(uint64_t, default, multiplier, 6364136223846793005ULL)
PCG_DEFINE_CONSTANT(uint64_t, default, increment,  1442695040888963407ULL)

PCG_DEFINE_CONSTANT(pcg128_t, default, multiplier,
        PCG_128BIT_CONSTANT(2549297995355413924ULL,4865540595714422341ULL))
PCG_DEFINE_CONSTANT(pcg128_t, default, increment,
        PCG_128BIT_CONSTANT(6364136223846793005ULL,1442695040888963407ULL))

/* Alternative (cheaper) multipliers for 128-bit */

template <typename T>
struct cheap_multiplier : public default_multiplier<T> {
    // For most types just use the default.
};

template <>
struct cheap_multiplier<pcg128_t> {
    static constexpr uint64_t multiplier() {
        return 0xda942042e4dd58b5ULL;
    }
};


/*
 * Each PCG generator is available in four variants, based on how it applies
 * the additive constant for its underlying LCG; the variations are:
 *
 *     single stream   - all instances use the same fixed constant, thus
 *                       the RNG always somewhere in same sequence
 *     mcg             - adds zero, resulting in a single stream and reduced
 *                       period
 *     specific stream - the constant can be changed at any time, selecting
 *                       a different random sequence
 *     unique stream   - the constant is based on the memory address of the
 *                       object, thus every RNG has its own unique sequence
 *
 * This variation is provided though mixin classes which define a function
 * value called increment() that returns the necessary additive constant.
 */



/*
 * unique stream
 */


template <typename itype>
class unique_stream {
protected:
    static constexpr bool is_mcg = false;

    // Is never called, but is provided for symmetry with specific_stream
    void set_stream(...)
    {
        abort();
    }

public:
    typedef itype state_type;

    constexpr itype increment() const {
        return itype(reinterpret_cast<uintptr_t>(this) | 1);
    }

    constexpr itype stream() const
    {
         return increment() >> 1;
    }

    static constexpr bool can_specify_stream = false;

    static constexpr size_t streams_pow2()
    {
        return (sizeof(itype) < sizeof(size_t) ? sizeof(itype)
                                               : sizeof(size_t))*8 - 1u;
    }

protected:
    constexpr unique_stream() = default;
};


/*
 * no stream (mcg)
 */

template <typename itype>
class no_stream {
protected:
    static constexpr bool is_mcg = true;

    // Is never called, but is provided for symmetry with specific_stream
    void set_stream(...)
    {
        abort();
    }

public:
    typedef itype state_type;

    static constexpr itype increment() {
        return 0;
    }

    static constexpr bool can_specify_stream = false;

    static constexpr size_t streams_pow2()
    {
        return 0u;
    }

protected:
    constexpr no_stream() = default;
};


/*
 * single stream/sequence (oneseq)
 */

template <typename itype>
class oneseq_stream : public default_increment<itype> {
protected:
    static constexpr bool is_mcg = false;

    // Is never called, but is provided for symmetry with specific_stream
    void set_stream(...)
    {
        abort();
    }

public:
    typedef itype state_type;

    static constexpr itype stream()
    {
         return default_increment<itype>::increment() >> 1;
    }

    static constexpr bool can_specify_stream = false;

    static constexpr size_t streams_pow2()
    {
        return 0u;
    }

protected:
    constexpr oneseq_stream() = default;
};


/*
 * specific stream
 */

template <typename itype>
class specific_stream {
protected:
    static constexpr bool is_mcg = false;

    itype inc_ = default_increment<itype>::increment();

public:
    typedef itype state_type;
    typedef itype stream_state;

    constexpr itype increment() const {
        return inc_;
    }

    itype stream()
    {
         return inc_ >> 1;
    }

    void set_stream(itype specific_seq)
    {
         inc_ = (specific_seq << 1) | 1;
    }

    static constexpr bool can_specify_stream = true;

    static constexpr size_t streams_pow2()
    {
        return (sizeof(itype)*8) - 1u;
    }

protected:
    specific_stream() = default;

    specific_stream(itype specific_seq)
        : inc_(itype(specific_seq << 1) | itype(1U))
    {
        // Nothing (else) to do.
    }
};


/*
 * This is where it all comes together.  This function joins together three
 * mixin classes which define
 *    - the LCG additive constant (the stream)
 *    - the LCG multiplier
 *    - the output function
 * in addition, we specify the type of the LCG state, and the result type,
 * and whether to use the pre-advance version of the state for the output
 * (increasing instruction-level parallelism) or the post-advance version
 * (reducing register pressure).
 *
 * Given the high level of parameterization, the code has to use some
 * template-metaprogramming tricks to handle some of the subtle variations
 * involved.
 */

template <typename xtype, typename itype,
          typename output_mixin,
          bool output_previous = true,
          typename stream_mixin = oneseq_stream<itype>,
          typename multiplier_mixin = default_multiplier<itype> >
class engine : protected output_mixin,
               public stream_mixin,
               protected multiplier_mixin {
protected:
    itype state_;

    struct can_specify_stream_tag {};
    struct no_specifiable_stream_tag {};

    using stream_mixin::increment;
    using multiplier_mixin::multiplier;

public:
    typedef xtype result_type;
    typedef itype state_type;

    static constexpr size_t period_pow2()
    {
        return sizeof(state_type)*8 - 2*stream_mixin::is_mcg;
    }

    // It would be nice to use std::numeric_limits for these, but
    // we can't be sure that it'd be defined for the 128-bit types.

    static constexpr result_type min()
    {
        return result_type(0UL);
    }

    static constexpr result_type max()
    {
        return result_type(~result_type(0UL));
    }

protected:
    itype bump(itype state)
    {
        return state * multiplier() + increment();
    }

    itype base_generate()
    {
        return state_ = bump(state_);
    }

    itype base_generate0()
    {
        itype old_state = state_;
        state_ = bump(state_);
        return old_state;
    }

public:
    result_type operator()()
    {
        if (output_previous)
            return this->output(base_generate0());
        else
            return this->output(base_generate());
    }

    result_type operator()(result_type upper_bound)
    {
        return bounded_rand(*this, upper_bound);
    }

protected:
    static itype advance(itype state, itype delta,
                         itype cur_mult, itype cur_plus);

    static itype distance(itype cur_state, itype newstate, itype cur_mult,
                          itype cur_plus, itype mask = ~itype(0U));

    itype distance(itype newstate, itype mask = itype(~itype(0U))) const
    {
        return distance(state_, newstate, multiplier(), increment(), mask);
    }

public:
    void advance(itype delta)
    {
        state_ = advance(state_, delta, this->multiplier(), this->increment());
    }

    void backstep(itype delta)
    {
        advance(-delta);
    }

    void discard(itype delta)
    {
        advance(delta);
    }

    bool wrapped()
    {
        if (stream_mixin::is_mcg) {
            // For MCGs, the low order two bits never change. In this
            // implementation, we keep them fixed at 3 to make this test
            // easier.
            return state_ == 3;
        } else {
            return state_ == 0;
        }
    }

    engine(itype state = itype(0xcafef00dd15ea5e5ULL))
        : state_(this->is_mcg ? state|state_type(3U)
                              : bump(state + this->increment()))
    {
        // Nothing else to do.
    }

    // This function may or may not exist.  It thus has to be a template
    // to use SFINAE; users don't have to worry about its template-ness.

    template <typename sm = stream_mixin>
    engine(itype state, typename sm::stream_state stream_seed)
        : stream_mixin(stream_seed),
          state_(this->is_mcg ? state|state_type(3U)
                              : bump(state + this->increment()))
    {
        // Nothing else to do.
    }

    template<typename SeedSeq>
    engine(SeedSeq&& seedSeq, typename std::enable_if<
                  !stream_mixin::can_specify_stream
               && !std::is_convertible<SeedSeq, itype>::value
               && !std::is_convertible<SeedSeq, engine>::value,
               no_specifiable_stream_tag>::type = {})
        : engine(generate_one<itype>(std::forward<SeedSeq>(seedSeq)))
    {
        // Nothing else to do.
    }

    template<typename SeedSeq>
    engine(SeedSeq&& seedSeq, typename std::enable_if<
                   stream_mixin::can_specify_stream
               && !std::is_convertible<SeedSeq, itype>::value
               && !std::is_convertible<SeedSeq, engine>::value,
        can_specify_stream_tag>::type = {})
    {
        itype seeddata[2];
        generate_to<2>(std::forward<SeedSeq>(seedSeq), seeddata);
        seed(seeddata[1], seeddata[0]);
    }


    template<typename... Args>
    void seed(Args&&... args)
    {
        new (this) engine(std::forward<Args>(args)...);
    }

    template <typename xtype1, typename itype1,
              typename output_mixin1, bool output_previous1,
              typename stream_mixin_lhs, typename multiplier_mixin_lhs,
              typename stream_mixin_rhs, typename multiplier_mixin_rhs>
    friend bool operator==(const engine<xtype1,itype1,
                                     output_mixin1,output_previous1,
                                     stream_mixin_lhs, multiplier_mixin_lhs>&,
                           const engine<xtype1,itype1,
                                     output_mixin1,output_previous1,
                                     stream_mixin_rhs, multiplier_mixin_rhs>&);

    template <typename xtype1, typename itype1,
              typename output_mixin1, bool output_previous1,
              typename stream_mixin_lhs, typename multiplier_mixin_lhs,
              typename stream_mixin_rhs, typename multiplier_mixin_rhs>
    friend itype1 operator-(const engine<xtype1,itype1,
                                     output_mixin1,output_previous1,
                                     stream_mixin_lhs, multiplier_mixin_lhs>&,
                            const engine<xtype1,itype1,
                                     output_mixin1,output_previous1,
                                     stream_mixin_rhs, multiplier_mixin_rhs>&);

    template <typename CharT, typename Traits,
              typename xtype1, typename itype1,
              typename output_mixin1, bool output_previous1,
              typename stream_mixin1, typename multiplier_mixin1>
    friend std::basic_ostream<CharT,Traits>&
    operator<<(std::basic_ostream<CharT,Traits>& out,
               const engine<xtype1,itype1,
                              output_mixin1,output_previous1,
                              stream_mixin1, multiplier_mixin1>&);

    template <typename CharT, typename Traits,
              typename xtype1, typename itype1,
              typename output_mixin1, bool output_previous1,
              typename stream_mixin1, typename multiplier_mixin1>
    friend std::basic_istream<CharT,Traits>&
    operator>>(std::basic_istream<CharT,Traits>& in,
               engine<xtype1, itype1,
                        output_mixin1, output_previous1,
                        stream_mixin1, multiplier_mixin1>& rng);
};

template <typename CharT, typename Traits,
          typename xtype, typename itype,
          typename output_mixin, bool output_previous,
          typename stream_mixin, typename multiplier_mixin>
std::basic_ostream<CharT,Traits>&
operator<<(std::basic_ostream<CharT,Traits>& out,
           const engine<xtype,itype,
                          output_mixin,output_previous,
                          stream_mixin, multiplier_mixin>& rng)
{
    using pcg_extras::operator<<;

    auto orig_flags = out.flags(std::ios_base::dec | std::ios_base::left);
    auto space = out.widen(' ');
    auto orig_fill = out.fill();

    out << rng.multiplier() << space
        << rng.increment() << space
        << rng.state_;

    out.flags(orig_flags);
    out.fill(orig_fill);
    return out;
}


template <typename CharT, typename Traits,
          typename xtype, typename itype,
          typename output_mixin, bool output_previous,
          typename stream_mixin, typename multiplier_mixin>
std::basic_istream<CharT,Traits>&
operator>>(std::basic_istream<CharT,Traits>& in,
           engine<xtype,itype,
                    output_mixin,output_previous,
                    stream_mixin, multiplier_mixin>& rng)
{
    using pcg_extras::operator>>;

    auto orig_flags = in.flags(std::ios_base::dec | std::ios_base::skipws);

    itype multiplier, increment, state;
    in >> multiplier >> increment >> state;

    if (!in.fail()) {
        bool good = true;
        if (multiplier != rng.multiplier()) {
           good = false;
        } else if (rng.can_specify_stream) {
           rng.set_stream(increment >> 1);
        } else if (increment != rng.increment()) {
           good = false;
        }
        if (good) {
            rng.state_ = state;
        } else {
            in.clear(std::ios::failbit);
        }
    }

    in.flags(orig_flags);
    return in;
}


template <typename xtype, typename itype,
          typename output_mixin, bool output_previous,
          typename stream_mixin, typename multiplier_mixin>
itype engine<xtype,itype,output_mixin,output_previous,stream_mixin,
             multiplier_mixin>::advance(
    itype state, itype delta, itype cur_mult, itype cur_plus)
{
    // The method used here is based on Brown, "Random Number Generation
    // with Arbitrary Stride,", Transactions of the American Nuclear
    // Society (Nov. 1994).  The algorithm is very similar to fast
    // exponentiation.
    //
    // Even though delta is an unsigned integer, we can pass a
    // signed integer to go backwards, it just goes "the long way round".

    constexpr itype ZERO = 0u;  // itype may be a non-trivial types, so
    constexpr itype ONE  = 1u;  // we define some ugly constants.
    itype acc_mult = 1;
    itype acc_plus = 0;
    while (delta > ZERO) {
       if (delta & ONE) {
          acc_mult *= cur_mult;
          acc_plus = acc_plus*cur_mult + cur_plus;
       }
       cur_plus = (cur_mult+ONE)*cur_plus;
       cur_mult *= cur_mult;
       delta >>= 1;
    }
    return acc_mult * state + acc_plus;
}

template <typename xtype, typename itype,
          typename output_mixin, bool output_previous,
          typename stream_mixin, typename multiplier_mixin>
itype engine<xtype,itype,output_mixin,output_previous,stream_mixin,
               multiplier_mixin>::distance(
    itype cur_state, itype newstate, itype cur_mult, itype cur_plus, itype mask)
{
    constexpr itype ONE  = 1u;  // itype could be weird, so use constant
    bool is_mcg = cur_plus == itype(0);
    itype the_bit = is_mcg ? itype(4u) : itype(1u);
    itype distance = 0u;
    while ((cur_state & mask) != (newstate & mask)) {
       if ((cur_state & the_bit) != (newstate & the_bit)) {
           cur_state = cur_state * cur_mult + cur_plus;
           distance |= the_bit;
       }
       assert((cur_state & the_bit) == (newstate & the_bit));
       the_bit <<= 1;
       cur_plus = (cur_mult+ONE)*cur_plus;
       cur_mult *= cur_mult;
    }
    return is_mcg ? distance >> 2 : distance;
}

template <typename xtype, typename itype,
          typename output_mixin, bool output_previous,
          typename stream_mixin_lhs, typename multiplier_mixin_lhs,
          typename stream_mixin_rhs, typename multiplier_mixin_rhs>
itype operator-(const engine<xtype,itype,
                               output_mixin,output_previous,
                               stream_mixin_lhs, multiplier_mixin_lhs>& lhs,
               const engine<xtype,itype,
                               output_mixin,output_previous,
                               stream_mixin_rhs, multiplier_mixin_rhs>& rhs)
{
    static_assert(
        std::is_same<stream_mixin_lhs, stream_mixin_rhs>::value &&
            std::is_same<multiplier_mixin_lhs, multiplier_mixin_rhs>::value,
        "Incomparable generators");
    if (lhs.increment() == rhs.increment()) {
       return rhs.distance(lhs.state_);
    } else  {
       constexpr itype ONE = 1u;
       itype lhs_diff = lhs.increment() + (lhs.multiplier()-ONE) * lhs.state_;
       itype rhs_diff = rhs.increment() + (rhs.multiplier()-ONE) * rhs.state_;
       if ((lhs_diff & itype(3u)) != (rhs_diff & itype(3u))) {
           rhs_diff = -rhs_diff;
       }
       return rhs.distance(rhs_diff, lhs_diff, rhs.multiplier(), itype(0u));
    }
}


template <typename xtype, typename itype,
          typename output_mixin, bool output_previous,
          typename stream_mixin_lhs, typename multiplier_mixin_lhs,
          typename stream_mixin_rhs, typename multiplier_mixin_rhs>
bool operator==(const engine<xtype,itype,
                               output_mixin,output_previous,
                               stream_mixin_lhs, multiplier_mixin_lhs>& lhs,
                const engine<xtype,itype,
                               output_mixin,output_previous,
                               stream_mixin_rhs, multiplier_mixin_rhs>& rhs)
{
    return    (lhs.multiplier() == rhs.multiplier())
           && (lhs.increment()  == rhs.increment())
           && (lhs.state_       == rhs.state_);
}

template <typename xtype, typename itype,
          typename output_mixin, bool output_previous,
          typename stream_mixin_lhs, typename multiplier_mixin_lhs,
          typename stream_mixin_rhs, typename multiplier_mixin_rhs>
inline bool operator!=(const engine<xtype,itype,
                               output_mixin,output_previous,
                               stream_mixin_lhs, multiplier_mixin_lhs>& lhs,
                       const engine<xtype,itype,
                               output_mixin,output_previous,
                               stream_mixin_rhs, multiplier_mixin_rhs>& rhs)
{
    return !operator==(lhs,rhs);
}


template <typename xtype, typename itype,
         template<typename XT,typename IT> class output_mixin,
         bool output_previous = (sizeof(itype) <= 8),
         template<typename IT> class multiplier_mixin = default_multiplier>
using oneseq_base  = engine<xtype, itype,
                        output_mixin<xtype, itype>, output_previous,
                        oneseq_stream<itype>,
                        multiplier_mixin<itype> >;

template <typename xtype, typename itype,
         template<typename XT,typename IT> class output_mixin,
         bool output_previous = (sizeof(itype) <= 8),
         template<typename IT> class multiplier_mixin = default_multiplier>
using unique_base = engine<xtype, itype,
                         output_mixin<xtype, itype>, output_previous,
                         unique_stream<itype>,
                         multiplier_mixin<itype> >;

template <typename xtype, typename itype,
         template<typename XT,typename IT> class output_mixin,
         bool output_previous = (sizeof(itype) <= 8),
         template<typename IT> class multiplier_mixin = default_multiplier>
using setseq_base = engine<xtype, itype,
                         output_mixin<xtype, itype>, output_previous,
                         specific_stream<itype>,
                         multiplier_mixin<itype> >;

template <typename xtype, typename itype,
         template<typename XT,typename IT> class output_mixin,
         bool output_previous = (sizeof(itype) <= 8),
         template<typename IT> class multiplier_mixin = default_multiplier>
using mcg_base = engine<xtype, itype,
                      output_mixin<xtype, itype>, output_previous,
                      no_stream<itype>,
                      multiplier_mixin<itype> >;

/*
 * OUTPUT FUNCTIONS.
 *
 * These are the core of the PCG generation scheme.  They specify how to
 * turn the base LCG's internal state into the output value of the final
 * generator.
 *
 * They're implemented as mixin classes.
 *
 * All of the classes have code that is written to allow it to be applied
 * at *arbitrary* bit sizes, although in practice they'll only be used at
 * standard sizes supported by C++.
 */

/*
 * XSH RS -- high xorshift, followed by a random shift
 *
 * Fast.  A good performer.
 */

template <typename xtype, typename itype>
struct xsh_rs_mixin {
    static xtype output(itype internal)
    {
        constexpr bitcount_t bits        = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t xtypebits   = bitcount_t(sizeof(xtype) * 8);
        constexpr bitcount_t sparebits   = bits - xtypebits;
        constexpr bitcount_t opbits =
                              sparebits-5 >= 64 ? 5
                            : sparebits-4 >= 32 ? 4
                            : sparebits-3 >= 16 ? 3
                            : sparebits-2 >= 4  ? 2
                            : sparebits-1 >= 1  ? 1
                            :                     0;
        constexpr bitcount_t mask = (1 << opbits) - 1;
        constexpr bitcount_t maxrandshift  = mask;
        constexpr bitcount_t topspare     = opbits;
        constexpr bitcount_t bottomspare = sparebits - topspare;
        constexpr bitcount_t xshift     = topspare + (xtypebits+maxrandshift)/2;
        bitcount_t rshift =
            opbits ? bitcount_t(internal >> (bits - opbits)) & mask : 0;
        internal ^= internal >> xshift;
        xtype result = xtype(internal >> (bottomspare - maxrandshift + rshift));
        return result;
    }
};

/*
 * XSH RR -- high xorshift, followed by a random rotate
 *
 * Fast.  A good performer.  Slightly better statistically than XSH RS.
 */

template <typename xtype, typename itype>
struct xsh_rr_mixin {
    static xtype output(itype internal)
    {
        constexpr bitcount_t bits        = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t xtypebits   = bitcount_t(sizeof(xtype)*8);
        constexpr bitcount_t sparebits   = bits - xtypebits;
        constexpr bitcount_t wantedopbits =
                              xtypebits >= 128 ? 7
                            : xtypebits >=  64 ? 6
                            : xtypebits >=  32 ? 5
                            : xtypebits >=  16 ? 4
                            :                    3;
        constexpr bitcount_t opbits =
                              sparebits >= wantedopbits ? wantedopbits
                                                        : sparebits;
        constexpr bitcount_t amplifier = wantedopbits - opbits;
        constexpr bitcount_t mask = (1 << opbits) - 1;
        constexpr bitcount_t topspare    = opbits;
        constexpr bitcount_t bottomspare = sparebits - topspare;
        constexpr bitcount_t xshift      = (topspare + xtypebits)/2;
        bitcount_t rot = opbits ? bitcount_t(internal >> (bits - opbits)) & mask
                                : 0;
        bitcount_t amprot = (rot << amplifier) & mask;
        internal ^= internal >> xshift;
        xtype result = xtype(internal >> bottomspare);
        result = rotr(result, amprot);
        return result;
    }
};

/*
 * RXS -- random xorshift
 */

template <typename xtype, typename itype>
struct rxs_mixin {
static xtype output_rxs(itype internal)
    {
        constexpr bitcount_t bits        = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t xtypebits   = bitcount_t(sizeof(xtype)*8);
        constexpr bitcount_t shift       = bits - xtypebits;
        constexpr bitcount_t extrashift  = (xtypebits - shift)/2;
        bitcount_t rshift = shift > 64+8 ? (internal >> (bits - 6)) & 63
                       : shift > 32+4 ? (internal >> (bits - 5)) & 31
                       : shift > 16+2 ? (internal >> (bits - 4)) & 15
                       : shift >  8+1 ? (internal >> (bits - 3)) & 7
                       : shift >  4+1 ? (internal >> (bits - 2)) & 3
                       : shift >  2+1 ? (internal >> (bits - 1)) & 1
                       :              0;
        internal ^= internal >> (shift + extrashift - rshift);
        xtype result = internal >> rshift;
        return result;
    }
};

/*
 * RXS M XS -- random xorshift, mcg multiply, fixed xorshift
 *
 * The most statistically powerful generator, but all those steps
 * make it slower than some of the others.  We give it the rottenest jobs.
 *
 * Because it's usually used in contexts where the state type and the
 * result type are the same, it is a permutation and is thus invertable.
 * We thus provide a function to invert it.  This function is used to
 * for the "inside out" generator used by the extended generator.
 */

/* Defined type-based concepts for the multiplication step.  They're actually
 * all derived by truncating the 128-bit, which was computed to be a good
 * "universal" constant.
 */

template <typename T>
struct mcg_multiplier {
    // Not defined for an arbitrary type
};

template <typename T>
struct mcg_unmultiplier {
    // Not defined for an arbitrary type
};

PCG_DEFINE_CONSTANT(uint8_t,  mcg, multiplier,   217U)
PCG_DEFINE_CONSTANT(uint8_t,  mcg, unmultiplier, 105U)

PCG_DEFINE_CONSTANT(uint16_t, mcg, multiplier,   62169U)
PCG_DEFINE_CONSTANT(uint16_t, mcg, unmultiplier, 28009U)

PCG_DEFINE_CONSTANT(uint32_t, mcg, multiplier,   277803737U)
PCG_DEFINE_CONSTANT(uint32_t, mcg, unmultiplier, 2897767785U)

PCG_DEFINE_CONSTANT(uint64_t, mcg, multiplier,   12605985483714917081ULL)
PCG_DEFINE_CONSTANT(uint64_t, mcg, unmultiplier, 15009553638781119849ULL)

PCG_DEFINE_CONSTANT(pcg128_t, mcg, multiplier,
        PCG_128BIT_CONSTANT(17766728186571221404ULL, 12605985483714917081ULL))
PCG_DEFINE_CONSTANT(pcg128_t, mcg, unmultiplier,
        PCG_128BIT_CONSTANT(14422606686972528997ULL, 15009553638781119849ULL))


template <typename xtype, typename itype>
struct rxs_m_xs_mixin {
    static xtype output(itype internal)
    {
        constexpr bitcount_t xtypebits = bitcount_t(sizeof(xtype) * 8);
        constexpr bitcount_t bits = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t opbits = xtypebits >= 128 ? 6
                                 : xtypebits >=  64 ? 5
                                 : xtypebits >=  32 ? 4
                                 : xtypebits >=  16 ? 3
                                 :                    2;
        constexpr bitcount_t shift = bits - xtypebits;
        constexpr bitcount_t mask = (1 << opbits) - 1;
        bitcount_t rshift =
            opbits ? bitcount_t(internal >> (bits - opbits)) & mask : 0;
        internal ^= internal >> (opbits + rshift);
        internal *= mcg_multiplier<itype>::multiplier();
        xtype result = internal >> shift;
        result ^= result >> ((2U*xtypebits+2U)/3U);
        return result;
    }

    static itype unoutput(itype internal)
    {
        constexpr bitcount_t bits = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t opbits = bits >= 128 ? 6
                                 : bits >=  64 ? 5
                                 : bits >=  32 ? 4
                                 : bits >=  16 ? 3
                                 :               2;
        constexpr bitcount_t mask = (1 << opbits) - 1;

        internal = unxorshift(internal, bits, (2U*bits+2U)/3U);

        internal *= mcg_unmultiplier<itype>::unmultiplier();

        bitcount_t rshift = opbits ? (internal >> (bits - opbits)) & mask : 0;
        internal = unxorshift(internal, bits, opbits + rshift);

        return internal;
    }
};


/*
 * RXS M -- random xorshift, mcg multiply
 */

template <typename xtype, typename itype>
struct rxs_m_mixin {
    static xtype output(itype internal)
    {
        constexpr bitcount_t xtypebits = bitcount_t(sizeof(xtype) * 8);
        constexpr bitcount_t bits = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t opbits = xtypebits >= 128 ? 6
                                 : xtypebits >=  64 ? 5
                                 : xtypebits >=  32 ? 4
                                 : xtypebits >=  16 ? 3
                                 :                    2;
        constexpr bitcount_t shift = bits - xtypebits;
        constexpr bitcount_t mask = (1 << opbits) - 1;
        bitcount_t rshift = opbits ? (internal >> (bits - opbits)) & mask : 0;
        internal ^= internal >> (opbits + rshift);
        internal *= mcg_multiplier<itype>::multiplier();
        xtype result = internal >> shift;
        return result;
    }
};


/*
 * DXSM -- double xorshift multiply
 *
 * This is a new, more powerful output permutation (added in 2019).  It's
 * a more comprehensive scrambling than RXS M, but runs faster on 128-bit
 * types.  Although primarily intended for use at large sizes, also works
 * at smaller sizes as well.
 *
 * This permutation is similar to xorshift multiply hash functions, except
 * that one of the multipliers is the LCG multiplier (to avoid needing to
 * have a second constant) and the other is based on the low-order bits.
 * This latter aspect means that the scrambling applied to the high bits
 * depends on the low bits, and makes it (to my eye) impractical to back
 * out the permutation without having the low-order bits.
 */

template <typename xtype, typename itype>
struct dxsm_mixin {
    inline xtype output(itype internal)
    {
        constexpr bitcount_t xtypebits = bitcount_t(sizeof(xtype) * 8);
        constexpr bitcount_t itypebits = bitcount_t(sizeof(itype) * 8);
        static_assert(xtypebits <= itypebits/2,
                      "Output type must be half the size of the state type.");
        
        xtype hi = xtype(internal >> (itypebits - xtypebits));
        xtype lo = xtype(internal);

        lo |= 1;
        hi ^= hi >> (xtypebits/2);
	hi *= xtype(cheap_multiplier<itype>::multiplier());
	hi ^= hi >> (3*(xtypebits/4));
	hi *= lo;
	return hi;
    }
};


/*
 * XSL RR -- fixed xorshift (to low bits), random rotate
 *
 * Useful for 128-bit types that are split across two CPU registers.
 */

template <typename xtype, typename itype>
struct xsl_rr_mixin {
    static xtype output(itype internal)
    {
        constexpr bitcount_t xtypebits = bitcount_t(sizeof(xtype) * 8);
        constexpr bitcount_t bits = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t sparebits = bits - xtypebits;
        constexpr bitcount_t wantedopbits = xtypebits >= 128 ? 7
                                       : xtypebits >=  64 ? 6
                                       : xtypebits >=  32 ? 5
                                       : xtypebits >=  16 ? 4
                                       :                    3;
        constexpr bitcount_t opbits = sparebits >= wantedopbits ? wantedopbits
                                                             : sparebits;
        constexpr bitcount_t amplifier = wantedopbits - opbits;
        constexpr bitcount_t mask = (1 << opbits) - 1;
        constexpr bitcount_t topspare = sparebits;
        constexpr bitcount_t bottomspare = sparebits - topspare;
        constexpr bitcount_t xshift = (topspare + xtypebits) / 2;

        bitcount_t rot =
            opbits ? bitcount_t(internal >> (bits - opbits)) & mask : 0;
        bitcount_t amprot = (rot << amplifier) & mask;
        internal ^= internal >> xshift;
        xtype result = xtype(internal >> bottomspare);
        result = rotr(result, amprot);
        return result;
    }
};


/*
 * XSL RR RR -- fixed xorshift (to low bits), random rotate (both parts)
 *
 * Useful for 128-bit types that are split across two CPU registers.
 * If you really want an invertable 128-bit RNG, I guess this is the one.
 */

template <typename T> struct halfsize_trait {};
template <> struct halfsize_trait<pcg128_t>  { typedef uint64_t type; };
template <> struct halfsize_trait<uint64_t>  { typedef uint32_t type; };
template <> struct halfsize_trait<uint32_t>  { typedef uint16_t type; };
template <> struct halfsize_trait<uint16_t>  { typedef uint8_t type;  };

template <typename xtype, typename itype>
struct xsl_rr_rr_mixin {
    typedef typename halfsize_trait<itype>::type htype;

    static itype output(itype internal)
    {
        constexpr bitcount_t htypebits = bitcount_t(sizeof(htype) * 8);
        constexpr bitcount_t bits      = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t sparebits = bits - htypebits;
        constexpr bitcount_t wantedopbits = htypebits >= 128 ? 7
                                       : htypebits >=  64 ? 6
                                       : htypebits >=  32 ? 5
                                       : htypebits >=  16 ? 4
                                       :                    3;
        constexpr bitcount_t opbits = sparebits >= wantedopbits ? wantedopbits
                                                                : sparebits;
        constexpr bitcount_t amplifier = wantedopbits - opbits;
        constexpr bitcount_t mask = (1 << opbits) - 1;
        constexpr bitcount_t topspare = sparebits;
        constexpr bitcount_t xshift = (topspare + htypebits) / 2;

        bitcount_t rot =
            opbits ? bitcount_t(internal >> (bits - opbits)) & mask : 0;
        bitcount_t amprot = (rot << amplifier) & mask;
        internal ^= internal >> xshift;
        htype lowbits = htype(internal);
        lowbits = rotr(lowbits, amprot);
        htype highbits = htype(internal >> topspare);
        bitcount_t rot2 = lowbits & mask;
        bitcount_t amprot2 = (rot2 << amplifier) & mask;
        highbits = rotr(highbits, amprot2);
        return (itype(highbits) << topspare) ^ itype(lowbits);
    }
};


/*
 * XSH -- fixed xorshift (to high bits)
 *
 * You shouldn't use this at 64-bits or less.
 */

template <typename xtype, typename itype>
struct xsh_mixin {
    static xtype output(itype internal)
    {
        constexpr bitcount_t xtypebits = bitcount_t(sizeof(xtype) * 8);
        constexpr bitcount_t bits = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t sparebits = bits - xtypebits;
        constexpr bitcount_t topspare = 0;
        constexpr bitcount_t bottomspare = sparebits - topspare;
        constexpr bitcount_t xshift = (topspare + xtypebits) / 2;

        internal ^= internal >> xshift;
        xtype result = internal >> bottomspare;
        return result;
    }
};

/*
 * XSL -- fixed xorshift (to low bits)
 *
 * You shouldn't use this at 64-bits or less.
 */

template <typename xtype, typename itype>
struct xsl_mixin {
    inline xtype output(itype internal)
    {
        constexpr bitcount_t xtypebits = bitcount_t(sizeof(xtype) * 8);
        constexpr bitcount_t bits = bitcount_t(sizeof(itype) * 8);
        constexpr bitcount_t sparebits = bits - xtypebits;
        constexpr bitcount_t topspare = sparebits;
        constexpr bitcount_t bottomspare = sparebits - topspare;
        constexpr bitcount_t xshift = (topspare + xtypebits) / 2;

        internal ^= internal >> xshift;
        xtype result = internal >> bottomspare;
        return result;
    }
};


/* ---- End of Output Functions ---- */


template <typename baseclass>
struct inside_out : private baseclass {
    inside_out() = delete;

    typedef typename baseclass::result_type result_type;
    typedef typename baseclass::state_type  state_type;
    static_assert(sizeof(result_type) == sizeof(state_type),
                  "Require a RNG whose output function is a permutation");

    static bool external_step(result_type& randval, size_t i)
    {
        state_type state = baseclass::unoutput(randval);
        state = state * baseclass::multiplier() + baseclass::increment()
                + state_type(i*2);
        result_type result = baseclass::output(state);
        randval = result;
        state_type zero =
            baseclass::is_mcg ? state & state_type(3U) : state_type(0U);
        return result == zero;
    }

    static bool external_advance(result_type& randval, size_t i,
                                 result_type delta, bool forwards = true)
    {
        state_type state = baseclass::unoutput(randval);
        state_type mult  = baseclass::multiplier();
        state_type inc   = baseclass::increment() + state_type(i*2);
        state_type zero =
            baseclass::is_mcg ? state & state_type(3U) : state_type(0U);
        state_type dist_to_zero = baseclass::distance(state, zero, mult, inc);
        bool crosses_zero =
            forwards ? dist_to_zero <= delta
                     : (-dist_to_zero) <= delta;
        if (!forwards)
            delta = -delta;
        state = baseclass::advance(state, delta, mult, inc);
        randval = baseclass::output(state);
        return crosses_zero;
    }
};


template <bitcount_t table_pow2, bitcount_t advance_pow2, typename baseclass, typename extvalclass, bool kdd = true>
class extended : public baseclass {
public:
    typedef typename baseclass::state_type  state_type;
    typedef typename baseclass::result_type result_type;
    typedef inside_out<extvalclass> insideout;

private:
    static constexpr bitcount_t rtypebits = sizeof(result_type)*8;
    static constexpr bitcount_t stypebits = sizeof(state_type)*8;

    static constexpr bitcount_t tick_limit_pow2 = 64U;

    static constexpr size_t table_size  = 1UL << table_pow2;
    static constexpr size_t table_shift = stypebits - table_pow2;
    static constexpr state_type table_mask =
        (state_type(1U) << table_pow2) - state_type(1U);

    static constexpr bool   may_tick  =
        (advance_pow2 < stypebits) && (advance_pow2 < tick_limit_pow2);
    static constexpr size_t tick_shift = stypebits - advance_pow2;
    static constexpr state_type tick_mask  =
        may_tick ? state_type(
                       (uint64_t(1) << (advance_pow2*may_tick)) - 1)
                                        // ^-- stupidity to appease GCC warnings
                 : ~state_type(0U);

    static constexpr bool may_tock = stypebits < tick_limit_pow2;

    result_type data_[table_size];

    PCG_NOINLINE void advance_table();

    PCG_NOINLINE void advance_table(state_type delta, bool isForwards = true);

    result_type& get_extended_value()
    {
        state_type state = this->state_;
        if (kdd && baseclass::is_mcg) {
            // The low order bits of an MCG are constant, so drop them.
            state >>= 2;
        }
        size_t index       = kdd ? state &  table_mask
                                 : state >> table_shift;

        if (may_tick) {
            bool tick = kdd ? (state & tick_mask) == state_type(0u)
                            : (state >> tick_shift) == state_type(0u);
            if (tick)
                    advance_table();
        }
        if (may_tock) {
            bool tock = state == state_type(0u);
            if (tock)
                advance_table();
        }
        return data_[index];
    }

public:
    static constexpr size_t period_pow2()
    {
        return baseclass::period_pow2() + table_size*extvalclass::period_pow2();
    }

    PCG_ALWAYS_INLINE result_type operator()()
    {
        result_type rhs = get_extended_value();
        result_type lhs = this->baseclass::operator()();
        return lhs ^ rhs;
    }

    result_type operator()(result_type upper_bound)
    {
        return bounded_rand(*this, upper_bound);
    }

    void set(result_type wanted)
    {
        result_type& rhs = get_extended_value();
        result_type lhs = this->baseclass::operator()();
        rhs = lhs ^ wanted;
    }

    void advance(state_type distance, bool forwards = true);

    void backstep(state_type distance)
    {
        advance(distance, false);
    }

    extended(const result_type* data)
        : baseclass()
    {
        datainit(data);
    }

    extended(const result_type* data, state_type seed)
        : baseclass(seed)
    {
        datainit(data);
    }

    // This function may or may not exist.  It thus has to be a template
    // to use SFINAE; users don't have to worry about its template-ness.

    template <typename bc = baseclass>
    extended(const result_type* data, state_type seed,
            typename bc::stream_state stream_seed)
        : baseclass(seed, stream_seed)
    {
        datainit(data);
    }

    extended()
        : baseclass()
    {
        selfinit();
    }

    extended(state_type seed)
        : baseclass(seed)
    {
        selfinit();
    }

    // This function may or may not exist.  It thus has to be a template
    // to use SFINAE; users don't have to worry about its template-ness.

    template <typename bc = baseclass>
    extended(state_type seed, typename bc::stream_state stream_seed)
        : baseclass(seed, stream_seed)
    {
        selfinit();
    }

private:
    void selfinit();
    void datainit(const result_type* data);

public:

    template<typename SeedSeq, typename = typename std::enable_if<
           !std::is_convertible<SeedSeq, result_type>::value
        && !std::is_convertible<SeedSeq, extended>::value>::type>
    extended(SeedSeq&& seedSeq)
        : baseclass(seedSeq)
    {
        generate_to<table_size>(seedSeq, data_);
    }

    template<typename... Args>
    void seed(Args&&... args)
    {
        new (this) extended(std::forward<Args>(args)...);
    }

    template <bitcount_t table_pow2_, bitcount_t advance_pow2_,
              typename baseclass_, typename extvalclass_, bool kdd_>
    friend bool operator==(const extended<table_pow2_, advance_pow2_,
                                              baseclass_, extvalclass_, kdd_>&,
                           const extended<table_pow2_, advance_pow2_,
                                              baseclass_, extvalclass_, kdd_>&);

    template <typename CharT, typename Traits,
              bitcount_t table_pow2_, bitcount_t advance_pow2_,
              typename baseclass_, typename extvalclass_, bool kdd_>
    friend std::basic_ostream<CharT,Traits>&
    operator<<(std::basic_ostream<CharT,Traits>& out,
               const extended<table_pow2_, advance_pow2_,
                              baseclass_, extvalclass_, kdd_>&);

    template <typename CharT, typename Traits,
              bitcount_t table_pow2_, bitcount_t advance_pow2_,
              typename baseclass_, typename extvalclass_, bool kdd_>
    friend std::basic_istream<CharT,Traits>&
    operator>>(std::basic_istream<CharT,Traits>& in,
               extended<table_pow2_, advance_pow2_,
                        baseclass_, extvalclass_, kdd_>&);

};


template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
void extended<table_pow2,advance_pow2,baseclass,extvalclass,kdd>::datainit(
         const result_type* data)
{
    for (size_t i = 0; i < table_size; ++i)
        data_[i] = data[i];
}

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
void extended<table_pow2,advance_pow2,baseclass,extvalclass,kdd>::selfinit()
{
    // We need to fill the extended table with something, and we have
    // very little provided data, so we use the base generator to
    // produce values.  Although not ideal (use a seed sequence, folks!),
    // unexpected correlations are mitigated by
    //      - using XOR differences rather than the number directly
    //      - the way the table is accessed, its values *won't* be accessed
    //        in the same order the were written.
    //      - any strange correlations would only be apparent if we
    //        were to backstep the generator so that the base generator
    //        was generating the same values again
    result_type lhs = baseclass::operator()();
    result_type rhs = baseclass::operator()();
    result_type xdiff = lhs - rhs;
    for (size_t i = 0; i < table_size; ++i) {
        data_[i] = baseclass::operator()() ^ xdiff;
    }
}

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
bool operator==(const extended<table_pow2, advance_pow2,
                               baseclass, extvalclass, kdd>& lhs,
                const extended<table_pow2, advance_pow2,
                               baseclass, extvalclass, kdd>& rhs)
{
    auto& base_lhs = static_cast<const baseclass&>(lhs);
    auto& base_rhs = static_cast<const baseclass&>(rhs);
    return base_lhs == base_rhs
        && std::equal(
               std::begin(lhs.data_), std::end(lhs.data_),
               std::begin(rhs.data_)
           );
}

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
inline bool operator!=(const extended<table_pow2, advance_pow2,
                                      baseclass, extvalclass, kdd>& lhs,
                       const extended<table_pow2, advance_pow2,
                                      baseclass, extvalclass, kdd>& rhs)
{
    return !operator==(lhs, rhs);
}

template <typename CharT, typename Traits,
          bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
std::basic_ostream<CharT,Traits>&
operator<<(std::basic_ostream<CharT,Traits>& out,
           const extended<table_pow2, advance_pow2,
                          baseclass, extvalclass, kdd>& rng)
{
    using pcg_extras::operator<<;

    auto orig_flags = out.flags(std::ios_base::dec | std::ios_base::left);
    auto space = out.widen(' ');
    auto orig_fill = out.fill();

    out << rng.multiplier() << space
        << rng.increment() << space
        << rng.state_;

    for (const auto& datum : rng.data_)
        out << space << datum;

    out.flags(orig_flags);
    out.fill(orig_fill);
    return out;
}

template <typename CharT, typename Traits,
          bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
std::basic_istream<CharT,Traits>&
operator>>(std::basic_istream<CharT,Traits>& in,
           extended<table_pow2, advance_pow2,
                    baseclass, extvalclass, kdd>& rng)
{
    extended<table_pow2, advance_pow2, baseclass, extvalclass> new_rng;
    auto& base_rng = static_cast<baseclass&>(new_rng);
    in >> base_rng;

    if (in.fail())
        return in;

    using pcg_extras::operator>>;

    auto orig_flags = in.flags(std::ios_base::dec | std::ios_base::skipws);

    for (auto& datum : new_rng.data_) {
        in >> datum;
        if (in.fail())
            goto bail;
    }

    rng = new_rng;

bail:
    in.flags(orig_flags);
    return in;
}



template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
void
extended<table_pow2,advance_pow2,baseclass,extvalclass,kdd>::advance_table()
{
    bool carry = false;
    for (size_t i = 0; i < table_size; ++i) {
        if (carry) {
            carry = insideout::external_step(data_[i],i+1);
        }
        bool carry2 = insideout::external_step(data_[i],i+1);
        carry = carry || carry2;
    }
}

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
void
extended<table_pow2,advance_pow2,baseclass,extvalclass,kdd>::advance_table(
        state_type delta, bool isForwards)
{
    typedef typename baseclass::state_type   base_state_t;
    typedef typename extvalclass::state_type ext_state_t;
    constexpr bitcount_t basebits = sizeof(base_state_t)*8;
    constexpr bitcount_t extbits  = sizeof(ext_state_t)*8;
    static_assert(basebits <= extbits || advance_pow2 > 0,
                  "Current implementation might overflow its carry");

    base_state_t carry = 0;
    for (size_t i = 0; i < table_size; ++i) {
        base_state_t total_delta = carry + delta;
        ext_state_t  trunc_delta = ext_state_t(total_delta);
        if (basebits > extbits) {
            carry = total_delta >> extbits;
        } else {
            carry = 0;
        }
        carry +=
            insideout::external_advance(data_[i],i+1, trunc_delta, isForwards);
    }
}

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename baseclass, typename extvalclass, bool kdd>
void extended<table_pow2,advance_pow2,baseclass,extvalclass,kdd>::advance(
    state_type distance, bool forwards)
{
    static_assert(kdd,
        "Efficient advance is too hard for non-kdd extension. "
        "For a weak advance, cast to base class");
    state_type zero =
        baseclass::is_mcg ? this->state_ & state_type(3U) : state_type(0U);
    if (may_tick) {
        state_type ticks = distance >> (advance_pow2*may_tick);
                                        // ^-- stupidity to appease GCC
                                        // warnings
        state_type adv_mask =
            baseclass::is_mcg ? tick_mask << 2 : tick_mask;
        state_type next_advance_distance = this->distance(zero, adv_mask);
        if (!forwards)
            next_advance_distance = (-next_advance_distance) & tick_mask;
        if (next_advance_distance < (distance & tick_mask)) {
            ++ticks;
        }
        if (ticks)
            advance_table(ticks, forwards);
    }
    if (forwards) {
        if (may_tock && this->distance(zero) <= distance)
            advance_table();
        baseclass::advance(distance);
    } else {
        if (may_tock && -(this->distance(zero)) <= distance)
            advance_table(state_type(1U), false);
        baseclass::advance(-distance);
    }
}

} // namespace pcg_detail

namespace pcg_engines {

using namespace pcg_detail;

/* Predefined types for XSH RS */

typedef oneseq_base<uint8_t,  uint16_t, xsh_rs_mixin>  oneseq_xsh_rs_16_8;
typedef oneseq_base<uint16_t, uint32_t, xsh_rs_mixin>  oneseq_xsh_rs_32_16;
typedef oneseq_base<uint32_t, uint64_t, xsh_rs_mixin>  oneseq_xsh_rs_64_32;
typedef oneseq_base<uint64_t, pcg128_t, xsh_rs_mixin>  oneseq_xsh_rs_128_64;
typedef oneseq_base<uint64_t, pcg128_t, xsh_rs_mixin, true, cheap_multiplier>
                                                       cm_oneseq_xsh_rs_128_64;

typedef unique_base<uint8_t,  uint16_t, xsh_rs_mixin>  unique_xsh_rs_16_8;
typedef unique_base<uint16_t, uint32_t, xsh_rs_mixin>  unique_xsh_rs_32_16;
typedef unique_base<uint32_t, uint64_t, xsh_rs_mixin>  unique_xsh_rs_64_32;
typedef unique_base<uint64_t, pcg128_t, xsh_rs_mixin>  unique_xsh_rs_128_64;
typedef unique_base<uint64_t, pcg128_t, xsh_rs_mixin, true, cheap_multiplier>
                                                       cm_unique_xsh_rs_128_64;

typedef setseq_base<uint8_t,  uint16_t, xsh_rs_mixin>  setseq_xsh_rs_16_8;
typedef setseq_base<uint16_t, uint32_t, xsh_rs_mixin>  setseq_xsh_rs_32_16;
typedef setseq_base<uint32_t, uint64_t, xsh_rs_mixin>  setseq_xsh_rs_64_32;
typedef setseq_base<uint64_t, pcg128_t, xsh_rs_mixin>  setseq_xsh_rs_128_64;
typedef setseq_base<uint64_t, pcg128_t, xsh_rs_mixin, true, cheap_multiplier>
                                                       cm_setseq_xsh_rs_128_64;

typedef mcg_base<uint8_t,  uint16_t, xsh_rs_mixin>  mcg_xsh_rs_16_8;
typedef mcg_base<uint16_t, uint32_t, xsh_rs_mixin>  mcg_xsh_rs_32_16;
typedef mcg_base<uint32_t, uint64_t, xsh_rs_mixin>  mcg_xsh_rs_64_32;
typedef mcg_base<uint64_t, pcg128_t, xsh_rs_mixin>  mcg_xsh_rs_128_64;
typedef mcg_base<uint64_t, pcg128_t, xsh_rs_mixin, true, cheap_multiplier>
                                                    cm_mcg_xsh_rs_128_64;

/* Predefined types for XSH RR */

typedef oneseq_base<uint8_t,  uint16_t, xsh_rr_mixin>  oneseq_xsh_rr_16_8;
typedef oneseq_base<uint16_t, uint32_t, xsh_rr_mixin>  oneseq_xsh_rr_32_16;
typedef oneseq_base<uint32_t, uint64_t, xsh_rr_mixin>  oneseq_xsh_rr_64_32;
typedef oneseq_base<uint64_t, pcg128_t, xsh_rr_mixin>  oneseq_xsh_rr_128_64;
typedef oneseq_base<uint64_t, pcg128_t, xsh_rr_mixin, true, cheap_multiplier>
                                                       cm_oneseq_xsh_rr_128_64;

typedef unique_base<uint8_t,  uint16_t, xsh_rr_mixin>  unique_xsh_rr_16_8;
typedef unique_base<uint16_t, uint32_t, xsh_rr_mixin>  unique_xsh_rr_32_16;
typedef unique_base<uint32_t, uint64_t, xsh_rr_mixin>  unique_xsh_rr_64_32;
typedef unique_base<uint64_t, pcg128_t, xsh_rr_mixin>  unique_xsh_rr_128_64;
typedef unique_base<uint64_t, pcg128_t, xsh_rr_mixin, true, cheap_multiplier>
                                                       cm_unique_xsh_rr_128_64;

typedef setseq_base<uint8_t,  uint16_t, xsh_rr_mixin>  setseq_xsh_rr_16_8;
typedef setseq_base<uint16_t, uint32_t, xsh_rr_mixin>  setseq_xsh_rr_32_16;
typedef setseq_base<uint32_t, uint64_t, xsh_rr_mixin>  setseq_xsh_rr_64_32;
typedef setseq_base<uint64_t, pcg128_t, xsh_rr_mixin>  setseq_xsh_rr_128_64;
typedef setseq_base<uint64_t, pcg128_t, xsh_rr_mixin, true, cheap_multiplier>
                                                       cm_setseq_xsh_rr_128_64;

typedef mcg_base<uint8_t,  uint16_t, xsh_rr_mixin>  mcg_xsh_rr_16_8;
typedef mcg_base<uint16_t, uint32_t, xsh_rr_mixin>  mcg_xsh_rr_32_16;
typedef mcg_base<uint32_t, uint64_t, xsh_rr_mixin>  mcg_xsh_rr_64_32;
typedef mcg_base<uint64_t, pcg128_t, xsh_rr_mixin>  mcg_xsh_rr_128_64;
typedef mcg_base<uint64_t, pcg128_t, xsh_rr_mixin, true, cheap_multiplier>
                                                    cm_mcg_xsh_rr_128_64;


/* Predefined types for RXS M XS */

typedef oneseq_base<uint8_t,  uint8_t, rxs_m_xs_mixin>   oneseq_rxs_m_xs_8_8;
typedef oneseq_base<uint16_t, uint16_t, rxs_m_xs_mixin>  oneseq_rxs_m_xs_16_16;
typedef oneseq_base<uint32_t, uint32_t, rxs_m_xs_mixin>  oneseq_rxs_m_xs_32_32;
typedef oneseq_base<uint64_t, uint64_t, rxs_m_xs_mixin>  oneseq_rxs_m_xs_64_64;
typedef oneseq_base<pcg128_t, pcg128_t, rxs_m_xs_mixin>
                                                        oneseq_rxs_m_xs_128_128;
typedef oneseq_base<pcg128_t, pcg128_t, rxs_m_xs_mixin, true, cheap_multiplier>
                                                     cm_oneseq_rxs_m_xs_128_128;

typedef unique_base<uint8_t,  uint8_t, rxs_m_xs_mixin>  unique_rxs_m_xs_8_8;
typedef unique_base<uint16_t, uint16_t, rxs_m_xs_mixin> unique_rxs_m_xs_16_16;
typedef unique_base<uint32_t, uint32_t, rxs_m_xs_mixin> unique_rxs_m_xs_32_32;
typedef unique_base<uint64_t, uint64_t, rxs_m_xs_mixin> unique_rxs_m_xs_64_64;
typedef unique_base<pcg128_t, pcg128_t, rxs_m_xs_mixin> unique_rxs_m_xs_128_128;
typedef unique_base<pcg128_t, pcg128_t, rxs_m_xs_mixin, true, cheap_multiplier>
                                                     cm_unique_rxs_m_xs_128_128;

typedef setseq_base<uint8_t,  uint8_t, rxs_m_xs_mixin>  setseq_rxs_m_xs_8_8;
typedef setseq_base<uint16_t, uint16_t, rxs_m_xs_mixin> setseq_rxs_m_xs_16_16;
typedef setseq_base<uint32_t, uint32_t, rxs_m_xs_mixin> setseq_rxs_m_xs_32_32;
typedef setseq_base<uint64_t, uint64_t, rxs_m_xs_mixin> setseq_rxs_m_xs_64_64;
typedef setseq_base<pcg128_t, pcg128_t, rxs_m_xs_mixin> setseq_rxs_m_xs_128_128;
typedef setseq_base<pcg128_t, pcg128_t, rxs_m_xs_mixin, true, cheap_multiplier>
                                                     cm_setseq_rxs_m_xs_128_128;

                // MCG versions don't make sense here, so aren't defined.

/* Predefined types for RXS M */

typedef oneseq_base<uint8_t,  uint16_t, rxs_m_mixin>  oneseq_rxs_m_16_8;
typedef oneseq_base<uint16_t, uint32_t, rxs_m_mixin>  oneseq_rxs_m_32_16;
typedef oneseq_base<uint32_t, uint64_t, rxs_m_mixin>  oneseq_rxs_m_64_32;
typedef oneseq_base<uint64_t, pcg128_t, rxs_m_mixin>  oneseq_rxs_m_128_64;
typedef oneseq_base<uint64_t, pcg128_t, rxs_m_mixin, true, cheap_multiplier>
                                                      cm_oneseq_rxs_m_128_64;

typedef unique_base<uint8_t,  uint16_t, rxs_m_mixin>  unique_rxs_m_16_8;
typedef unique_base<uint16_t, uint32_t, rxs_m_mixin>  unique_rxs_m_32_16;
typedef unique_base<uint32_t, uint64_t, rxs_m_mixin>  unique_rxs_m_64_32;
typedef unique_base<uint64_t, pcg128_t, rxs_m_mixin>  unique_rxs_m_128_64;
typedef unique_base<uint64_t, pcg128_t, rxs_m_mixin, true, cheap_multiplier>
                                                      cm_unique_rxs_m_128_64;

typedef setseq_base<uint8_t,  uint16_t, rxs_m_mixin>  setseq_rxs_m_16_8;
typedef setseq_base<uint16_t, uint32_t, rxs_m_mixin>  setseq_rxs_m_32_16;
typedef setseq_base<uint32_t, uint64_t, rxs_m_mixin>  setseq_rxs_m_64_32;
typedef setseq_base<uint64_t, pcg128_t, rxs_m_mixin>  setseq_rxs_m_128_64;
typedef setseq_base<uint64_t, pcg128_t, rxs_m_mixin, true, cheap_multiplier>
                                                      cm_setseq_rxs_m_128_64;

typedef mcg_base<uint8_t,  uint16_t, rxs_m_mixin>  mcg_rxs_m_16_8;
typedef mcg_base<uint16_t, uint32_t, rxs_m_mixin>  mcg_rxs_m_32_16;
typedef mcg_base<uint32_t, uint64_t, rxs_m_mixin>  mcg_rxs_m_64_32;
typedef mcg_base<uint64_t, pcg128_t, rxs_m_mixin>  mcg_rxs_m_128_64;
typedef mcg_base<uint64_t, pcg128_t, rxs_m_mixin, true, cheap_multiplier>
                                                   cm_mcg_rxs_m_128_64;

/* Predefined types for DXSM */

typedef oneseq_base<uint8_t,  uint16_t, dxsm_mixin>  oneseq_dxsm_16_8;
typedef oneseq_base<uint16_t, uint32_t, dxsm_mixin>  oneseq_dxsm_32_16;
typedef oneseq_base<uint32_t, uint64_t, dxsm_mixin>  oneseq_dxsm_64_32;
typedef oneseq_base<uint64_t, pcg128_t, dxsm_mixin>  oneseq_dxsm_128_64;
typedef oneseq_base<uint64_t, pcg128_t, dxsm_mixin, true, cheap_multiplier>
                                                     cm_oneseq_dxsm_128_64;

typedef unique_base<uint8_t,  uint16_t, dxsm_mixin>  unique_dxsm_16_8;
typedef unique_base<uint16_t, uint32_t, dxsm_mixin>  unique_dxsm_32_16;
typedef unique_base<uint32_t, uint64_t, dxsm_mixin>  unique_dxsm_64_32;
typedef unique_base<uint64_t, pcg128_t, dxsm_mixin>  unique_dxsm_128_64;
typedef unique_base<uint64_t, pcg128_t, dxsm_mixin, true, cheap_multiplier>
                                                     cm_unique_dxsm_128_64;

typedef setseq_base<uint8_t,  uint16_t, dxsm_mixin>  setseq_dxsm_16_8;
typedef setseq_base<uint16_t, uint32_t, dxsm_mixin>  setseq_dxsm_32_16;
typedef setseq_base<uint32_t, uint64_t, dxsm_mixin>  setseq_dxsm_64_32;
typedef setseq_base<uint64_t, pcg128_t, dxsm_mixin>  setseq_dxsm_128_64;
typedef setseq_base<uint64_t, pcg128_t, dxsm_mixin, true, cheap_multiplier>
                                                     cm_setseq_dxsm_128_64;

typedef mcg_base<uint8_t,  uint16_t, dxsm_mixin>  mcg_dxsm_16_8;
typedef mcg_base<uint16_t, uint32_t, dxsm_mixin>  mcg_dxsm_32_16;
typedef mcg_base<uint32_t, uint64_t, dxsm_mixin>  mcg_dxsm_64_32;
typedef mcg_base<uint64_t, pcg128_t, dxsm_mixin>  mcg_dxsm_128_64;
typedef mcg_base<uint64_t, pcg128_t, dxsm_mixin, true, cheap_multiplier>
                                                  cm_mcg_dxsm_128_64;

/* Predefined types for XSL RR (only defined for "large" types) */

typedef oneseq_base<uint32_t, uint64_t, xsl_rr_mixin>  oneseq_xsl_rr_64_32;
typedef oneseq_base<uint64_t, pcg128_t, xsl_rr_mixin>  oneseq_xsl_rr_128_64;
typedef oneseq_base<uint64_t, pcg128_t, xsl_rr_mixin, true, cheap_multiplier>
                                                       cm_oneseq_xsl_rr_128_64;

typedef unique_base<uint32_t, uint64_t, xsl_rr_mixin>  unique_xsl_rr_64_32;
typedef unique_base<uint64_t, pcg128_t, xsl_rr_mixin>  unique_xsl_rr_128_64;
typedef unique_base<uint64_t, pcg128_t, xsl_rr_mixin, true, cheap_multiplier>
                                                       cm_unique_xsl_rr_128_64;

typedef setseq_base<uint32_t, uint64_t, xsl_rr_mixin>  setseq_xsl_rr_64_32;
typedef setseq_base<uint64_t, pcg128_t, xsl_rr_mixin>  setseq_xsl_rr_128_64;
typedef setseq_base<uint64_t, pcg128_t, xsl_rr_mixin, true, cheap_multiplier>
                                                       cm_setseq_xsl_rr_128_64;

typedef mcg_base<uint32_t, uint64_t, xsl_rr_mixin>  mcg_xsl_rr_64_32;
typedef mcg_base<uint64_t, pcg128_t, xsl_rr_mixin>  mcg_xsl_rr_128_64;
typedef mcg_base<uint64_t, pcg128_t, xsl_rr_mixin, true, cheap_multiplier>
                                                    cm_mcg_xsl_rr_128_64;


/* Predefined types for XSL RR RR (only defined for "large" types) */

typedef oneseq_base<uint64_t, uint64_t, xsl_rr_rr_mixin>
    oneseq_xsl_rr_rr_64_64;
typedef oneseq_base<pcg128_t, pcg128_t, xsl_rr_rr_mixin>
    oneseq_xsl_rr_rr_128_128;
typedef oneseq_base<pcg128_t, pcg128_t, xsl_rr_rr_mixin, true, cheap_multiplier>
    cm_oneseq_xsl_rr_rr_128_128;

typedef unique_base<uint64_t, uint64_t, xsl_rr_rr_mixin>
    unique_xsl_rr_rr_64_64;
typedef unique_base<pcg128_t, pcg128_t, xsl_rr_rr_mixin>
    unique_xsl_rr_rr_128_128;
typedef unique_base<pcg128_t, pcg128_t, xsl_rr_rr_mixin, true, cheap_multiplier>
    cm_unique_xsl_rr_rr_128_128;

typedef setseq_base<uint64_t, uint64_t, xsl_rr_rr_mixin>
    setseq_xsl_rr_rr_64_64;
typedef setseq_base<pcg128_t, pcg128_t, xsl_rr_rr_mixin>
    setseq_xsl_rr_rr_128_128;
typedef setseq_base<pcg128_t, pcg128_t, xsl_rr_rr_mixin, true, cheap_multiplier>
    cm_setseq_xsl_rr_rr_128_128;

                // MCG versions don't make sense here, so aren't defined.

/* Extended generators */

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename BaseRNG, bool kdd = true>
using ext_std8 = extended<table_pow2, advance_pow2, BaseRNG,
                          oneseq_rxs_m_xs_8_8, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename BaseRNG, bool kdd = true>
using ext_std16 = extended<table_pow2, advance_pow2, BaseRNG,
                           oneseq_rxs_m_xs_16_16, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename BaseRNG, bool kdd = true>
using ext_std32 = extended<table_pow2, advance_pow2, BaseRNG,
                           oneseq_rxs_m_xs_32_32, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2,
          typename BaseRNG, bool kdd = true>
using ext_std64 = extended<table_pow2, advance_pow2, BaseRNG,
                           oneseq_rxs_m_xs_64_64, kdd>;


template <bitcount_t table_pow2, bitcount_t advance_pow2, bool kdd = true>
using ext_oneseq_rxs_m_xs_32_32 =
          ext_std32<table_pow2, advance_pow2, oneseq_rxs_m_xs_32_32, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2, bool kdd = true>
using ext_mcg_xsh_rs_64_32 =
          ext_std32<table_pow2, advance_pow2, mcg_xsh_rs_64_32, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2, bool kdd = true>
using ext_oneseq_xsh_rs_64_32 =
          ext_std32<table_pow2, advance_pow2, oneseq_xsh_rs_64_32, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2, bool kdd = true>
using ext_setseq_xsh_rr_64_32 =
          ext_std32<table_pow2, advance_pow2, setseq_xsh_rr_64_32, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2, bool kdd = true>
using ext_mcg_xsl_rr_128_64 =
          ext_std64<table_pow2, advance_pow2, mcg_xsl_rr_128_64, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2, bool kdd = true>
using ext_oneseq_xsl_rr_128_64 =
          ext_std64<table_pow2, advance_pow2, oneseq_xsl_rr_128_64, kdd>;

template <bitcount_t table_pow2, bitcount_t advance_pow2, bool kdd = true>
using ext_setseq_xsl_rr_128_64 =
          ext_std64<table_pow2, advance_pow2, setseq_xsl_rr_128_64, kdd>;

} // namespace pcg_engines

typedef pcg_engines::setseq_xsh_rr_64_32        pcg32;
typedef pcg_engines::oneseq_xsh_rr_64_32        pcg32_oneseq;
typedef pcg_engines::unique_xsh_rr_64_32        pcg32_unique;
typedef pcg_engines::mcg_xsh_rs_64_32           pcg32_fast;

typedef pcg_engines::setseq_xsl_rr_128_64       pcg64;
typedef pcg_engines::oneseq_xsl_rr_128_64       pcg64_oneseq;
typedef pcg_engines::unique_xsl_rr_128_64       pcg64_unique;
typedef pcg_engines::mcg_xsl_rr_128_64          pcg64_fast;

typedef pcg_engines::setseq_rxs_m_xs_8_8        pcg8_once_insecure;
typedef pcg_engines::setseq_rxs_m_xs_16_16      pcg16_once_insecure;
typedef pcg_engines::setseq_rxs_m_xs_32_32      pcg32_once_insecure;
typedef pcg_engines::setseq_rxs_m_xs_64_64      pcg64_once_insecure;
typedef pcg_engines::setseq_xsl_rr_rr_128_128   pcg128_once_insecure;

typedef pcg_engines::oneseq_rxs_m_xs_8_8        pcg8_oneseq_once_insecure;
typedef pcg_engines::oneseq_rxs_m_xs_16_16      pcg16_oneseq_once_insecure;
typedef pcg_engines::oneseq_rxs_m_xs_32_32      pcg32_oneseq_once_insecure;
typedef pcg_engines::oneseq_rxs_m_xs_64_64      pcg64_oneseq_once_insecure;
typedef pcg_engines::oneseq_xsl_rr_rr_128_128   pcg128_oneseq_once_insecure;


// These two extended RNGs provide two-dimensionally equidistributed
// 32-bit generators.  pcg32_k2_fast occupies the same space as pcg64,
// and can be called twice to generate 64 bits, but does not required
// 128-bit math; on 32-bit systems, it's faster than pcg64 as well.

typedef pcg_engines::ext_setseq_xsh_rr_64_32<1,16,true>     pcg32_k2;
typedef pcg_engines::ext_oneseq_xsh_rs_64_32<1,32,true>     pcg32_k2_fast;

// These eight extended RNGs have about as much state as arc4random
//
//  - the k variants are k-dimensionally equidistributed
//  - the c variants offer are intended to be harder to predict
//
// (neither is intended for use in cryptographic applications)

typedef pcg_engines::ext_setseq_xsh_rr_64_32<6,16,true>     pcg32_k64;
typedef pcg_engines::ext_mcg_xsh_rs_64_32<6,32,true>        pcg32_k64_oneseq;
typedef pcg_engines::ext_oneseq_xsh_rs_64_32<6,32,true>     pcg32_k64_fast;

typedef pcg_engines::ext_setseq_xsh_rr_64_32<6,16,false>    pcg32_c64;
typedef pcg_engines::ext_oneseq_xsh_rs_64_32<6,32,false>    pcg32_c64_oneseq;
typedef pcg_engines::ext_mcg_xsh_rs_64_32<6,32,false>       pcg32_c64_fast;

typedef pcg_engines::ext_setseq_xsl_rr_128_64<5,16,true>    pcg64_k32;
typedef pcg_engines::ext_oneseq_xsl_rr_128_64<5,128,true>   pcg64_k32_oneseq;
typedef pcg_engines::ext_mcg_xsl_rr_128_64<5,128,true>      pcg64_k32_fast;

typedef pcg_engines::ext_setseq_xsl_rr_128_64<5,16,false>   pcg64_c32;
typedef pcg_engines::ext_oneseq_xsl_rr_128_64<5,128,false>  pcg64_c32_oneseq;
typedef pcg_engines::ext_mcg_xsl_rr_128_64<5,128,false>     pcg64_c32_fast;

// These eight extended RNGs have more state than the Mersenne twister
//
//  - the k variants are k-dimensionally equidistributed
//  - the c variants offer are intended to be harder to predict
//
// (neither is intended for use in cryptographic applications)

typedef pcg_engines::ext_setseq_xsh_rr_64_32<10,16,true>    pcg32_k1024;
typedef pcg_engines::ext_oneseq_xsh_rs_64_32<10,32,true>    pcg32_k1024_fast;

typedef pcg_engines::ext_setseq_xsh_rr_64_32<10,16,false>   pcg32_c1024;
typedef pcg_engines::ext_oneseq_xsh_rs_64_32<10,32,false>   pcg32_c1024_fast;

typedef pcg_engines::ext_setseq_xsl_rr_128_64<10,16,true>   pcg64_k1024;
typedef pcg_engines::ext_oneseq_xsl_rr_128_64<10,128,true>  pcg64_k1024_fast;

typedef pcg_engines::ext_setseq_xsl_rr_128_64<10,16,false>  pcg64_c1024;
typedef pcg_engines::ext_oneseq_xsl_rr_128_64<10,128,false> pcg64_c1024_fast;

// These generators have an insanely huge period (2^524352), and is suitable
// for silly party tricks, such as dumping out 64 KB ZIP files at an arbitrary
// point in the future.   [Actually, over the full period of the generator, it
// will produce every 64 KB ZIP file 2^64 times!]

typedef pcg_engines::ext_setseq_xsh_rr_64_32<14,16,true>    pcg32_k16384;
typedef pcg_engines::ext_oneseq_xsh_rs_64_32<14,32,true>    pcg32_k16384_fast;

#ifdef _MSC_VER
    #pragma warning(default:4146)
#endif

#endif // PCG_RAND_HPP_INCLUDED
