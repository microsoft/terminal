// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BuiltinGlyphs.h"

// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26472) // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narrow (type.1).
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;
using namespace Microsoft::Console::Render::Atlas::BuiltinGlyphs;

union Instruction
{
    struct
    {
        u32 shape : 4; // Shape enum
        u32 begX : 5; // Pos enum
        u32 begY : 5; // Pos enum
        u32 endX : 5; // Pos enum
        u32 endY : 5; // Pos enum
    };
    u32 value = 0;
};
static_assert(sizeof(Instruction) == sizeof(u32));

static constexpr u32 InstructionsPerGlyph = 4;

enum Shape : u32
{
    Shape_Filled025, // axis aligned rectangle, 25% filled
    Shape_Filled050, // axis aligned rectangle, 50% filled
    Shape_Filled075, // axis aligned rectangle, 75% filled
    Shape_Filled100, // axis aligned rectangle, 100% filled
    Shape_LightLine, // 1/8th wide line
    Shape_HeavyLine, // 1/4th wide line
    Shape_EmptyRect, // axis aligned hollow rectangle
    Shape_RoundRect, // axis aligned hollow, rounded rectangle
    Shape_FilledEllipsis, // axis aligned, filled ellipsis
    Shape_EmptyEllipsis, // axis aligned, hollow ellipsis
    Shape_ClosedFilledPath, // filled path, the last segment connects to the first; set endX==Pos_Min to ignore
    Shape_OpenLinePath, // regular line path; Pos_Min positions are ignored
};

// Pos indicates a fraction between 0 and 1 and is used as a UV coordinate with a cell.
// (0,0) is in the top-left corner. Some enum entries also contain a suffix.
// This suffix indicates an offset of that many times the line width, to be added to the position.
// This allows us to store 2 floats in just 5 bits and helps with keeping the Instruction tables compact.
enum Pos : u32
{
    Pos_Min,
    Pos_Max,

    Pos_0_1,
    Pos_0_1_Add_0_5,
    Pos_1_1,
    Pos_1_1_Sub_0_5,

    Pos_1_2,
    Pos_1_2_Sub_0_5,
    Pos_1_2_Add_0_5,
    Pos_1_2_Sub_1,
    Pos_1_2_Add_1,

    Pos_1_4,
    Pos_3_4,

    Pos_2_6,
    Pos_3_6,
    Pos_5_6,

    Pos_1_8,
    Pos_3_8,
    Pos_5_8,
    Pos_7_8,

    Pos_2_9,
    Pos_3_9,
    Pos_5_9,
    Pos_6_9,
    Pos_8_9,

    Pos_2_12,
    Pos_3_12,
    Pos_5_12,
    Pos_6_12,
    Pos_8_12,
    Pos_9_12,
    Pos_11_12,
};

inline constexpr f32 Pos_Lut[][2] = {
    /* Pos_Min         */ { -0.5f, 0.0f },
    /* Pos_Max         */ { 1.5f, 0.0f },

    /* Pos_0_1         */ { 0.0f, 0.0f },
    /* Pos_0_1_Add_0_5 */ { 0.0f, 0.5f },
    /* Pos_1_1         */ { 1.0f, 0.0f },
    /* Pos_1_1_Sub_0_5 */ { 1.0f, -0.5f },

    /* Pos_1_2         */ { 1.0f / 2.0f, 0.0f },
    /* Pos_1_2_Sub_0_5 */ { 1.0f / 2.0f, -0.5f },
    /* Pos_1_2_Add_0_5 */ { 1.0f / 2.0f, 0.5f },
    /* Pos_1_2_Sub_1   */ { 1.0f / 2.0f, -1.0f },
    /* Pos_1_2_Add_1   */ { 1.0f / 2.0f, 1.0f },

    /* Pos_1_4         */ { 1.0f / 4.0f, 0.0f },
    /* Pos_3_4         */ { 3.0f / 4.0f, 0.0f },

    /* Pos_2_6         */ { 2.0f / 6.0f, 0.0f },
    /* Pos_3_6         */ { 3.0f / 6.0f, 0.0f },
    /* Pos_5_6         */ { 5.0f / 6.0f, 0.0f },

    /* Pos_1_8         */ { 1.0f / 8.0f, 0.0f },
    /* Pos_3_8         */ { 3.0f / 8.0f, 0.0f },
    /* Pos_5_8         */ { 5.0f / 8.0f, 0.0f },
    /* Pos_7_8         */ { 7.0f / 8.0f, 0.0f },

    /* Pos_2_9         */ { 2.0f / 9.0f, 0.0f },
    /* Pos_3_9         */ { 3.0f / 9.0f, 0.0f },
    /* Pos_5_9         */ { 5.0f / 9.0f, 0.0f },
    /* Pos_6_9         */ { 6.0f / 9.0f, 0.0f },
    /* Pos_8_9         */ { 8.0f / 9.0f, 0.0f },

    /* Pos_2_12        */ { 2.0f / 12.0f, 0.0f },
    /* Pos_3_12        */ { 3.0f / 12.0f, 0.0f },
    /* Pos_5_12        */ { 5.0f / 12.0f, 0.0f },
    /* Pos_6_12        */ { 6.0f / 12.0f, 0.0f },
    /* Pos_8_12        */ { 8.0f / 12.0f, 0.0f },
    /* Pos_9_12        */ { 9.0f / 12.0f, 0.0f },
    /* Pos_11_12       */ { 11.0f / 12.0f, 0.0f },
};

static constexpr char32_t BoxDrawing_FirstChar = 0x2500;
static constexpr u32 BoxDrawing_CharCount = 0xA0;
static constexpr Instruction BoxDrawing[BoxDrawing_CharCount][InstructionsPerGlyph] = {
    // U+2500 ─ BOX DRAWINGS LIGHT HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+2501 ━ BOX DRAWINGS HEAVY HORIZONTAL
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+2502 │ BOX DRAWINGS LIGHT VERTICAL
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2503 ┃ BOX DRAWINGS HEAVY VERTICAL
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2504 ┄ BOX DRAWINGS LIGHT TRIPLE DASH HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_2_9, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_3_9, Pos_1_2, Pos_5_9, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_6_9, Pos_1_2, Pos_8_9, Pos_1_2 },
    },
    // U+2505 ┅ BOX DRAWINGS HEAVY TRIPLE DASH HORIZONTAL
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_2_9, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_3_9, Pos_1_2, Pos_5_9, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_6_9, Pos_1_2, Pos_8_9, Pos_1_2 },
    },
    // U+2506 ┆ BOX DRAWINGS LIGHT TRIPLE DASH VERTICAL
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_2_9 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_3_9, Pos_1_2, Pos_5_9 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_6_9, Pos_1_2, Pos_8_9 },
    },
    // U+2507 ┇ BOX DRAWINGS HEAVY TRIPLE DASH VERTICAL
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_2_9 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_3_9, Pos_1_2, Pos_5_9 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_6_9, Pos_1_2, Pos_8_9 },
    },
    // U+2508 ┈ BOX DRAWINGS LIGHT QUADRUPLE DASH HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_2_12, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_3_12, Pos_1_2, Pos_5_12, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_6_12, Pos_1_2, Pos_8_12, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_9_12, Pos_1_2, Pos_11_12, Pos_1_2 },
    },
    // U+2509 ┉ BOX DRAWINGS HEAVY QUADRUPLE DASH HORIZONTAL
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_2_12, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_3_12, Pos_1_2, Pos_5_12, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_6_12, Pos_1_2, Pos_8_12, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_9_12, Pos_1_2, Pos_11_12, Pos_1_2 },
    },
    // U+250A ┊ BOX DRAWINGS LIGHT QUADRUPLE DASH VERTICAL
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_2_12 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_3_12, Pos_1_2, Pos_5_12 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_6_12, Pos_1_2, Pos_8_12 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_9_12, Pos_1_2, Pos_11_12 },
    },
    // U+250B ┋ BOX DRAWINGS HEAVY QUADRUPLE DASH VERTICAL
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_2_12 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_3_12, Pos_1_2, Pos_5_12 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_6_12, Pos_1_2, Pos_8_12 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_9_12, Pos_1_2, Pos_11_12 },
    },
    // U+250C ┌ BOX DRAWINGS LIGHT DOWN AND RIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_0_5, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+250D ┍ BOX DRAWINGS DOWN LIGHT AND RIGHT HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_0_5, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+250E ┎ BOX DRAWINGS DOWN HEAVY AND RIGHT LIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+250F ┏ BOX DRAWINGS HEAVY DOWN AND RIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2510 ┐ BOX DRAWINGS LIGHT DOWN AND LEFT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_0_5, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2511 ┑ BOX DRAWINGS DOWN LIGHT AND LEFT HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_0_5, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2512 ┒ BOX DRAWINGS DOWN HEAVY AND LEFT LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2513 ┓ BOX DRAWINGS HEAVY DOWN AND LEFT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2514 └ BOX DRAWINGS LIGHT UP AND RIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_0_5, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2515 ┕ BOX DRAWINGS UP LIGHT AND RIGHT HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_0_5, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2516 ┖ BOX DRAWINGS UP HEAVY AND RIGHT LIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2517 ┗ BOX DRAWINGS HEAVY UP AND RIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2518 ┘ BOX DRAWINGS LIGHT UP AND LEFT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_0_5, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2519 ┙ BOX DRAWINGS UP LIGHT AND LEFT HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_0_5, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+251A ┚ BOX DRAWINGS UP HEAVY AND LEFT LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+251B ┛ BOX DRAWINGS HEAVY UP AND LEFT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+251C ├ BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+251D ┝ BOX DRAWINGS VERTICAL LIGHT AND RIGHT HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+251E ┞ BOX DRAWINGS UP HEAVY AND RIGHT DOWN LIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+251F ┟ BOX DRAWINGS DOWN HEAVY AND RIGHT UP LIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2520 ┠ BOX DRAWINGS VERTICAL HEAVY AND RIGHT LIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2521 ┡ BOX DRAWINGS DOWN LIGHT AND RIGHT UP HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2522 ┢ BOX DRAWINGS UP LIGHT AND RIGHT DOWN HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2523 ┣ BOX DRAWINGS HEAVY VERTICAL AND RIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2524 ┤ BOX DRAWINGS LIGHT VERTICAL AND LEFT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2525 ┥ BOX DRAWINGS VERTICAL LIGHT AND LEFT HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2526 ┦ BOX DRAWINGS UP HEAVY AND LEFT DOWN LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2527 ┧ BOX DRAWINGS DOWN HEAVY AND LEFT UP LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2528 ┨ BOX DRAWINGS VERTICAL HEAVY AND LEFT LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2529 ┩ BOX DRAWINGS DOWN LIGHT AND LEFT UP HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+252A ┪ BOX DRAWINGS UP LIGHT AND LEFT DOWN HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+252B ┫ BOX DRAWINGS HEAVY VERTICAL AND LEFT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+252C ┬ BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+252D ┭ BOX DRAWINGS LEFT HEAVY AND RIGHT DOWN LIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_0_5, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+252E ┮ BOX DRAWINGS RIGHT HEAVY AND LEFT DOWN LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_0_5, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+252F ┯ BOX DRAWINGS DOWN LIGHT AND HORIZONTAL HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2530 ┰ BOX DRAWINGS DOWN HEAVY AND HORIZONTAL LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2531 ┱ BOX DRAWINGS RIGHT LIGHT AND LEFT DOWN HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2532 ┲ BOX DRAWINGS LEFT LIGHT AND RIGHT DOWN HEAVY
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2533 ┳ BOX DRAWINGS HEAVY DOWN AND HORIZONTAL
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2534 ┴ BOX DRAWINGS LIGHT UP AND HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2535 ┵ BOX DRAWINGS LEFT HEAVY AND RIGHT UP LIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_0_5, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2536 ┶ BOX DRAWINGS RIGHT HEAVY AND LEFT UP LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_0_5, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2537 ┷ BOX DRAWINGS UP LIGHT AND HORIZONTAL HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2538 ┸ BOX DRAWINGS UP HEAVY AND HORIZONTAL LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2539 ┹ BOX DRAWINGS RIGHT LIGHT AND LEFT UP HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+253A ┺ BOX DRAWINGS LEFT LIGHT AND RIGHT UP HEAVY
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+253B ┻ BOX DRAWINGS HEAVY UP AND HORIZONTAL
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+253C ┼ BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+253D ┽ BOX DRAWINGS LEFT HEAVY AND RIGHT VERTICAL LIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+253E ┾ BOX DRAWINGS RIGHT HEAVY AND LEFT VERTICAL LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+253F ┿ BOX DRAWINGS VERTICAL LIGHT AND HORIZONTAL HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2540 ╀ BOX DRAWINGS UP HEAVY AND DOWN HORIZONTAL LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2541 ╁ BOX DRAWINGS DOWN HEAVY AND UP HORIZONTAL LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2542 ╂ BOX DRAWINGS VERTICAL HEAVY AND HORIZONTAL LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2543 ╃ BOX DRAWINGS LEFT UP HEAVY AND RIGHT DOWN LIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2544 ╄ BOX DRAWINGS RIGHT UP HEAVY AND LEFT DOWN LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2545 ╅ BOX DRAWINGS LEFT DOWN HEAVY AND RIGHT UP LIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2546 ╆ BOX DRAWINGS RIGHT DOWN HEAVY AND LEFT UP LIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2547 ╇ BOX DRAWINGS DOWN LIGHT AND UP HORIZONTAL HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2548 ╈ BOX DRAWINGS UP LIGHT AND DOWN HORIZONTAL HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2549 ╉ BOX DRAWINGS RIGHT LIGHT AND LEFT VERTICAL HEAVY
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+254A ╊ BOX DRAWINGS LEFT LIGHT AND RIGHT VERTICAL HEAVY
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+254B ╋ BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+254C ╌ BOX DRAWINGS LIGHT DOUBLE DASH HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_2_6, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_3_6, Pos_1_2, Pos_5_6, Pos_1_2 },
    },
    // U+254D ╍ BOX DRAWINGS HEAVY DOUBLE DASH HORIZONTAL
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_2_6, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_3_6, Pos_1_2, Pos_5_6, Pos_1_2 },
    },
    // U+254E ╎ BOX DRAWINGS LIGHT DOUBLE DASH VERTICAL
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_2_6 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_3_6, Pos_1_2, Pos_5_6 },
    },
    // U+254F ╏ BOX DRAWINGS HEAVY DOUBLE DASH VERTICAL
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_2_6 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_3_6, Pos_1_2, Pos_5_6 },
    },
    // U+2550 ═ BOX DRAWINGS DOUBLE HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
    },
    // U+2551 ║ BOX DRAWINGS DOUBLE VERTICAL
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_1 },
    },
    // U+2552 ╒ BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_0_5, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Sub_0_5, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1 },
    },
    // U+2553 ╓ BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2_Sub_0_5, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_1_2_Sub_0_5, Pos_1_2_Add_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+2554 ╔ BOX DRAWINGS DOUBLE DOWN AND RIGHT
    {
        Instruction{ Shape_EmptyRect, Pos_1_2_Sub_1, Pos_1_2_Sub_1, Pos_Max, Pos_Max },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_1_2_Add_1, Pos_Max, Pos_Max },
    },
    // U+2555 ╕ BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_2_Add_0_5, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_2_Add_0_5, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1 },
    },
    // U+2556 ╖ BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2_Sub_0_5, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_1_2_Sub_0_5, Pos_1_2_Add_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
    },
    // U+2557 ╗ BOX DRAWINGS DOUBLE DOWN AND LEFT
    {
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_1_2_Sub_1, Pos_1_2_Add_1, Pos_Max },
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_1_2_Add_1, Pos_1_2_Sub_1, Pos_Max },
    },
    // U+2558 ╘ BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_0_5, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Sub_0_5, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2_Add_1 },
    },
    // U+2559 ╙ BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_2_Add_0_5 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_2_Add_0_5 },
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+255A ╚ BOX DRAWINGS DOUBLE UP AND RIGHT
    {
        Instruction{ Shape_EmptyRect, Pos_1_2_Sub_1, Pos_Min, Pos_Max, Pos_1_2_Add_1 },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_Min, Pos_Max, Pos_1_2_Sub_1 },
    },
    // U+255B ╛ BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_2_Add_0_5, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_2_Add_0_5, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2_Add_1 },
    },
    // U+255C ╜ BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_2_Add_0_5 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_2_Add_0_5 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_2 },
    },
    // U+255D ╝ BOX DRAWINGS DOUBLE UP AND LEFT
    {
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_Min, Pos_1_2_Add_1, Pos_1_2_Add_1 },
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_Min, Pos_1_2_Sub_1, Pos_1_2_Sub_1 },
    },
    // U+255E ╞ BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+255F ╟ BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+2560 ╠ BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_Min, Pos_Max, Pos_1_2_Sub_1 },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_1_2_Add_1, Pos_Max, Pos_Max },
    },
    // U+2561 ╡ BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_2, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_2, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+2562 ╢ BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2_Sub_1, Pos_1_2 },
    },
    // U+2563 ╣ BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    {
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_1 },
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_Min, Pos_1_2_Sub_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_1_2_Add_1, Pos_1_2_Sub_1, Pos_Max },
    },
    // U+2564 ╤ BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2_Add_1, Pos_1_2, Pos_1_1 },
    },
    // U+2565 ╥ BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_1_2, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_1_2, Pos_1_2_Add_1, Pos_1_1 },
    },
    // U+2566 ╦ BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_1_2_Add_1, Pos_1_2_Sub_1, Pos_Max },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_1_2_Add_1, Pos_Max, Pos_Max },
    },
    // U+2567 ╧ BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2_Sub_1 },
    },
    // U+2568 ╨ BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_2 },
    },
    // U+2569 ╩ BOX DRAWINGS DOUBLE UP AND HORIZONTAL
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_Min, Pos_1_2_Sub_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_Min, Pos_Max, Pos_1_2_Sub_1 },
    },
    // U+256A ╪ BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2_Add_1, Pos_1_1, Pos_1_2_Add_1 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+256B ╫ BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
    {
        Instruction{ Shape_LightLine, Pos_1_2_Sub_1, Pos_0_1, Pos_1_2_Sub_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_1_2_Add_1, Pos_0_1, Pos_1_2_Add_1, Pos_1_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+256C ╬ BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    {
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_Min, Pos_1_2_Sub_1, Pos_1_2_Sub_1 },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_Min, Pos_Max, Pos_1_2_Sub_1 },
        Instruction{ Shape_EmptyRect, Pos_Min, Pos_1_2_Add_1, Pos_1_2_Sub_1, Pos_Max },
        Instruction{ Shape_EmptyRect, Pos_1_2_Add_1, Pos_1_2_Add_1, Pos_Max, Pos_Max },
    },
    // U+256D ╭ BOX DRAWINGS LIGHT ARC DOWN AND RIGHT
    {
        Instruction{ Shape_RoundRect, Pos_1_2, Pos_1_2, Pos_Max, Pos_Max },
    },
    // U+256E ╮ BOX DRAWINGS LIGHT ARC DOWN AND LEFT
    {
        Instruction{ Shape_RoundRect, Pos_Min, Pos_1_2, Pos_1_2, Pos_Max },
    },
    // U+256F ╯ BOX DRAWINGS LIGHT ARC UP AND LEFT
    {
        Instruction{ Shape_RoundRect, Pos_Min, Pos_Min, Pos_1_2, Pos_1_2 },
    },
    // U+2570 ╰ BOX DRAWINGS LIGHT ARC UP AND RIGHT
    {
        Instruction{ Shape_RoundRect, Pos_1_2, Pos_Min, Pos_Max, Pos_1_2 },
    },
    // U+2571 ╱ BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_1, Pos_1_1, Pos_0_1 },
    },
    // U+2572 ╲ BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2573 ╳ BOX DRAWINGS LIGHT DIAGONAL CROSS
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_1, Pos_1_1, Pos_0_1 },
        Instruction{ Shape_LightLine, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2574 ╴ BOX DRAWINGS LIGHT LEFT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
    },
    // U+2575 ╵ BOX DRAWINGS LIGHT UP
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2576 ╶ BOX DRAWINGS LIGHT RIGHT
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+2577 ╷ BOX DRAWINGS LIGHT DOWN
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2578 ╸ BOX DRAWINGS HEAVY LEFT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
    },
    // U+2579 ╹ BOX DRAWINGS HEAVY UP
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+257A ╺ BOX DRAWINGS HEAVY RIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+257B ╻ BOX DRAWINGS HEAVY DOWN
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+257C ╼ BOX DRAWINGS LIGHT LEFT AND HEAVY RIGHT
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+257D ╽ BOX DRAWINGS LIGHT UP AND HEAVY DOWN
    {
        Instruction{ Shape_LightLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+257E ╾ BOX DRAWINGS HEAVY LEFT AND LIGHT RIGHT
    {
        Instruction{ Shape_HeavyLine, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+257F ╿ BOX DRAWINGS HEAVY UP AND LIGHT DOW
    {
        Instruction{ Shape_HeavyLine, Pos_1_2, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_LightLine, Pos_1_2, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2580 ▀ UPPER HALF BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_2 },
    },
    // U+2581 ▁ LOWER ONE EIGHTH BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_7_8, Pos_1_1, Pos_1_1 },
    },
    // U+2582 ▂ LOWER ONE QUARTER BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_3_4, Pos_1_1, Pos_1_1 },
    },
    // U+2583 ▃ LOWER THREE EIGHTHS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_5_8, Pos_1_1, Pos_1_1 },
    },
    // U+2584 ▄ LOWER HALF BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_1 },
    },
    // U+2585 ▅ LOWER FIVE EIGHTHS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_3_8, Pos_1_1, Pos_1_1 },
    },
    // U+2586 ▆ LOWER THREE QUARTERS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_1_4, Pos_1_1, Pos_1_1 },
    },
    // U+2587 ▇ LOWER SEVEN EIGHTHS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_1_8, Pos_1_1, Pos_1_1 },
    },
    // U+2588 █ FULL BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2589 ▉ LEFT SEVEN EIGHTHS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_7_8, Pos_1_1 },
    },
    // U+258A ▊ LEFT THREE QUARTERS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_3_4, Pos_1_1 },
    },
    // U+258B ▋ LEFT FIVE EIGHTHS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_5_8, Pos_1_1 },
    },
    // U+258C ▌ LEFT HALF BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_2, Pos_1_1 },
    },
    // U+258D ▍ LEFT THREE EIGHTHS BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_3_8, Pos_1_1 },
    },
    // U+258E ▎ LEFT ONE QUARTER BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_4, Pos_1_1 },
    },
    // U+258F ▏ LEFT ONE EIGHTH BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_8, Pos_1_1 },
    },
    // U+2590 ▐ RIGHT HALF BLOCK
    {
        Instruction{ Shape_Filled100, Pos_1_2, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2591 ░ LIGHT SHADE
    {
        Instruction{ Shape_Filled025, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2592 ▒ MEDIUM SHADE
    {
        Instruction{ Shape_Filled050, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2593 ▓ DARK SHADE
    {
        Instruction{ Shape_Filled075, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2594 ▔ UPPER ONE EIGHTH BLOCK
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_8 },
    },
    // U+2595 ▕ RIGHT ONE EIGHTH BLOCK
    {
        Instruction{ Shape_Filled100, Pos_7_8, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+2596 ▖ QUADRANT LOWER LEFT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_1 },
    },
    // U+2597 ▗ QUADRANT LOWER RIGHT
    {
        Instruction{ Shape_Filled100, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_1 },
    },
    // U+2598 ▘ QUADRANT UPPER LEFT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_2, Pos_1_2 },
    },
    // U+2599 ▙ QUADRANT UPPER LEFT AND LOWER LEFT AND LOWER RIGHT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_2, Pos_1_1 },
        Instruction{ Shape_Filled100, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_1 },
    },
    // U+259A ▚ QUADRANT UPPER LEFT AND LOWER RIGHT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_Filled100, Pos_1_2, Pos_1_2, Pos_1_1, Pos_1_1 },
    },
    // U+259B ▛ QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER LEFT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_2, Pos_1_1 },
        Instruction{ Shape_Filled100, Pos_1_2, Pos_0_1, Pos_1_1, Pos_1_2 },
    },
    // U+259C ▜ QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER RIGHT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_0_1, Pos_1_2, Pos_1_2 },
        Instruction{ Shape_Filled100, Pos_1_2, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+259D ▝ QUADRANT UPPER RIGHT
    {
        Instruction{ Shape_Filled100, Pos_1_2, Pos_0_1, Pos_1_1, Pos_1_2 },
    },
    // U+259E ▞ QUADRANT UPPER RIGHT AND LOWER LEFT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_1 },
        Instruction{ Shape_Filled100, Pos_1_2, Pos_0_1, Pos_1_1, Pos_1_2 },
    },
    // U+259F ▟ QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT
    {
        Instruction{ Shape_Filled100, Pos_0_1, Pos_1_2, Pos_1_2, Pos_1_1 },
        Instruction{ Shape_Filled100, Pos_1_2, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
};

static constexpr char32_t Powerline_FirstChar = 0xE0B0;
static constexpr u32 Powerline_CharCount = 0x10;
static constexpr Instruction Powerline[Powerline_CharCount][InstructionsPerGlyph] = {
    // U+E0B0 Right triangle solid
    {
        Instruction{ Shape_ClosedFilledPath, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_2 },
        Instruction{ Shape_ClosedFilledPath, Pos_0_1, Pos_1_1, Pos_Min, Pos_Min },
    },
    // U+E0B1 Right triangle line
    {
        Instruction{ Shape_OpenLinePath, Pos_0_1, Pos_0_1, Pos_1_1_Sub_0_5, Pos_1_2 },
        Instruction{ Shape_OpenLinePath, Pos_0_1, Pos_1_1, Pos_Min, Pos_Min },
    },
    // U+E0B2 Left triangle solid
    {
        Instruction{ Shape_ClosedFilledPath, Pos_1_1, Pos_0_1, Pos_0_1, Pos_1_2 },
        Instruction{ Shape_ClosedFilledPath, Pos_1_1, Pos_1_1, Pos_Min, Pos_Min },
    },
    // U+E0B3 Left triangle line
    {
        Instruction{ Shape_OpenLinePath, Pos_1_1, Pos_0_1, Pos_0_1_Add_0_5, Pos_1_2 },
        Instruction{ Shape_OpenLinePath, Pos_1_1, Pos_1_1, Pos_Min, Pos_Min },
    },
    // U+E0B4 Right semi-circle solid
    {
        Instruction{ Shape_FilledEllipsis, Pos_0_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+E0B5 Right semi-circle line
    {
        Instruction{ Shape_EmptyEllipsis, Pos_0_1, Pos_1_2, Pos_1_1_Sub_0_5, Pos_1_2_Sub_0_5 },
    },
    // U+E0B6 Left semi-circle solid
    {
        Instruction{ Shape_FilledEllipsis, Pos_1_1, Pos_1_2, Pos_1_1, Pos_1_2 },
    },
    // U+E0B7 Left semi-circle line
    {
        Instruction{ Shape_EmptyEllipsis, Pos_1_1, Pos_1_2, Pos_1_1_Sub_0_5, Pos_1_2_Sub_0_5 },
    },
    // U+E0B8 Lower left triangle
    {
        Instruction{ Shape_ClosedFilledPath, Pos_0_1, Pos_0_1, Pos_0_1, Pos_1_1 },
        Instruction{ Shape_ClosedFilledPath, Pos_1_1, Pos_1_1, Pos_Min, Pos_Min },
    },
    // U+E0B9 Backslash separator
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
    // U+E0BA Lower right triangle
    {
        Instruction{ Shape_ClosedFilledPath, Pos_0_1, Pos_1_1, Pos_1_1, Pos_1_1 },
        Instruction{ Shape_ClosedFilledPath, Pos_1_1, Pos_0_1, Pos_Min, Pos_Min },
    },
    // U+E0BB Forward slash separator
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_1, Pos_1_1, Pos_0_1 },
    },
    // U+E0BC Upper left triangle
    {
        Instruction{ Shape_ClosedFilledPath, Pos_0_1, Pos_1_1, Pos_0_1, Pos_0_1 },
        Instruction{ Shape_ClosedFilledPath, Pos_1_1, Pos_0_1, Pos_Min, Pos_Min },
    },
    // U+E0BD Forward slash separator
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_1_1, Pos_1_1, Pos_0_1 },
    },
    // U+E0BE Upper right triangle
    {
        Instruction{ Shape_ClosedFilledPath, Pos_0_1, Pos_0_1, Pos_1_1, Pos_0_1 },
        Instruction{ Shape_ClosedFilledPath, Pos_1_1, Pos_1_1, Pos_Min, Pos_Min },
    },
    // U+E0BF Backslash separator
    {
        Instruction{ Shape_LightLine, Pos_0_1, Pos_0_1, Pos_1_1, Pos_1_1 },
    },
};

constexpr bool BoxDrawing_IsMapped(char32_t codepoint)
{
    return codepoint >= BoxDrawing_FirstChar && codepoint < (BoxDrawing_FirstChar + BoxDrawing_CharCount);
}

constexpr bool Powerline_IsMapped(char32_t codepoint)
{
    return codepoint >= Powerline_FirstChar && codepoint < (Powerline_FirstChar + Powerline_CharCount);
}

// How should I make this constexpr == inline, if it's an external symbol? Bad compiler!
#pragma warning(suppress : 26497) // You can attempt to make '...' constexpr unless it contains any undefined behavior (f.4).
bool BuiltinGlyphs::IsBuiltinGlyph(char32_t codepoint) noexcept
{
    return BoxDrawing_IsMapped(codepoint) || Powerline_IsMapped(codepoint);
}

static const Instruction* GetInstructions(char32_t codepoint) noexcept
{
    if (BoxDrawing_IsMapped(codepoint))
    {
        return &BoxDrawing[codepoint - BoxDrawing_FirstChar][0];
    }
    if (Powerline_IsMapped(codepoint))
    {
        return &Powerline[codepoint - Powerline_FirstChar][0];
    }
    return nullptr;
}

void BuiltinGlyphs::DrawBuiltinGlyph(ID2D1Factory* factory, ID2D1DeviceContext* renderTarget, ID2D1SolidColorBrush* brush, const D2D1_RECT_F& rect, char32_t codepoint)
{
    renderTarget->PushAxisAlignedClip(&rect, D2D1_ANTIALIAS_MODE_ALIASED);
    const auto restoreD2D = wil::scope_exit([&]() {
        renderTarget->PopAxisAlignedClip();
    });

    const auto instructions = GetInstructions(codepoint);
    if (!instructions)
    {
        assert(false); // If everything in AtlasEngine works correctly, then this function should not get called when !IsBuiltinGlyph(codepoint).
        renderTarget->Clear(nullptr);
        return;
    }

    const auto rectX = rect.left;
    const auto rectY = rect.top;
    const auto rectW = rect.right - rect.left;
    const auto rectH = rect.bottom - rect.top;
    const auto lightLineWidth = std::max(1.0f, roundf(rectW / 8.0f));
    D2D1_POINT_2F geometryPoints[2 * InstructionsPerGlyph];
    size_t geometryPointsCount = 0;

    for (size_t i = 0; i < InstructionsPerGlyph; ++i)
    {
        const auto& instruction = instructions[i];
        if (instruction.value == 0)
        {
            break;
        }

        const auto shape = static_cast<Shape>(instruction.shape);
        auto begX = Pos_Lut[instruction.begX][0] * rectW;
        auto begY = Pos_Lut[instruction.begY][0] * rectH;
        auto endX = Pos_Lut[instruction.endX][0] * rectW;
        auto endY = Pos_Lut[instruction.endY][0] * rectH;

        begX += Pos_Lut[instruction.begX][1] * lightLineWidth;
        begY += Pos_Lut[instruction.begY][1] * lightLineWidth;
        endX += Pos_Lut[instruction.endX][1] * lightLineWidth;
        endY += Pos_Lut[instruction.endY][1] * lightLineWidth;

        const auto lineWidth = shape == Shape_HeavyLine ? lightLineWidth * 2.0f : lightLineWidth;
        const auto lineWidthHalf = lineWidth * 0.5f;
        const auto isHollowRect = shape == Shape_EmptyRect || shape == Shape_RoundRect;
        const auto isLine = shape == Shape_LightLine || shape == Shape_HeavyLine;
        const auto isLineX = isLine && begX == endX;
        const auto isLineY = isLine && begY == endY;
        const auto lineOffsetX = isHollowRect || isLineX ? lineWidthHalf : 0.0f;
        const auto lineOffsetY = isHollowRect || isLineY ? lineWidthHalf : 0.0f;

        begX = roundf(begX - lineOffsetX) + lineOffsetX;
        begY = roundf(begY - lineOffsetY) + lineOffsetY;
        endX = roundf(endX + lineOffsetX) - lineOffsetX;
        endY = roundf(endY + lineOffsetY) - lineOffsetY;

        const auto begXabs = begX + rectX;
        const auto begYabs = begY + rectY;
        const auto endXabs = endX + rectX;
        const auto endYabs = endY + rectY;

        switch (shape)
        {
        case Shape_Filled025:
        case Shape_Filled050:
        case Shape_Filled075:
        case Shape_Filled100:
        {
            // This code works in tandem with SHADING_TYPE_TEXT_BUILTIN_GLYPH in our pixel shader.
            // Unless someone removed it, it should have a lengthy comment visually explaining
            // what each of the 3 RGB components do. The short version is:
            //   R: stretch the checkerboard pattern (Shape_Filled050) horizontally
            //   G: invert the pixels
            //   B: overrides the above and fills it
            static constexpr D2D1_COLOR_F colors[] = {
                { 1, 0, 0, 1 }, // Shape_Filled025
                { 0, 0, 0, 1 }, // Shape_Filled050
                { 1, 1, 0, 1 }, // Shape_Filled075
                { 1, 1, 1, 1 }, // Shape_Filled100
            };

            const auto brushColor = brush->GetColor();
            brush->SetColor(&colors[shape]);

            const D2D1_RECT_F r{ begXabs, begYabs, endXabs, endYabs };
            renderTarget->FillRectangle(&r, brush);

            brush->SetColor(&brushColor);
            break;
        }
        case Shape_LightLine:
        case Shape_HeavyLine:
        {
            const D2D1_POINT_2F beg{ begXabs, begYabs };
            const D2D1_POINT_2F end{ endXabs, endYabs };
            renderTarget->DrawLine(beg, end, brush, lineWidth, nullptr);
            break;
        }
        case Shape_EmptyRect:
        {
            const D2D1_RECT_F r{ begXabs, begYabs, endXabs, endYabs };
            renderTarget->DrawRectangle(&r, brush, lineWidth, nullptr);
            break;
        }
        case Shape_RoundRect:
        {
            const D2D1_ROUNDED_RECT rr{ { begXabs, begYabs, endXabs, endYabs }, lightLineWidth * 2, lightLineWidth * 2 };
            renderTarget->DrawRoundedRectangle(&rr, brush, lineWidth, nullptr);
            break;
        }
        case Shape_FilledEllipsis:
        {
            const D2D1_ELLIPSE e{ { begXabs, begYabs }, endX, endY };
            renderTarget->FillEllipse(&e, brush);
            break;
        }
        case Shape_EmptyEllipsis:
        {
            const D2D1_ELLIPSE e{ { begXabs, begYabs }, endX, endY };
            renderTarget->DrawEllipse(&e, brush, lineWidth, nullptr);
            break;
        }
        case Shape_ClosedFilledPath:
        case Shape_OpenLinePath:
            if (instruction.begX)
            {
                geometryPoints[geometryPointsCount++] = { begXabs, begYabs };
            }
            if (instruction.endX)
            {
                geometryPoints[geometryPointsCount++] = { endXabs, endYabs };
            }
            break;
        }
    }

    if (geometryPointsCount)
    {
        const auto shape = instructions[0].shape;
        const auto beginType = shape == Shape_ClosedFilledPath ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW;
        const auto endType = shape == Shape_ClosedFilledPath ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN;

        wil::com_ptr<ID2D1PathGeometry> geometry;
        THROW_IF_FAILED(factory->CreatePathGeometry(geometry.addressof()));

        wil::com_ptr<ID2D1GeometrySink> sink;
        THROW_IF_FAILED(geometry->Open(sink.addressof()));

        sink->BeginFigure(geometryPoints[0], beginType);
        sink->AddLines(&geometryPoints[1], static_cast<UINT32>(geometryPointsCount - 1));
        sink->EndFigure(endType);

        THROW_IF_FAILED(sink->Close());

        if (beginType == D2D1_FIGURE_BEGIN_FILLED)
        {
            renderTarget->FillGeometry(geometry.get(), brush, nullptr);
        }
        else
        {
            renderTarget->DrawGeometry(geometry.get(), brush, lightLineWidth, nullptr);
        }
    }
}
