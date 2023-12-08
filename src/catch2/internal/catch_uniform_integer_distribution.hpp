
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#ifndef CATCH_UNIFORM_INTEGER_DISTRIBUTION_HPP_INCLUDED
#define CATCH_UNIFORM_INTEGER_DISTRIBUTION_HPP_INCLUDED

#include <catch2/internal/catch_random_integer_helpers.hpp>

namespace Catch {

/**
 * Implementation of uniform distribution on integers.
 *
 * TODO: explain more?
 *    * optimize for many calls to same distribution (precompute the threshold for rejection method)
 *    * based on Lemire's paper
 *    * precompute various constant factors (range distance, adjustment factor, etc)
 */
template <typename IntegerType>
class uniform_integer_distribution {
    static_assert(std::is_integral<IntegerType>::value, "...");

    using UnsignedIntegerType = std::make_unsigned_t<IntegerType>;

    // TODO: We could store these as the adjusted unsigned type, and then adjust + cast them for the a(), b() getters.

    // We store the left range bound converted to internal representation,
    // because it will be used in computation in the () operator.
    UnsignedIntegerType m_a;
    // After initialization, right bound is only used for the b() getter,
    // so we keep it in the original type.
    IntegerType m_b;

    // How many different values are there in [a, b]. a == b => 1, can be 0 for distribution over all values in the type.
    UnsignedIntegerType m_ab_distance;

    // We hoisted this out of the main generation function. Technically,
    // this means that using this distribution will be slower than Lemire's
    // algorithm if this distribution instance will be used only few times,
    // but it will be faster if it is used many times. Since Catch2 uses
    // distributions only to implement random generators, we assume that each
    // distribution will be reused many times and this is an optimization.
    UnsignedIntegerType m_rejection_threshold = 0;

    // Assumes m_b and m_a are already filled
    UnsignedIntegerType computeDistance() const {
        // This overflows and returns 0 if ua == 0 and ub == TYPE_MAX.
        // We handle that later when generating the number.
        return transposeTo(m_b) - m_a + 1;
    }
    // Assumes m_ab_distance is already computed
    static UnsignedIntegerType computeRejectionThreshold(UnsignedIntegerType ab_distance) {
        // distance == 0 means that we will return all possible values from
        // the type's range, and that we shouldn't reject anything.
        if ( ab_distance == 0 ) { return 0; }
        return ( ~ab_distance + 1 ) % ab_distance;
    }

    static UnsignedIntegerType transposeTo(IntegerType in) {
        return Detail::transposeToNaturalOrder<IntegerType>(
            static_cast<UnsignedIntegerType>( in ) );
    }
    static IntegerType transposeBack(UnsignedIntegerType in) {
        return static_cast<IntegerType>(
            Detail::transposeToNaturalOrder<IntegerType>(in) );
    }

public:
    using result_type = IntegerType;

    uniform_integer_distribution( IntegerType a, IntegerType b ):
        m_a( transposeTo(a) ),
        m_b( b ),
        m_ab_distance( computeDistance() ),
        m_rejection_threshold( computeRejectionThreshold(m_ab_distance) ) {
        assert( a <= b );
    }

    template <typename Generator>
    result_type operator()( Generator& g ) {
        // All possible values of result_Type are valid.
        if ( m_ab_distance == 0 ) {
            return transposeBack( Detail::fillBitsFrom<UnsignedIntegerType>( g ) );
        }

        auto random_number = Detail::fillBitsFrom<UnsignedIntegerType>( g );
        auto emul = Detail::ExtendedMult( random_number, m_ab_distance );
        // This means that we have to compute a proper rejection threshold
        // and handle iterating until we get results.
        if ( emul.lower < m_ab_distance ) {
            while (emul.lower < m_rejection_threshold) {
                random_number = Detail::fillBitsFrom<UnsignedIntegerType>( g );
                emul = Detail::ExtendedMult( random_number, m_ab_distance );
            }
        }

        return transposeBack(m_a + emul.upper);
    }

    result_type a() const { return transposeBack(m_a); }
    result_type b() const { return m_b; }
};

} // end namespace Catch

#endif // CATCH_UNIFORM_INTEGER_DISTRIBUTION_HPP_INCLUDED
