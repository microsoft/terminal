/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DbcsAttribute.hpp

Abstract:
- helper class for storing double byte character set information about a cell in the output buffer.

Author(s):
- Austin Diviness (AustDi) 26-Jan-2018

Revision History:
--*/

#pragma once

class DbcsAttribute final
{
public:
    enum class Attribute : BYTE
    {
        Single = 0x00,
        Leading = 0x01,
        Trailing = 0x02
    };

    DbcsAttribute() noexcept :
        _attribute{ Attribute::Single },
        _glyphStored{ false }
    {
    }

    DbcsAttribute(const Attribute attribute) noexcept :
        _attribute{ attribute },
        _glyphStored{ false }
    {
    }

    constexpr bool IsSingle() const noexcept
    {
        return _attribute == Attribute::Single;
    }

    constexpr bool IsLeading() const noexcept
    {
        return _attribute == Attribute::Leading;
    }

    constexpr bool IsTrailing() const noexcept
    {
        return _attribute == Attribute::Trailing;
    }

    constexpr bool IsDbcs() const noexcept
    {
        return IsLeading() || IsTrailing();
    }

    constexpr bool IsGlyphStored() const noexcept
    {
        return _glyphStored;
    }

    void SetGlyphStored(const bool stored) noexcept
    {
        _glyphStored = stored;
    }

    void SetSingle() noexcept
    {
        _attribute = Attribute::Single;
    }

    void SetLeading() noexcept
    {
        _attribute = Attribute::Leading;
    }

    void SetTrailing() noexcept
    {
        _attribute = Attribute::Trailing;
    }

    void Reset() noexcept
    {
        SetSingle();
        SetGlyphStored(false);
    }

    WORD GeneratePublicApiAttributeFormat() const noexcept
    {
        WORD publicAttribute = 0;
        if (IsLeading())
        {
            WI_SetFlag(publicAttribute, COMMON_LVB_LEADING_BYTE);
        }
        if (IsTrailing())
        {
            WI_SetFlag(publicAttribute, COMMON_LVB_TRAILING_BYTE);
        }
        return publicAttribute;
    }

    static DbcsAttribute FromPublicApiAttributeFormat(WORD publicAttribute)
    {
        // it's not valid to be both a leading and trailing byte
        if (WI_AreAllFlagsSet(publicAttribute, COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE))
        {
            THROW_HR(E_INVALIDARG);
        }

        DbcsAttribute attr;
        if (WI_IsFlagSet(publicAttribute, COMMON_LVB_LEADING_BYTE))
        {
            attr.SetLeading();
        }
        else if (WI_IsFlagSet(publicAttribute, COMMON_LVB_TRAILING_BYTE))
        {
            attr.SetTrailing();
        }
        return attr;
    }

    friend constexpr bool operator==(const DbcsAttribute& a, const DbcsAttribute& b) noexcept;

private:
    Attribute _attribute : 2;
    bool _glyphStored : 1;

#ifdef UNIT_TESTING
    friend class TextBufferTests;
#endif
};

constexpr bool operator==(const DbcsAttribute& a, const DbcsAttribute& b) noexcept
{
    return a._attribute == b._attribute;
}

static_assert(sizeof(DbcsAttribute) == sizeof(BYTE), "DbcsAttribute should be one byte big. if this changes then it needs "
                                                     "either an implicit conversion to a BYTE or an update to all places "
                                                     "that assume it's a byte big");
