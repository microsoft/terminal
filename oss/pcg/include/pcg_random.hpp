// PCG Random Number Generation for C++
//
// Copyright 2014-2019 Melissa O'Neill <oneill@pcg-random.org>,
//                     and the PCG Project contributors.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
//
// Licensed under the Apache License, Version 2.0 (provided in
// LICENSE-APACHE.txt and at http://www.apache.org/licenses/LICENSE-2.0)
// or under the MIT license (provided in LICENSE-MIT.txt and at
// http://opensource.org/licenses/MIT), at your option. This file may not
// be copied, modified, or distributed except according to those terms.
//
// Distributed on an "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, either
// express or implied.  See your chosen license for details.
//
// For additional information about the PCG random number generation scheme,
// visit http://www.pcg-random.org/.
//
// -----------------------------------------------------------------------------
//
// Leonard Hecker <lhecker@microsoft.com>:
// The following contents are an extract of pcg_engines::oneseq_dxsm_64_32
// reduced down to the bare essentials, while retaining base functionality.

namespace pcg_engines {
    class oneseq_dxsm_64_32 {
        using xtype = uint32_t;
        using itype = uint64_t;

        itype state_;

        static constexpr uint64_t multiplier() {
            return 6364136223846793005ULL;
        }

        static constexpr uint64_t increment() {
            return 1442695040888963407ULL;
        }

        static itype bump(itype state) {
            return state * multiplier() + increment();
        }

        itype base_generate0() {
            itype old_state = state_;
            state_ = bump(state_);
            return old_state;
        }

    public:
        explicit oneseq_dxsm_64_32(itype state = 0xcafef00dd15ea5e5ULL) : state_(bump(state + increment())) {
        }

        // Returns a value in the interval [0, UINT32_MAX].
        xtype operator()() {
            constexpr auto xtypebits = uint8_t(sizeof(xtype) * 8);
            constexpr auto itypebits = uint8_t(sizeof(itype) * 8);

            auto internal = base_generate0();
            auto hi = xtype(internal >> (itypebits - xtypebits));
            auto lo = xtype(internal);

            lo |= 1;
            hi ^= hi >> (xtypebits / 2);
            hi *= xtype(multiplier());
            hi ^= hi >> (3 * (xtypebits / 4));
            hi *= lo;
            return hi;
        }

        // Returns a value in the interval [0, upper_bound).
        xtype operator()(xtype upper_bound) {
            uint32_t threshold = (UINT64_MAX + uint32_t(1) - upper_bound) % upper_bound;
            for (;;) {
                auto r = operator()();
                if (r >= threshold)
                    return r % upper_bound;
            }
        }
    };
}
