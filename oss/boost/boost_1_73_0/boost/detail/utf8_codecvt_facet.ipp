/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// utf8_codecvt_facet.ipp

// Copyright (c) 2001 Ronald Garcia, Indiana University (garcia@osl.iu.edu)
// Andrew Lumsdaine, Indiana University (lums@osl.iu.edu). 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Please see the comments in <boost/detail/utf8_codecvt_facet.hpp> to
// learn how this file should be used.

#include <boost/detail/utf8_codecvt_facet.hpp>

#include <cstdlib> // for multi-byte converson routines
#include <cassert>

#include <boost/limits.hpp>
#include <boost/config.hpp>

// If we don't have wstring, then Unicode support 
// is not available anyway, so we don't need to even
// compiler this file. This also fixes the problem
// with mingw, which can compile this file, but will
// generate link error when building DLL.
#ifndef BOOST_NO_STD_WSTRING

BOOST_UTF8_BEGIN_NAMESPACE

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// implementation for wchar_t

utf8_codecvt_facet::utf8_codecvt_facet(
    std::size_t no_locale_manage
) :
    std::codecvt<wchar_t, char, std::mbstate_t>(no_locale_manage)
{}

utf8_codecvt_facet::~utf8_codecvt_facet()
{}

// Translate incoming UTF-8 into UCS-4
std::codecvt_base::result utf8_codecvt_facet::do_in(
    std::mbstate_t& /*state*/, 
    const char * from,
    const char * from_end, 
    const char * & from_next,
    wchar_t * to, 
    wchar_t * to_end, 
    wchar_t * & to_next
) const {
    // Basic algorithm:  The first octet determines how many
    // octets total make up the UCS-4 character.  The remaining
    // "continuing octets" all begin with "10". To convert, subtract
    // the amount that specifies the number of octets from the first
    // octet.  Subtract 0x80 (1000 0000) from each continuing octet,
    // then mash the whole lot together.  Note that each continuing
    // octet only uses 6 bits as unique values, so only shift by
    // multiples of 6 to combine.
    while (from != from_end && to != to_end) {

        // Error checking   on the first octet
        if (invalid_leading_octet(*from)){
            from_next = from;
            to_next = to;
            return std::codecvt_base::error;
        }

        // The first octet is   adjusted by a value dependent upon 
        // the number   of "continuing octets" encoding the character
        const   int cont_octet_count = get_cont_octet_count(*from);
        const   wchar_t octet1_modifier_table[] =   {
            0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc
        };

        // The unsigned char conversion is necessary in case char is
        // signed   (I learned this the hard way)
        wchar_t ucs_result = 
            (unsigned char)(*from++) - octet1_modifier_table[cont_octet_count];

        // Invariants   : 
        //   1) At the start of the loop,   'i' continuing characters have been
        //    processed 
        //   2) *from   points to the next continuing character to be processed.
        int i   = 0;
        while(i != cont_octet_count && from != from_end) {

            // Error checking on continuing characters
            if (invalid_continuing_octet(*from)) {
                from_next   = from;
                to_next =   to;
                return std::codecvt_base::error;
            }

            ucs_result *= (1 << 6); 

            // each continuing character has an extra (10xxxxxx)b attached to 
            // it that must be removed.
            ucs_result += (unsigned char)(*from++) - 0x80;
            ++i;
        }

        // If   the buffer ends with an incomplete unicode character...
        if (from == from_end && i   != cont_octet_count) {
            // rewind "from" to before the current character translation
            from_next = from - (i+1); 
            to_next = to;
            return std::codecvt_base::partial;
        }
        *to++   = ucs_result;
    }
    from_next = from;
    to_next = to;

    // Were we done converting or did we run out of destination space?
    if(from == from_end) return std::codecvt_base::ok;
    else return std::codecvt_base::partial;
}

std::codecvt_base::result utf8_codecvt_facet::do_out(
    std::mbstate_t& /*state*/, 
    const wchar_t *   from,
    const wchar_t * from_end, 
    const wchar_t * & from_next,
    char * to, 
    char * to_end, 
    char * & to_next
) const
{
    // RG - consider merging this table with the other one
    const wchar_t octet1_modifier_table[] = {
        0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc
    };

    wchar_t max_wchar = (std::numeric_limits<wchar_t>::max)();
    while (from != from_end && to != to_end) {

        // Check for invalid UCS-4 character
        if (*from  > max_wchar) {
            from_next = from;
            to_next = to;
            return std::codecvt_base::error;
        }

        int cont_octet_count = get_cont_octet_out_count(*from);

        // RG  - comment this formula better
        int shift_exponent = (cont_octet_count) *   6;

        // Process the first character
        *to++ = static_cast<char>(octet1_modifier_table[cont_octet_count] +
            (unsigned char)(*from / (1 << shift_exponent)));

        // Process the continuation characters 
        // Invariants: At   the start of the loop:
        //   1) 'i' continuing octets   have been generated
        //   2) '*to'   points to the next location to place an octet
        //   3) shift_exponent is   6 more than needed for the next octet
        int i   = 0;
        while   (i != cont_octet_count && to != to_end) {
            shift_exponent -= 6;
            *to++ = static_cast<char>(0x80 + ((*from / (1 << shift_exponent)) % (1 << 6)));
            ++i;
        }
        // If   we filled up the out buffer before encoding the character
        if(to   == to_end && i != cont_octet_count) {
            from_next = from;
            to_next = to - (i+1);
            return std::codecvt_base::partial;
        }
        ++from;
    }
    from_next = from;
    to_next = to;
    // Were we done or did we run out of destination space
    if(from == from_end) return std::codecvt_base::ok;
    else return std::codecvt_base::partial;
}

// How many char objects can I process to get <= max_limit
// wchar_t objects?
int utf8_codecvt_facet::do_length(
    std::mbstate_t &,
    const char * from,
    const char * from_end, 
    std::size_t max_limit
) const
#if BOOST_WORKAROUND(__IBMCPP__, BOOST_TESTED_AT(600))
        throw()
#endif
{ 
    // RG - this code is confusing!  I need a better way to express it.
    // and test cases.

    // Invariants:
    // 1) last_octet_count has the size of the last measured character
    // 2) char_count holds the number of characters shown to fit
    // within the bounds so far (no greater than max_limit)
    // 3) from_next points to the octet 'last_octet_count' before the
    // last measured character.  
    int last_octet_count=0;
    std::size_t char_count = 0;
    const char* from_next = from;
    // Use "<" because the buffer may represent incomplete characters
    while (from_next+last_octet_count <= from_end && char_count <= max_limit) {
        from_next += last_octet_count;
        last_octet_count = (get_octet_count(*from_next));
        ++char_count;
    }
    return static_cast<int>(from_next-from);
}

unsigned int utf8_codecvt_facet::get_octet_count(
    unsigned char lead_octet
){
    // if the 0-bit (MSB) is 0, then 1 character
    if (lead_octet <= 0x7f) return 1;

    // Otherwise the count number of consecutive 1 bits starting at MSB
//    assert(0xc0 <= lead_octet && lead_octet <= 0xfd);

    if (0xc0 <= lead_octet && lead_octet <= 0xdf) return 2;
    else if (0xe0 <= lead_octet && lead_octet <= 0xef) return 3;
    else if (0xf0 <= lead_octet && lead_octet <= 0xf7) return 4;
    else if (0xf8 <= lead_octet && lead_octet <= 0xfb) return 5;
    else return 6;
}

namespace detail {

template<std::size_t s>
int get_cont_octet_out_count_impl(wchar_t word){
    if (word < 0x80) {
        return 0;
    }
    if (word < 0x800) {
        return 1;
    }
    return 2;
}

template<>
int get_cont_octet_out_count_impl<4>(wchar_t word){
    if (word < 0x80) {
        return 0;
    }
    if (word < 0x800) {
        return 1;
    }

    // Note that the following code will generate warnings on some platforms
    // where wchar_t is defined as UCS2.  The warnings are superfluous as the
    // specialization is never instantitiated with such compilers, but this
    // can cause problems if warnings are being treated as errors, so we guard
    // against that.  Including <boost/detail/utf8_codecvt_facet.hpp> as we do
    // should be enough to get WCHAR_MAX defined.
#if !defined(WCHAR_MAX)
#   error WCHAR_MAX not defined!
#endif
    // cope with VC++ 7.1 or earlier having invalid WCHAR_MAX
#if defined(_MSC_VER) && _MSC_VER <= 1310 // 7.1 or earlier
    return 2;
#elif WCHAR_MAX > 0x10000
    
   if (word < 0x10000) {
        return 2;
    }
    if (word < 0x200000) {
        return 3;
    }
    if (word < 0x4000000) {
        return 4;
    }
    return 5;
    
#else
    return 2;
#endif
}

} // namespace detail

// How many "continuing octets" will be needed for this word
// ==   total octets - 1.
int utf8_codecvt_facet::get_cont_octet_out_count(
    wchar_t word
) const {
    return detail::get_cont_octet_out_count_impl<sizeof(wchar_t)>(word);
}
BOOST_UTF8_END_NAMESPACE

#endif
