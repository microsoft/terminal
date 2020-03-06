// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef UNIT_TESTING
class SizeTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    class size
    {
    public:
        constexpr size() noexcept :
            size(0, 0)
        {

        }

        constexpr size(int width, int height) noexcept :
            size(static_cast<ptrdiff_t>(width), static_cast<ptrdiff_t>(height))
        {

        }

        size(size_t width, size_t height)
        {
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(width).AssignIfValid(&_width));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(height).AssignIfValid(&_height));
        }

        constexpr size(ptrdiff_t width, ptrdiff_t height) noexcept :
            _width(width),
            _height(height)
        {

        }

        template<typename TOther>
        constexpr size(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().X)> && std::is_integral_v<decltype(std::declval<TOther>().Y)>, int> /*sentinel*/ = 0) :
            size(static_cast<ptrdiff_t>(other.X), static_cast<ptrdiff_t>(other.Y))
        {

        }

        template<typename TOther>
        constexpr size(const TOther& other, std::enable_if_t<std::is_integral_v<decltype(std::declval<TOther>().cx)> && std::is_integral_v<decltype(std::declval<TOther>().cy)>, int> /*sentinel*/ = 0) :
            size(static_cast<ptrdiff_t>(other.cx), static_cast<ptrdiff_t>(other.cy))
        {

        }

        constexpr bool operator==(const size& other) const noexcept
        {
            return _width == other._width &&
                _height == other._height;
        }

        constexpr bool operator!=(const size& other) const noexcept
        {
            return !(*this == other);
        }

        constexpr ptrdiff_t width() const noexcept
        {
            return _width;
        }

        template<typename T>
        T width() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(width()).AssignIfValid(&ret));
            return ret;
        }

        constexpr ptrdiff_t height() const noexcept
        {
            return _height;
        }

        template<typename T>
        T height() const
        {
            T ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(height()).AssignIfValid(&ret));
            return ret;
        }

        size operator*(const float& other) const
        {
            ptrdiff_t w;
            THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(width()) * other).AssignIfValid(&w));

            ptrdiff_t h;
            THROW_HR_IF(E_ABORT, !(base::MakeCheckedNum(height()) * other).AssignIfValid(&h));

            return size{ w, h };
        }

        ptrdiff_t area() const 
        {
            ptrdiff_t result;
            THROW_HR_IF(E_ABORT, !base::CheckMul(_width, _height).AssignIfValid(&result));
            return result;
        }

#ifdef _WINCONTYPES_
        operator COORD() const
        {
            COORD ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(_width).AssignIfValid(&ret.X));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(_height).AssignIfValid(&ret.Y));
            return ret;
        }
#endif

#ifdef _WINDEF_
        operator SIZE() const
        {
            SIZE ret;
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(_width).AssignIfValid(&ret.cx));
            THROW_HR_IF(E_ABORT, !base::MakeCheckedNum(_height).AssignIfValid(&ret.cy));
            return ret;
        }
#endif

#ifdef DCOMMON_H_INCLUDED
        constexpr operator D2D1_SIZE_F() const noexcept
        {
            return D2D1_SIZE_F{ gsl::narrow_cast<float>(_width), gsl::narrow_cast<float>(_height) };
        }
#endif

    protected:
        ptrdiff_t _width;
        ptrdiff_t _height;

#ifdef UNIT_TESTING
        friend class ::SizeTests;
#endif
    };
};

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<>
    class VerifyOutputTraits<::til::size>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::size& size)
        {
            return WEX::Common::NoThrowString().Format(L"[W:%td, H:%td]", size.width(), size.height());
        }
    };

    template<>
    class VerifyCompareTraits<::til::size, ::til::size>
    {
    public:
        static bool AreEqual(const ::til::size& expected, const ::til::size& actual)
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::size& expected, const ::til::size& actual)
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::size& expectedLess, const ::til::size& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::size& expectedGreater, const ::til::size& expectedLess) = delete;

        static bool IsNull(const ::til::size& object)
        {
            return object == til::size{};
        }
    };
};
#endif
