//
// Copyright (c) 2019 Maxime Pinard
//
// Distributed under the MIT license
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
//
#ifndef DYNAMIC_BITSET_DYNAMIC_BITSET_HPP
#define DYNAMIC_BITSET_DYNAMIC_BITSET_HPP

#include <memory>
#include <vector>
#include <algorithm>
#include <string>
#include <string_view>
#include <functional>
#include <type_traits>
#include <climits>
#include <cmath>
#include <cassert>

#ifndef DYNAMIC_BITSET_NO_LIBPOPCNT
#	if __has_include(<libpopcnt.h>)
#		include <libpopcnt.h>
#		define DYNAMIC_BITSET_USE_LIBPOPCNT
#	endif
#endif

#if defined(__clang__)
#	define DYNAMIC_BITSET_CLANG
#	ifdef __has_builtin
#		if __has_builtin(__builtin_popcount) && __has_builtin(__builtin_popcountl) \
		  && __has_builtin(__builtin_popcountll)
#			define DYNAMIC_BITSET_CLANG_builtin_popcount
#		endif
#		if __has_builtin(__builtin_ctz) && __has_builtin(__builtin_ctzl) \
		  && __has_builtin(__builtin_ctzll)
#			define DYNAMIC_BITSET_CLANG_builtin_ctz
#		endif
#	endif
#elif defined(__GNUC__)
#	define DYNAMIC_BITSET_GCC
#elif defined(_MSC_VER)
#	define DYNAMIC_BITSET_MSVC
#	include <intrin.h>
#	pragma intrinsic(_BitScanForward)
#	if defined(_M_X64) || defined(_M_ARM64)
#		define DYNAMIC_BITSET_MSVC_64
#		pragma intrinsic(_BitScanForward64)
#	else
#	endif
#endif

template<typename Block = unsigned long long, typename Allocator = std::allocator<Block>>
class dynamic_bitset
{
	static_assert(std::is_unsigned<Block>::value, "Block is not an unsigned integral type");

public:
	typedef size_t size_type;
	typedef Block block_type;
	typedef Allocator allocator_type;

	static constexpr size_type bits_per_block = CHAR_BIT * sizeof(block_type);
	static constexpr size_type npos = std::numeric_limits<size_type>::max();

	class reference
	{
	public:
		constexpr reference(dynamic_bitset<Block, Allocator>& bitset, size_type bit_pos);
		constexpr reference(const reference&) noexcept = default;
		constexpr reference(reference&&) noexcept = default;
		~reference() noexcept = default;

		constexpr reference& operator=(bool v);
		constexpr reference& operator=(const reference& rhs);
		constexpr reference& operator=(reference&& rhs) noexcept;

		constexpr reference& operator&=(bool v);
		constexpr reference& operator|=(bool v);
		constexpr reference& operator^=(bool v);
		constexpr reference& operator-=(bool v);

		[[nodiscard]] constexpr bool operator~() const;
		[[nodiscard]] constexpr operator bool() const;
		constexpr void operator&() = delete;

		constexpr reference& set();
		constexpr reference& reset();
		constexpr reference& flip();
		constexpr reference& assign(bool v);

	private:
		block_type& m_block;
		block_type m_mask;
	};
	typedef bool const_reference;

	// copy/move constructors = default
	constexpr dynamic_bitset(const dynamic_bitset<Block, Allocator>& other) = default;
	constexpr dynamic_bitset(dynamic_bitset<Block, Allocator>&& other) noexcept = default;
	constexpr dynamic_bitset<Block, Allocator>& operator=(
	  const dynamic_bitset<Block, Allocator>& other) = default;
	constexpr dynamic_bitset<Block, Allocator>& operator=(
	  dynamic_bitset<Block, Allocator>&& other) noexcept = default;

	// other constructors
	constexpr explicit dynamic_bitset(const allocator_type& allocator = allocator_type());
	constexpr explicit dynamic_bitset(size_type nbits,
	                                  unsigned long long init_val = 0,
	                                  const allocator_type& allocator = allocator_type());
	constexpr dynamic_bitset(std::initializer_list<block_type> init_vals,
	                         const allocator_type& allocator = allocator_type());

	// string constructors
	template<typename _CharT, typename _Traits>
	constexpr explicit dynamic_bitset(
	  std::basic_string_view<_CharT, _Traits> str,
	  typename std::basic_string_view<_CharT, _Traits>::size_type pos = 0,
	  typename std::basic_string_view<_CharT, _Traits>::size_type n =
	    std::basic_string_view<_CharT, _Traits>::npos,
	  _CharT zero = _CharT('0'),
	  _CharT one = _CharT('1'),
	  const allocator_type& allocator = allocator_type());

	template<typename _CharT, typename _Traits, typename _Alloc>
	constexpr explicit dynamic_bitset(
	  const std::basic_string<_CharT, _Traits, _Alloc>& str,
	  typename std::basic_string<_CharT, _Traits, _Alloc>::size_type pos = 0,
	  typename std::basic_string<_CharT, _Traits, _Alloc>::size_type n =
	    std::basic_string<_CharT, _Traits, _Alloc>::npos,
	  _CharT zero = _CharT('0'),
	  _CharT one = _CharT('1'),
	  const allocator_type& allocator = allocator_type());

	template<typename _CharT>
	constexpr explicit dynamic_bitset(
	  const _CharT* str,
	  typename std::basic_string<_CharT>::size_type pos = 0,
	  typename std::basic_string<_CharT>::size_type n = std::basic_string<_CharT>::npos,
	  _CharT zero = _CharT('0'),
	  _CharT one = _CharT('1'),
	  const allocator_type& allocator = allocator_type());

	// destructor
	~dynamic_bitset() noexcept = default;

	// size changing operations
	constexpr void resize(size_type nbits, bool value = false);
	constexpr void clear();
	constexpr void push_back(bool value);
	constexpr void pop_back();
	constexpr void append(block_type block);
	constexpr void append(std::initializer_list<block_type> blocks);
	template<typename BlockInputIterator>
	constexpr void append(BlockInputIterator first, BlockInputIterator last);

	// bitset operations
	constexpr dynamic_bitset<Block, Allocator>& operator&=(
	  const dynamic_bitset<Block, Allocator>& rhs);
	constexpr dynamic_bitset<Block, Allocator>& operator|=(
	  const dynamic_bitset<Block, Allocator>& rhs);
	constexpr dynamic_bitset<Block, Allocator>& operator^=(
	  const dynamic_bitset<Block, Allocator>& rhs);
	constexpr dynamic_bitset<Block, Allocator>& operator-=(
	  const dynamic_bitset<Block, Allocator>& rhs);
	constexpr dynamic_bitset<Block, Allocator>& operator<<=(size_type shift);
	constexpr dynamic_bitset<Block, Allocator>& operator>>=(size_type shift);
	[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator<<(size_type shift) const;
	[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator>>(size_type shift) const;
	[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator~() const;

	// bit operations
	constexpr dynamic_bitset<Block, Allocator>& set(size_type pos, size_type len, bool value);
	constexpr dynamic_bitset<Block, Allocator>& set(size_type pos, bool value = true);
	constexpr dynamic_bitset<Block, Allocator>& set();
	constexpr dynamic_bitset<Block, Allocator>& reset(size_type pos, size_type len);
	constexpr dynamic_bitset<Block, Allocator>& reset(size_type pos);
	constexpr dynamic_bitset<Block, Allocator>& reset();
	constexpr dynamic_bitset<Block, Allocator>& flip(size_type pos, size_type len);
	constexpr dynamic_bitset<Block, Allocator>& flip(size_type pos);
	constexpr dynamic_bitset<Block, Allocator>& flip();
	[[nodiscard]] constexpr bool test(size_type pos) const;
	[[nodiscard]] constexpr bool test_set(size_type pos, bool value = true);
	[[nodiscard]] constexpr bool all() const;
	[[nodiscard]] constexpr bool any() const;
	[[nodiscard]] constexpr bool none() const;
	[[nodiscard]] constexpr size_type count() const noexcept;

	// subscript operators
	[[nodiscard]] constexpr reference operator[](size_type pos);
	[[nodiscard]] constexpr const_reference operator[](size_type pos) const;

	//container-like functions
	[[nodiscard]] constexpr size_type size() const noexcept;
	[[nodiscard]] constexpr size_type num_blocks() const noexcept;
	[[nodiscard]] constexpr bool empty() const noexcept;
	[[nodiscard]] constexpr size_type capacity() const noexcept;
	constexpr void reserve(size_type num_bits);
	constexpr void shrink_to_fit();

	// subsets
	[[nodiscard]] constexpr bool is_subset_of(const dynamic_bitset<Block, Allocator>& bitset) const;
	[[nodiscard]] constexpr bool is_proper_subset_of(
	  const dynamic_bitset<Block, Allocator>& bitset) const;
	[[nodiscard]] constexpr bool intersects(const dynamic_bitset<Block, Allocator>& bitset) const;

	// find functions
	[[nodiscard]] constexpr size_type find_first() const;
	[[nodiscard]] constexpr size_type find_next(size_type prev) const;

	// utils
	constexpr void swap(dynamic_bitset<Block, Allocator>& other);
	[[nodiscard]] constexpr allocator_type get_allocator() const;
	template<typename _CharT = char,
	         typename _Traits = std::char_traits<_CharT>,
	         typename _Alloc = std::allocator<_CharT>>
	[[nodiscard]] constexpr std::basic_string<_CharT, _Traits, _Alloc> to_string(
	  _CharT zero = _CharT('0'),
	  _CharT one = _CharT('1')) const;
	template<typename Function, typename... Parameters>
	constexpr void iterate_bits_on(Function&& function, Parameters&&... parameters) const;

	// friend external binary operators
	template<typename Block_, typename Allocator_>
	friend constexpr bool operator==(const dynamic_bitset<Block_, Allocator_>& lhs,
	                                 const dynamic_bitset<Block_, Allocator_>& rhs);
	template<typename Block_, typename Allocator_>
	friend constexpr bool operator<(const dynamic_bitset<Block_, Allocator_>& lhs,
	                                const dynamic_bitset<Block_, Allocator_>& rhs);

private:
	template<typename T>
	struct dependent_false : public std::false_type
	{
	};

	std::vector<Block, Allocator> m_blocks;
	size_type m_bits_number;

	static constexpr block_type zero_block = block_type(0);
	static constexpr block_type one_block = block_type(~zero_block);
	static constexpr size_type block_last_bit_index = bits_per_block - 1;

	static constexpr size_type blocks_required(size_type nbits) noexcept;

	static constexpr size_type block_index(size_type pos) noexcept;
	static constexpr size_type bit_index(size_type pos) noexcept;

	static constexpr block_type bit_mask(size_type pos) noexcept;
	static constexpr block_type bit_mask(size_type first, size_type last) noexcept;

	static constexpr void set_block_bits(block_type& block,
	                                     size_type first,
	                                     size_type last,
	                                     bool val = true) noexcept;
	static constexpr void flip_block_bits(block_type& block,
	                                      size_type first,
	                                      size_type last) noexcept;

	static constexpr size_type block_count(const block_type& block) noexcept;
	static constexpr size_type block_count(const block_type& block, size_type nbits) noexcept;

	static constexpr size_type first_on(const block_type& block) noexcept;

	template<typename _CharT, typename _Traits>
	constexpr void init_from_string(std::basic_string_view<_CharT, _Traits> str,
	                                typename std::basic_string_view<_CharT, _Traits>::size_type pos,
	                                typename std::basic_string_view<_CharT, _Traits>::size_type n,
	                                _CharT zero,
	                                _CharT one);

	constexpr block_type& get_block(size_type pos);
	constexpr const block_type& get_block(size_type pos) const;
	constexpr block_type& last_block();
	constexpr block_type last_block() const;

	// used bits in the last block
	constexpr size_type extra_bits_number() const noexcept;
	// unused bits in the last block
	constexpr size_type unused_bits_number() const noexcept;

	template<typename BinaryOperation>
	constexpr void apply(const dynamic_bitset<Block, Allocator>& other, BinaryOperation binary_op);
	template<typename UnaryOperation>
	constexpr void apply(UnaryOperation unary_op);
	constexpr void apply_left_shift(size_type shift);
	constexpr void apply_right_shift(size_type shift);

	// reset unused bits to 0
	constexpr void sanitize();

	// check functions used in asserts
	constexpr bool check_unused_bits() const noexcept;
	constexpr bool check_size() const noexcept;
	constexpr bool check_consistency() const noexcept;
};

// Deduction guideline for expressions like "dynamic_bitset a(32);" with an integral type as parameter
// to use the constructor with the initial size instead of the constructor with the allocator.
template<typename integral_type, typename = std::enable_if_t<std::is_integral_v<integral_type>>>
dynamic_bitset(integral_type)->dynamic_bitset<>;

//=================================================================================================
// dynamic_bitset external functions declarations
//=================================================================================================

template<typename Block, typename Allocator>
constexpr bool operator!=(const dynamic_bitset<Block, Allocator>& lhs,
                          const dynamic_bitset<Block, Allocator>& rhs);
template<typename Block, typename Allocator>
constexpr bool operator<=(const dynamic_bitset<Block, Allocator>& lhs,
                          const dynamic_bitset<Block, Allocator>& rhs);
template<typename Block, typename Allocator>
constexpr bool operator>(const dynamic_bitset<Block, Allocator>& lhs,
                         const dynamic_bitset<Block, Allocator>& rhs);
template<typename Block, typename Allocator>
constexpr bool operator>=(const dynamic_bitset<Block, Allocator>& lhs,
                          const dynamic_bitset<Block, Allocator>& rhs);

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator&(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs);
template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator|(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs);
template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator^(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs);
template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator-(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs);

template<typename _CharT, typename _Traits, typename Block, typename Allocator>
constexpr std::basic_ostream<_CharT, _Traits>& operator<<(
  std::basic_ostream<_CharT, _Traits>& os,
  const dynamic_bitset<Block, Allocator>& bitset);

template<typename _CharT, typename _Traits, typename Block, typename Allocator>
constexpr std::basic_istream<_CharT, _Traits>& operator>>(std::basic_istream<_CharT, _Traits>& is,
                                                          dynamic_bitset<Block, Allocator>& bitset);

template<typename Block, typename Allocator>
constexpr void swap(dynamic_bitset<Block, Allocator>& bitset1,
                    dynamic_bitset<Block, Allocator>& bitset2);

//=================================================================================================
// dynamic_bitset::reference functions implementations
//=================================================================================================

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>::reference::reference(
  dynamic_bitset<Block, Allocator>& bitset,
  size_type bit_pos)
  : m_block(bitset.get_block(bit_pos)), m_mask(dynamic_bitset<Block, Allocator>::bit_mask(bit_pos))
{
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::operator=(bool v)
{
	assign(v);
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::operator=(const dynamic_bitset<Block, Allocator>::reference& rhs)
{
	assign(rhs);
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::operator=(dynamic_bitset::reference&& rhs) noexcept
{
	assign(rhs);
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::operator&=(bool v)
{
	if(!v)
	{
		reset();
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::operator|=(bool v)
{
	if(v)
	{
		set();
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::operator^=(bool v)
{
	if(v)
	{
		flip();
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::operator-=(bool v)
{
	if(v)
	{
		reset();
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::reference::operator~() const
{
	return (m_block & m_mask) == zero_block;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>::reference::operator bool() const
{
	return (m_block & m_mask) != zero_block;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::set()
{
	m_block |= m_mask;
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::reset()
{
	m_block &= ~m_mask;
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::flip()
{
	m_block ^= m_mask;
	return *this;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::reference& dynamic_bitset<Block, Allocator>::
  reference::assign(bool v)
{
	if(v)
	{
		set();
	}
	else
	{
		reset();
	}
	return *this;
}

//=================================================================================================
// dynamic_bitset public functions implementations
//=================================================================================================

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>::dynamic_bitset(const allocator_type& allocator)
  : m_blocks(allocator), m_bits_number(0)
{
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>::dynamic_bitset(size_type nbits,
                                                           unsigned long long init_val,
                                                           const allocator_type& allocator)
  : m_blocks(blocks_required(nbits), allocator), m_bits_number(nbits)
{
	if(nbits == 0 || init_val == 0)
	{
		return;
	}

	constexpr size_type init_val_required_blocks = sizeof(unsigned long long) / sizeof(block_type);
	if constexpr(init_val_required_blocks == 1)
	{
		m_blocks[0] = init_val;
	}
	else
	{
		const unsigned long long block_mask = static_cast<unsigned long long>(one_block);
		const size_type blocks_to_init = std::min(m_blocks.size(), init_val_required_blocks);
		for(size_type i = 0; i < blocks_to_init; ++i)
		{
			m_blocks[i] = block_type((init_val >> (i * bits_per_block) & block_mask));
		}
	}
	sanitize();
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>::dynamic_bitset(
  std::initializer_list<block_type> init_vals,
  const allocator_type& allocator)
  : m_blocks(allocator), m_bits_number(0)
{
	append(init_vals);
}

template<typename Block, typename Allocator>
template<typename _CharT, typename _Traits>
constexpr dynamic_bitset<Block, Allocator>::dynamic_bitset(
  std::basic_string_view<_CharT, _Traits> str,
  typename std::basic_string_view<_CharT, _Traits>::size_type pos,
  typename std::basic_string_view<_CharT, _Traits>::size_type n,
  _CharT zero,
  _CharT one,
  const allocator_type& allocator)
  : m_blocks(allocator), m_bits_number(0)
{
	assert(pos < str.size());
	init_from_string(str, pos, n, zero, one);
}

template<typename Block, typename Allocator>
template<typename _CharT, typename _Traits, typename _Alloc>
constexpr dynamic_bitset<Block, Allocator>::dynamic_bitset(
  const std::basic_string<_CharT, _Traits, _Alloc>& str,
  typename std::basic_string<_CharT, _Traits, _Alloc>::size_type pos,
  typename std::basic_string<_CharT, _Traits, _Alloc>::size_type n,
  _CharT zero,
  _CharT one,
  const allocator_type& allocator)
  : m_blocks(allocator), m_bits_number(0)
{
	assert(pos < str.size());
	init_from_string(std::basic_string_view<_CharT, _Traits>(str), pos, n, zero, one);
}

template<typename Block, typename Allocator>
template<typename _CharT>
constexpr dynamic_bitset<Block, Allocator>::dynamic_bitset(
  const _CharT* str,
  typename std::basic_string<_CharT>::size_type pos,
  typename std::basic_string<_CharT>::size_type n,
  _CharT zero,
  _CharT one,
  const allocator_type& allocator)
  : m_blocks(allocator), m_bits_number(0)
{
	init_from_string(std::basic_string_view<_CharT>(str), pos, n, zero, one);
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::resize(size_type nbits, bool value)
{
	if(nbits == m_bits_number)
	{
		return;
	}

	const size_type old_num_blocks = num_blocks();
	const size_type new_num_blocks = blocks_required(nbits);

	const block_type init_value = value ? one_block : zero_block;
	if(new_num_blocks != old_num_blocks)
	{
		m_blocks.resize(new_num_blocks, init_value);
	}

	if(value && nbits > m_bits_number && old_num_blocks > 0)
	{
		// set value of the new bits in the old last block
		const size_type extra_bits = extra_bits_number();
		if(extra_bits > 0)
		{
			m_blocks[old_num_blocks - 1] |= (init_value << extra_bits);
		}
	}

	m_bits_number = nbits;
	sanitize();
	assert(check_consistency());
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::clear()
{
	m_blocks.clear();
	m_bits_number = 0;
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::push_back(bool value)
{
	const size_type new_last_bit = m_bits_number++;
	if(m_bits_number <= m_blocks.size() * bits_per_block)
	{
		if(value)
		{
			set(new_last_bit, value);
		}
	}
	else
	{
		m_blocks.push_back(block_type(value));
	}
	assert(operator[](new_last_bit) == value);
	assert(check_consistency());
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::pop_back()
{
	if(empty())
	{
		return;
	}

	--m_bits_number;
	if(m_blocks.size() > blocks_required(m_bits_number))
	{
		m_blocks.pop_back();
		// no extra bits: sanitize not required
		assert(extra_bits_number() == 0);
	}
	else
	{
		sanitize();
	}
	assert(check_consistency());
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::append(block_type block)
{
	const size_type extra_bits = extra_bits_number();
	if(extra_bits == 0)
	{
		m_blocks.push_back(block);
	}
	else
	{
		last_block() |= (block << extra_bits);
		m_blocks.push_back(block_type(block >> (bits_per_block - extra_bits)));
	}

	m_bits_number += bits_per_block;
	assert(check_consistency());
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::append(std::initializer_list<block_type> blocks)
{
	if(blocks.size() == 0)
	{
		return;
	}

	append(std::cbegin(blocks), std::cend(blocks));
}

template<typename Block, typename Allocator>
template<typename BlockInputIterator>
constexpr void dynamic_bitset<Block, Allocator>::append(BlockInputIterator first,
                                                        BlockInputIterator last)
{
	if(first == last)
	{
		return;
	}

	// if random access iterators, std::distance complexity is constant
	if constexpr(std::is_same_v<
	               typename std::iterator_traits<BlockInputIterator>::iterator_category,
	               std::random_access_iterator_tag>)
	{
		assert(std::distance(first, last) > 0);
		m_blocks.reserve(m_blocks.size() + static_cast<size_type>(std::distance(first, last)));
	}

	const size_type extra_bits = extra_bits_number();
	const size_type unused_bits = unused_bits_number();
	if(extra_bits == 0)
	{
		auto pos = m_blocks.insert(std::end(m_blocks), first, last);
		assert(std::distance(pos, std::end(m_blocks)) > 0);
		m_bits_number +=
		  static_cast<size_type>(std::distance(pos, std::end(m_blocks))) * bits_per_block;
	}
	else
	{
		last_block() |= (*first << extra_bits);
		block_type block = block_type(*first >> unused_bits);
		++first;
		while(first != last)
		{
			block |= (*first << extra_bits);
			m_blocks.push_back(block);
			m_bits_number += bits_per_block;
			block = block_type(*first >> unused_bits);
			++first;
		}
		m_blocks.push_back(block);
		m_bits_number += bits_per_block;
	}

	assert(check_consistency());
}
template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::operator&=(
  const dynamic_bitset<Block, Allocator>& rhs)
{
	assert(size() == rhs.size());
	//apply(rhs, std::bit_and());
	for(size_type i = 0; i < m_blocks.size(); ++i)
	{
		m_blocks[i] &= rhs.m_blocks[i];
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::operator|=(
  const dynamic_bitset<Block, Allocator>& rhs)
{
	assert(size() == rhs.size());
	//apply(rhs, std::bit_or());
	for(size_type i = 0; i < m_blocks.size(); ++i)
	{
		m_blocks[i] |= rhs.m_blocks[i];
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::operator^=(
  const dynamic_bitset<Block, Allocator>& rhs)
{
	assert(size() == rhs.size());
	//apply(rhs, std::bit_xor());
	for(size_type i = 0; i < m_blocks.size(); ++i)
	{
		m_blocks[i] ^= rhs.m_blocks[i];
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::operator-=(
  const dynamic_bitset<Block, Allocator>& rhs)
{
	assert(size() == rhs.size());
	//apply(rhs, [](const block_type& x, const block_type& y) { return (x & ~y); });
	for(size_type i = 0; i < m_blocks.size(); ++i)
	{
		m_blocks[i] &= ~rhs.m_blocks[i];
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::operator<<=(
  size_type shift)
{
	if(shift != 0)
	{
		if(shift >= m_bits_number)
		{
			reset();
		}
		else
		{
			apply_left_shift(shift);
			sanitize(); // unused bits can have changed, reset them to 0
		}
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::operator>>=(
  size_type shift)
{
	if(shift != 0)
	{
		if(shift >= m_bits_number)
		{
			reset();
		}
		else
		{
			apply_right_shift(shift);
		}
	}
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> dynamic_bitset<Block, Allocator>::operator<<(
  size_type shift) const
{
	return dynamic_bitset<Block, Allocator>(*this) <<= shift;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> dynamic_bitset<Block, Allocator>::operator>>(
  size_type shift) const
{
	return dynamic_bitset<Block, Allocator>(*this) >>= shift;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> dynamic_bitset<Block, Allocator>::operator~() const
{
	dynamic_bitset<Block, Allocator> bitset(*this);
	bitset.flip();
	return bitset;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::set(size_type pos,
                                                                                  size_type len,
                                                                                  bool value)
{
	assert(pos < size());
	assert(pos + len - 1 < size());
	if(len == 0)
	{
		return *this;
	}

	const size_type first_block = block_index(pos);
	const size_type last_block = block_index(pos + len - 1);
	const size_type first_bit_index = bit_index(pos);
	const size_type last_bit_index = bit_index(pos + len - 1);

	if(first_block == last_block)
	{
		set_block_bits(m_blocks[first_block], first_bit_index, last_bit_index, value);
	}
	else
	{
		size_type first_full_block = first_block;
		size_type last_full_block = last_block;

		if(first_bit_index != 0)
		{
			++first_full_block; // first block is not full
			set_block_bits(m_blocks[first_block], first_bit_index, block_last_bit_index, value);
		}

		if(last_bit_index != block_last_bit_index)
		{
			--last_full_block; // last block is not full
			set_block_bits(m_blocks[last_block], 0, last_bit_index, value);
		}

		const block_type full_block = value ? one_block : zero_block;
		for(size_type i = first_full_block; i <= last_full_block; ++i)
		{
			m_blocks[i] = full_block;
		}
	}

	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::set(size_type pos,
                                                                                  bool value)
{
	assert(pos < size());

	if(value)
	{
		m_blocks[block_index(pos)] |= bit_mask(pos);
	}
	else
	{
		m_blocks[block_index(pos)] &= ~bit_mask(pos);
	}

	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::set()
{
	std::fill(std::begin(m_blocks), std::end(m_blocks), one_block);
	sanitize();
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::reset(size_type pos,
                                                                                    size_type len)
{
	return set(pos, len, false);
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::reset(size_type pos)
{
	return set(pos, false);
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::reset()
{
	std::fill(std::begin(m_blocks), std::end(m_blocks), zero_block);
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::flip(size_type pos,
                                                                                   size_type len)
{
	assert(pos < size());
	assert(pos + len - 1 < size());
	if(len == 0)
	{
		return *this;
	}

	const size_type first_block = block_index(pos);
	const size_type last_block = block_index(pos + len - 1);
	const size_type first_bit_index = bit_index(pos);
	const size_type last_bit_index = bit_index(pos + len - 1);

	if(first_block == last_block)
	{
		flip_block_bits(m_blocks[first_block], first_bit_index, last_bit_index);
	}
	else
	{
		size_type first_full_block = first_block;
		size_type last_full_block = last_block;

		if(first_bit_index != 0)
		{
			++first_full_block; // first block is not full
			flip_block_bits(m_blocks[first_block], first_bit_index, block_last_bit_index);
		}

		if(last_bit_index != block_last_bit_index)
		{
			--last_full_block; // last block is not full
			flip_block_bits(m_blocks[last_block], 0, last_bit_index);
		}

		for(size_type i = first_full_block; i <= last_full_block; ++i)
		{
			m_blocks[i] = block_type(~m_blocks[i]);
		}
	}

	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::flip(size_type pos)
{
	assert(pos < size());
	m_blocks[block_index(pos)] ^= bit_mask(pos);
	return *this;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator>& dynamic_bitset<Block, Allocator>::flip()
{
	std::transform(
	  std::cbegin(m_blocks), std::cend(m_blocks), std::begin(m_blocks), std::bit_not<block_type>());
	sanitize();
	return *this;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::test(size_type pos) const
{
	assert(pos < size());
	return (m_blocks[block_index(pos)] & bit_mask(pos)) != zero_block;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::test_set(size_type pos, bool value)
{
	bool const result = test(pos);
	if(result != value)
	{
		set(pos, value);
	}
	return result;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::all() const
{
	if(empty())
	{
		return true;
	}

	const block_type full_block = one_block;
	if(extra_bits_number() == 0)
	{
		for(const block_type& block: m_blocks)
		{
			if(block != full_block)
			{
				return false;
			}
		}
	}
	else
	{
		for(size_type i = 0; i < m_blocks.size() - 1; ++i)
		{
			if(m_blocks[i] != full_block)
			{
				return false;
			}
		}
		if(last_block() != (full_block >> unused_bits_number()))
		{
			return false;
		}
	}
	return true;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::any() const
{
	for(const block_type& block: m_blocks)
	{
		if(block != zero_block)
		{
			return true;
		}
	}
	return false;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::none() const
{
	return !any();
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block,
                                                                              Allocator>::count()
  const noexcept
{
	if(empty())
	{
		return 0;
	}

#ifdef DYNAMIC_BITSET_USE_LIBPOPCNT
	const size_type count =
	  static_cast<size_type>(popcnt(m_blocks.data(), m_blocks.size() * sizeof(block_type)));
#else
	size_type count = 0;

	// full blocks
	for(size_type i = 0; i < m_blocks.size() - 1; ++i)
	{
		count += block_count(m_blocks[i]);
	}

	// last block
	const block_type& block = last_block();
	if(block != zero_block)
	{
		const size_t extra_bits = extra_bits_number();
		if(extra_bits == 0)
		{
			count += block_count(block);
		}
		else
		{
			count += block_count(block, extra_bits);
		}
	}
#endif
	return count;
}

template<typename Block, typename Allocator>
constexpr
  typename dynamic_bitset<Block, Allocator>::reference dynamic_bitset<Block, Allocator>::operator[](
    size_type pos)
{
	assert(pos < size());
	return dynamic_bitset<Block, Allocator>::reference(*this, pos);
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::
  const_reference dynamic_bitset<Block, Allocator>::operator[](size_type pos) const
{
	return test(pos);
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block,
                                                                              Allocator>::size()
  const noexcept
{
	return m_bits_number;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  num_blocks() const noexcept
{
	return m_blocks.size();
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::empty() const noexcept
{
	return size() == 0;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block,
                                                                              Allocator>::capacity()
  const noexcept
{
	return m_blocks.capacity() * bits_per_block;
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::reserve(size_type num_bits)
{
	m_blocks.reserve(blocks_required(num_bits));
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::shrink_to_fit()
{
	m_blocks.shrink_to_fit();
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::is_subset_of(
  const dynamic_bitset<Block, Allocator>& bitset) const
{
	assert(size() == bitset.size());
	for(size_type i = 0; i < m_blocks.size(); ++i)
	{
		if((m_blocks[i] & ~bitset.m_blocks[i]) != zero_block)
		{
			return false;
		}
	}
	return true;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::is_proper_subset_of(
  const dynamic_bitset<Block, Allocator>& bitset) const
{
	assert(size() == bitset.size());
	bool is_proper = false;
	for(size_type i = 0; i < m_blocks.size(); ++i)
	{
		const block_type& self_block = m_blocks[i];
		const block_type& other_block = bitset.m_blocks[i];

		if((self_block & ~other_block) != zero_block)
		{
			return false;
		}
		if((~self_block & other_block) != zero_block)
		{
			is_proper = true;
		}
	}
	return is_proper;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::intersects(
  const dynamic_bitset<Block, Allocator>& bitset) const
{
	const size_type min_blocks_number = std::min(m_blocks.size(), bitset.m_blocks.size());
	for(size_type i = 0; i < min_blocks_number; ++i)
	{
		if((m_blocks[i] & bitset.m_blocks[i]) != zero_block)
		{
			return true;
		}
	}
	return false;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  find_first() const
{
	for(size_type i = 0; i < m_blocks.size(); ++i)
	{
		if(m_blocks[i] != zero_block)
		{
			return i * bits_per_block + first_on(m_blocks[i]);
		}
	}
	return npos;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  find_next(size_type prev) const
{
	if(empty() || prev >= (size() - 1))
	{
		return npos;
	}

	const size_type first_bit = prev + 1;
	const size_type first_block = block_index(first_bit);
	const size_type first_bit_index = bit_index(first_bit);
	const block_type first_block_shifted = block_type(m_blocks[first_block] >> first_bit_index);

	if(first_block_shifted != zero_block)
	{
		return first_bit + first_on(first_block_shifted);
	}
	else
	{
		for(size_type i = first_block + 1; i < m_blocks.size(); ++i)
		{
			if(m_blocks[i] != zero_block)
			{
				return i * bits_per_block + first_on(m_blocks[i]);
			}
		}
	}
	return npos;
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::swap(dynamic_bitset<Block, Allocator>& other)
{
	std::swap(m_blocks, other.m_blocks);
	std::swap(m_bits_number, other.m_bits_number);
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::allocator_type dynamic_bitset<
  Block,
  Allocator>::get_allocator() const
{
	return m_blocks.get_allocator();
}

template<typename Block, typename Allocator>
template<typename _CharT, typename _Traits, typename _Alloc>
constexpr std::basic_string<_CharT, _Traits, _Alloc> dynamic_bitset<Block, Allocator>::to_string(
  _CharT zero,
  _CharT one) const
{
	const size_type len = size();
	std::basic_string<_CharT, _Traits, _Alloc> str(len, zero);
	for(size_type i_block = 0; i_block < m_blocks.size(); ++i_block)
	{
		if(m_blocks[i_block] == zero_block)
		{
			continue;
		}
		block_type mask = block_type(1);
		const size_type limit =
		  i_block * bits_per_block < len ? len - i_block * bits_per_block : bits_per_block;
		for(size_type i_bit = 0; i_bit < limit; ++i_bit)
		{
			if((m_blocks[i_block] & mask) != zero_block)
			{
				_Traits::assign(str[len - (i_block * bits_per_block + i_bit + 1)], one);
			}
			mask <<= 1;
		}
	}
	return str;
}

template<typename Block, typename Allocator>
template<typename Function, typename... Parameters>
constexpr void dynamic_bitset<Block, Allocator>::iterate_bits_on(Function&& function,
                                                                 Parameters&&... parameters) const
{
	if constexpr(!std::is_invocable_v<Function, size_t, Parameters...>)
	{
		static_assert(dependent_false<Function>::value, "Function take invalid arguments");
		// function should take (size_t, parameters...) as arguments
	}

	if constexpr(std::is_same_v<std::invoke_result_t<Function, size_t, Parameters...>, void>)
	{
		size_t i_bit = find_first();
		while(i_bit != npos)
		{
			std::invoke(
			  std::forward<Function>(function), i_bit, std::forward<Parameters>(parameters)...);
			i_bit = find_next(i_bit);
		}
	}
	else if constexpr(std::is_convertible_v<std::invoke_result_t<Function, size_t, Parameters...>,
	                                        bool>)
	{
		size_t i_bit = find_first();
		while(i_bit != npos)
		{
			if(!std::invoke(
			     std::forward<Function>(function), i_bit, std::forward<Parameters>(parameters)...))
			{
				break;
			}
			i_bit = find_next(i_bit);
		}
	}
	else
	{
		static_assert(dependent_false<Function>::value, "Function have invalid return type");
		// return type should be void, or convertible to bool
	}
}

template<typename Block, typename Allocator>
[[nodiscard]] constexpr bool operator==(const dynamic_bitset<Block, Allocator>& lhs,
                                        const dynamic_bitset<Block, Allocator>& rhs)
{
	return (lhs.m_bits_number == rhs.m_bits_number) && (lhs.m_blocks == rhs.m_blocks);
}

template<typename Block, typename Allocator>
[[nodiscard]] constexpr bool operator<(const dynamic_bitset<Block, Allocator>& lhs,
                                       const dynamic_bitset<Block, Allocator>& rhs)
{
	using size_type = typename dynamic_bitset<Block, Allocator>::size_type;
	using block_type = typename dynamic_bitset<Block, Allocator>::block_type;
	const size_type lhs_size = lhs.size();
	const size_type rhs_size = rhs.size();
	const size_type lhs_blocks_size = lhs.m_blocks.size();
	const size_type rhs_blocks_size = rhs.m_blocks.size();

	if(lhs_size == rhs_size)
	{
		// if comparison of two empty bitsets
		if(lhs_size == 0)
		{
			return false;
		}

		for(size_type i = lhs_blocks_size - 1; i > 0; --i)
		{
			if(lhs.m_blocks[i] != rhs.m_blocks[i])
			{
				return lhs.m_blocks[i] < rhs.m_blocks[i];
			}
		}
		return lhs.m_blocks[0] < rhs.m_blocks[0];
	}

	// empty bitset inferior to 0-only bitset
	if(lhs_size == 0)
	{
		return true;
	}
	if(rhs_size == 0)
	{
		return false;
	}

	const bool rhs_longer = rhs_size > lhs_size;
	const dynamic_bitset<Block, Allocator>& longest_bitset = rhs_longer ? rhs : lhs;
	const size_type longest_blocks_size = std::max(lhs_blocks_size, rhs_blocks_size);
	const size_type shortest_blocks_size = std::min(lhs_blocks_size, rhs_blocks_size);
	for(size_type i = longest_blocks_size - 1; i >= shortest_blocks_size; --i)
	{
		if(longest_bitset.m_blocks[i] != block_type(0))
		{
			return rhs_longer;
		}
	}

	for(size_type i = shortest_blocks_size - 1; i > 0; --i)
	{
		if(lhs.m_blocks[i] != rhs.m_blocks[i])
		{
			return lhs.m_blocks[i] < rhs.m_blocks[i];
		}
	}
	if(lhs.m_blocks[0] != rhs.m_blocks[0])
	{
		return lhs.m_blocks[0] < rhs.m_blocks[0];
	}
	return lhs_size < rhs_size;
}

//=================================================================================================
// dynamic_bitset private functions implementations
//=================================================================================================

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  blocks_required(size_type nbits) noexcept
{
	return nbits / bits_per_block + static_cast<size_type>(nbits % bits_per_block > 0);
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  block_index(size_type pos) noexcept
{
	return pos / bits_per_block;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  bit_index(size_type pos) noexcept
{
	return pos % bits_per_block;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::block_type dynamic_bitset<Block, Allocator>::
  bit_mask(size_type pos) noexcept
{
	return block_type(block_type(1) << bit_index(pos));
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::block_type dynamic_bitset<Block, Allocator>::
  bit_mask(size_type first, size_type last) noexcept
{
	first = bit_index(first);
	last = bit_index(last);
	if(last == (block_last_bit_index))
	{
		return block_type(one_block << first);
	}
	else
	{
		return block_type(((block_type(1) << (last + 1)) - 1) ^ ((block_type(1) << first) - 1));
	}
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::set_block_bits(block_type& block,
                                                                size_type first,
                                                                size_type last,
                                                                bool val) noexcept
{
	if(val)
	{
		block |= bit_mask(first, last);
	}
	else
	{
		block &= ~bit_mask(first, last);
	}
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::flip_block_bits(block_type& block,
                                                                 size_type first,
                                                                 size_type last) noexcept
{
	block ^= bit_mask(first, last);
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  block_count(const block_type& block) noexcept
{
	if(block == zero_block)
	{
		return 0;
	}

#if defined(DYNAMIC_BITSET_GCC) \
  || (defined(DYNAMIC_BITSET_CLANG) && defined(DYNAMIC_BITSET_CLANG_builtin_popcount))
	if constexpr(std::is_same_v<block_type, unsigned long long>)
	{
		return static_cast<size_type>(__builtin_popcountll(block));
	}
	if constexpr(std::is_same_v<block_type, unsigned long>)
	{
		return static_cast<size_type>(__builtin_popcountl(block));
	}
	if constexpr(sizeof(block_type) <= sizeof(unsigned int))
	{
		return static_cast<size_type>(__builtin_popcount(static_cast<unsigned int>(block)));
	}
#endif

	size_type count = 0;
	block_type mask = 1;
	for(size_type bit_index = 0; bit_index < bits_per_block; ++bit_index)
	{
		count += ((block & mask) != zero_block);
		mask <<= 1;
	}
	return count;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  block_count(const block_type& block, size_type nbits) noexcept
{
	assert(nbits <= bits_per_block);
	if(block == zero_block)
	{
		return 0;
	}

#if defined(DYNAMIC_BITSET_GCC) \
  || (defined(DYNAMIC_BITSET_CLANG) && defined(DYNAMIC_BITSET_CLANG_builtin_popcount))
	const block_type shifted_block = block_type(block << (bits_per_block - nbits));
	if constexpr(std::is_same_v<block_type, unsigned long long>)
	{
		return static_cast<size_type>(__builtin_popcountll(shifted_block));
	}
	if constexpr(std::is_same_v<block_type, unsigned long>)
	{
		return static_cast<size_type>(__builtin_popcountl(shifted_block));
	}
	if constexpr(sizeof(block_type) <= sizeof(unsigned int))
	{
		return static_cast<size_type>(__builtin_popcount(static_cast<unsigned int>(shifted_block)));
	}
#endif

	size_type count = 0;
	block_type mask = 1;
	for(size_type bit_index = 0; bit_index < nbits; ++bit_index)
	{
		count += ((block & mask) != zero_block);
		mask <<= 1;
	}

	return count;
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  first_on(const block_type& block) noexcept
{
	assert(block != zero_block);

#if defined(DYNAMIC_BITSET_GCC) \
  || (defined(DYNAMIC_BITSET_CLANG) && defined(DYNAMIC_BITSET_CLANG_builtin_ctz))
	if constexpr(std::is_same_v<block_type, unsigned long long>)
	{
		return static_cast<size_type>(__builtin_ctzll(block));
	}
	if constexpr(std::is_same_v<block_type, unsigned long>)
	{
		return static_cast<size_type>(__builtin_ctzl(block));
	}
	if constexpr(sizeof(block_type) <= sizeof(unsigned int))
	{
		return static_cast<size_type>(__builtin_ctz(static_cast<unsigned int>(block)));
	}
#elif defined(DYNAMIC_BITSET_MSVC)
#	if defined(DYNAMIC_BITSET_MSVC_64)
	if constexpr(std::is_same_v<block_type, unsigned __int64>)
	{
		unsigned long index = std::numeric_limits<unsigned long>::max();
		_BitScanForward64(&index, block);
		return static_cast<size_type>(index);
	}
#	endif
	if constexpr(std::is_same_v<block_type, unsigned long>)
	{
		unsigned long index = std::numeric_limits<unsigned long>::max();
		_BitScanForward(&index, block);
		return static_cast<size_type>(index);
	}
	if constexpr(sizeof(block_type) <= sizeof(unsigned long))
	{
		unsigned long index = std::numeric_limits<unsigned long>::max();
		_BitScanForward(&index, static_cast<unsigned long>(block));
		return static_cast<size_type>(index);
	}
#endif

	block_type mask = block_type(1);
	for(size_type i = 0; i < bits_per_block; ++i)
	{
		if((block & mask) != zero_block)
		{
			return i;
		}
		mask <<= 1;
	}
	return npos;
}

template<typename Block, typename Allocator>
template<typename _CharT, typename _Traits>
constexpr void dynamic_bitset<Block, Allocator>::init_from_string(
  std::basic_string_view<_CharT, _Traits> str,
  typename std::basic_string_view<_CharT, _Traits>::size_type pos,
  typename std::basic_string_view<_CharT, _Traits>::size_type n,
  [[maybe_unused]] _CharT zero,
  _CharT one)
{
	assert(pos < str.size());

	const size_type size = std::min(n, str.size() - pos);
	m_bits_number = size;

	m_blocks.clear();
	m_blocks.resize(blocks_required(size));
	for(size_t i = 0; i < size; ++i)
	{
		const _CharT c = str[(pos + size - 1) - i];
		assert(c == zero || c == one);
		if(c == one)
		{
			set(i);
		}
	}
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::block_type& dynamic_bitset<Block, Allocator>::
  get_block(size_type pos)
{
	return m_blocks[block_index(pos)];
}

template<typename Block, typename Allocator>
constexpr const typename dynamic_bitset<Block, Allocator>::block_type& dynamic_bitset<
  Block,
  Allocator>::get_block(size_type pos) const
{
	return m_blocks[block_index(pos)];
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::block_type& dynamic_bitset<Block, Allocator>::
  last_block()
{
	return m_blocks[m_blocks.size() - 1];
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::block_type dynamic_bitset<Block, Allocator>::
  last_block() const
{
	return m_blocks[m_blocks.size() - 1];
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  extra_bits_number() const noexcept
{
	return bit_index(m_bits_number);
}

template<typename Block, typename Allocator>
constexpr typename dynamic_bitset<Block, Allocator>::size_type dynamic_bitset<Block, Allocator>::
  unused_bits_number() const noexcept
{
	return bits_per_block - extra_bits_number();
}

template<typename Block, typename Allocator>
template<typename BinaryOperation>
constexpr void dynamic_bitset<Block, Allocator>::apply(
  const dynamic_bitset<Block, Allocator>& other,
  BinaryOperation binary_op)
{
	assert(num_blocks() == other.num_blocks());
	std::transform(std::cbegin(m_blocks),
	               std::cend(m_blocks),
	               std::cbegin(other.m_blocks),
	               std::begin(m_blocks),
	               binary_op);
}

template<typename Block, typename Allocator>
template<typename UnaryOperation>
constexpr void dynamic_bitset<Block, Allocator>::apply(UnaryOperation unary_op)
{
	std::transform(std::cbegin(m_blocks), std::cend(m_blocks), std::begin(m_blocks), unary_op);
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::apply_left_shift(size_type shift)
{
	assert(shift > 0);
	assert(shift < capacity());

	const size_type blocks_shift = shift / bits_per_block;
	const size_type bits_offset = shift % bits_per_block;

	if(bits_offset == 0)
	{
		for(size_type i = m_blocks.size() - 1; i >= blocks_shift; --i)
		{
			m_blocks[i] = m_blocks[i - blocks_shift];
		}
	}
	else
	{
		const size_type reverse_bits_offset = bits_per_block - bits_offset;
		for(size_type i = m_blocks.size() - 1; i > blocks_shift; --i)
		{
			m_blocks[i] =
			  block_type((m_blocks[i - blocks_shift] << bits_offset)
			             | block_type(m_blocks[i - blocks_shift - 1] >> reverse_bits_offset));
		}
		m_blocks[blocks_shift] = block_type(m_blocks[0] << bits_offset);
	}

	// set bit that came at the right to 0 in unmodified blocks
	std::fill(std::begin(m_blocks),
	          std::begin(m_blocks)
	            + static_cast<typename decltype(m_blocks)::difference_type>(blocks_shift),
	          zero_block);
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::apply_right_shift(size_type shift)
{
	assert(shift > 0);
	assert(shift < capacity());

	const size_type blocks_shift = shift / bits_per_block;
	const size_type bits_offset = shift % bits_per_block;
	const size_type last_block_to_shift = m_blocks.size() - blocks_shift - 1;

	if(bits_offset == 0)
	{
		for(size_type i = 0; i <= last_block_to_shift; ++i)
		{
			m_blocks[i] = m_blocks[i + blocks_shift];
		}
	}
	else
	{
		const size_type reverse_bits_offset = bits_per_block - bits_offset;
		for(size_type i = 0; i < last_block_to_shift; ++i)
		{
			m_blocks[i] =
			  block_type((m_blocks[i + blocks_shift] >> bits_offset)
			             | block_type(m_blocks[i + blocks_shift + 1] << reverse_bits_offset));
		}
		m_blocks[last_block_to_shift] = block_type(m_blocks[m_blocks.size() - 1] >> bits_offset);
	}

	// set bit that came at the left to 0 in unmodified blocks
	std::fill(
	  std::begin(m_blocks)
	    + static_cast<typename decltype(m_blocks)::difference_type>(last_block_to_shift + 1),
	  std::end(m_blocks),
	  zero_block);
}

template<typename Block, typename Allocator>
constexpr void dynamic_bitset<Block, Allocator>::sanitize()
{
	size_type shift = m_bits_number % bits_per_block;
	if(shift > 0)
	{
		last_block() &= ~(one_block << shift);
	}
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::check_unused_bits() const noexcept
{
	const size_type extra_bits = extra_bits_number();
	if(extra_bits > 0)
	{
		return (last_block() & (one_block << extra_bits)) == zero_block;
	}
	return true;
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::check_size() const noexcept
{
	return blocks_required(size()) == m_blocks.size();
}

template<typename Block, typename Allocator>
constexpr bool dynamic_bitset<Block, Allocator>::check_consistency() const noexcept
{
	return check_unused_bits() && check_size();
}

//=================================================================================================
// dynamic_bitset external functions implementations
//=================================================================================================

template<typename Block, typename Allocator>
constexpr bool operator!=(const dynamic_bitset<Block, Allocator>& lhs,
                          const dynamic_bitset<Block, Allocator>& rhs)
{
	return !(lhs == rhs);
}

template<typename Block, typename Allocator>
constexpr bool operator<=(const dynamic_bitset<Block, Allocator>& lhs,
                          const dynamic_bitset<Block, Allocator>& rhs)
{
	return !(rhs < lhs);
}

template<typename Block, typename Allocator>
constexpr bool operator>(const dynamic_bitset<Block, Allocator>& lhs,
                         const dynamic_bitset<Block, Allocator>& rhs)
{
	return rhs < lhs;
}

template<typename Block, typename Allocator>
constexpr bool operator>=(const dynamic_bitset<Block, Allocator>& lhs,
                          const dynamic_bitset<Block, Allocator>& rhs)
{
	return !(lhs < rhs);
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator&(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs)
{
	dynamic_bitset<Block, Allocator> result(lhs);
	return result &= rhs;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator|(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs)
{
	dynamic_bitset<Block, Allocator> result(lhs);
	return result |= rhs;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator^(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs)
{
	dynamic_bitset<Block, Allocator> result(lhs);
	return result ^= rhs;
}

template<typename Block, typename Allocator>
constexpr dynamic_bitset<Block, Allocator> operator-(const dynamic_bitset<Block, Allocator>& lhs,
                                                     const dynamic_bitset<Block, Allocator>& rhs)
{
	dynamic_bitset<Block, Allocator> result(lhs);
	return result -= rhs;
}

template<typename _CharT, typename _Traits, typename Block, typename Allocator>
constexpr std::basic_ostream<_CharT, _Traits>& operator<<(
  std::basic_ostream<_CharT, _Traits>& os,
  const dynamic_bitset<Block, Allocator>& bitset)
{
	// A better implementation is possible
	return os << bitset.template to_string<_CharT, _Traits>();
}

template<typename _CharT, typename _Traits, typename Block, typename Allocator>
constexpr std::basic_istream<_CharT, _Traits>& operator>>(std::basic_istream<_CharT, _Traits>& is,
                                                          dynamic_bitset<Block, Allocator>& bitset)
{
	// A better implementation is possible
	constexpr _CharT zero = _CharT('0');
	constexpr _CharT one = _CharT('1');
	typename std::basic_istream<_CharT, _Traits>::sentry s(is);
	if(!s)
	{
		return is;
	}

	dynamic_bitset<Block, Allocator> reverse_bitset;
	_CharT val;
	is.get(val);
	while(is.good())
	{
		if(val == one)
		{
			reverse_bitset.push_back(true);
		}
		else if(val == zero)
		{
			reverse_bitset.push_back(false);
		}
		else
		{
			is.unget();
			break;
		}
		is.get(val);
	}

	bitset.clear();
	if(!reverse_bitset.empty())
	{
		for(typename dynamic_bitset<Block, Allocator>::size_type i = reverse_bitset.size() - 1;
		    i > 0;
		    --i)
		{
			bitset.push_back(reverse_bitset.test(i));
		}
		bitset.push_back(reverse_bitset.test(0));
	}

	return is;
}

template<typename Block, typename Allocator>
constexpr void swap(dynamic_bitset<Block, Allocator>& bitset1,
                    dynamic_bitset<Block, Allocator>& bitset2)
{
	bitset1.swap(bitset2);
}

#endif //DYNAMIC_BITSET_DYNAMIC_BITSET_HPP
