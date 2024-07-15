// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til::colorbrewer
{
    // The following list of colors is only used as a debug aid and not part of the final product.
    // They're licensed under:
    //
    //   Apache-Style Software License for ColorBrewer software and ColorBrewer Color Schemes
    //
    //   Copyright (c) 2002 Cynthia Brewer, Mark Harrower, and The Pennsylvania State University.
    //
    //   Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License.
    //   You may obtain a copy of the License at
    //
    //   http://www.apache.org/licenses/LICENSE-2.0
    //
    //   Unless required by applicable law or agreed to in writing, software distributed
    //   under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    //   CONDITIONS OF ANY KIND, either express or implied. See the License for the
    //   specific language governing permissions and limitations under the License.
    //
    inline constexpr uint32_t pastel1[]{
        0xfbb4ae,
        0xb3cde3,
        0xccebc5,
        0xdecbe4,
        0xfed9a6,
        0xffffcc,
        0xe5d8bd,
        0xfddaec,
        0xf2f2f2,
    };

    inline constexpr uint32_t dark2[]{
        0x1b9e77,
        0xd95f02,
        0x7570b3,
        0xe7298a,
        0x66a61e,
        0xe6ab02,
        0xa6761d,
        0x666666,
    };
}
