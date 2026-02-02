
void TerminalInput::_initKeyboardMap() noexcept
try
{
    auto defineKeyWithUnusedModifiers = [this](const int keyCode, const std::wstring& sequence) {
        for (auto m = 0; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = sequence;
    };
    auto defineKeyWithAltModifier = [this](const int keyCode, const std::wstring& sequence) {
        _keyMap[keyCode] = sequence;
        _keyMap[Alt + keyCode] = L"\x1B" + sequence;
    };
    auto defineKeypadKey = [this](const int keyCode, const wchar_t* prefix, const wchar_t finalChar) {
        _keyMap[keyCode] = fmt::format(FMT_COMPILE(L"{}{}"), prefix, finalChar);
        for (auto m = 1; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = fmt::format(FMT_COMPILE(L"{}1;{}{}"), _csi, m + 1, finalChar);
    };
    auto defineEditingKey = [this](const int keyCode, const int parm) {
        _keyMap[keyCode] = fmt::format(FMT_COMPILE(L"{}{}~"), _csi, parm);
        for (auto m = 1; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = fmt::format(FMT_COMPILE(L"{}{};{}~"), _csi, parm, m + 1);
    };
    auto defineNumericKey = [this](const int keyCode, const wchar_t finalChar) {
        _keyMap[keyCode] = fmt::format(FMT_COMPILE(L"{}{}"), _ss3, finalChar);
        for (auto m = 1; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = fmt::format(FMT_COMPILE(L"{}{}{}"), _ss3, m + 1, finalChar);
    };

    _keyMap.clear();

    // The CSI and SS3 introducers are C1 control codes, which can either be
    // sent as a single codepoint, or as a two character escape sequence.
    if (_inputMode.test(Mode::SendC1))
    {
        _csi = L"\x9B";
        _ss3 = L"\x8F";
    }
    else
    {
        _csi = L"\x1B[";
        _ss3 = L"\x1BO";
    }

    // PAUSE doesn't have a VT mapping, but traditionally we've mapped it to ^Z,
    // regardless of modifiers.
    defineKeyWithUnusedModifiers(VK_PAUSE, L"\x1A"s);

    // BACKSPACE maps to either DEL or BS, depending on the Backarrow Key mode.
    // The Ctrl modifier inverts the active mode, swapping BS and DEL (this is
    // not standard, but a modern terminal convention). The Alt modifier adds
    // an ESC prefix (also not standard).
    const auto backSequence = _inputMode.test(Mode::BackarrowKey) ? L"\b"s : L"\x7F"s;
    const auto ctrlBackSequence = _inputMode.test(Mode::BackarrowKey) ? L"\x7F"s : L"\b"s;
    defineKeyWithAltModifier(VK_BACK, backSequence);
    defineKeyWithAltModifier(Ctrl + VK_BACK, ctrlBackSequence);
    defineKeyWithAltModifier(Shift + VK_BACK, backSequence);
    defineKeyWithAltModifier(Ctrl + Shift + VK_BACK, ctrlBackSequence);

    // TAB maps to HT, and Shift+TAB to CBT. The Ctrl modifier has no effect.
    // The Alt modifier adds an ESC prefix, although in practice all the Alt
    // mappings are likely to be system hotkeys.
    const auto shiftTabSequence = fmt::format(FMT_COMPILE(L"{}Z"), _csi);
    defineKeyWithAltModifier(VK_TAB, L"\t"s);
    defineKeyWithAltModifier(Ctrl + VK_TAB, L"\t"s);
    defineKeyWithAltModifier(Shift + VK_TAB, shiftTabSequence);
    defineKeyWithAltModifier(Ctrl + Shift + VK_TAB, shiftTabSequence);

    // RETURN maps to either CR or CR LF, depending on the Line Feed mode. With
    // a Ctrl modifier it maps to LF, because that's the expected behavior for
    // most PC keyboard layouts. The Alt modifier adds an ESC prefix.
    const auto returnSequence = _inputMode.test(Mode::LineFeed) ? L"\r\n"s : L"\r"s;
    defineKeyWithAltModifier(VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Shift + VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Ctrl + VK_RETURN, L"\n"s);
    defineKeyWithAltModifier(Ctrl + Shift + VK_RETURN, L"\n"s);

    // The keypad RETURN key works the same way, except when Keypad mode is
    // enabled, but that's handled below with the other keypad keys.
    defineKeyWithAltModifier(Enhanced + VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Shift + Enhanced + VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Ctrl + Enhanced + VK_RETURN, L"\n"s);
    defineKeyWithAltModifier(Ctrl + Shift + Enhanced + VK_RETURN, L"\n"s);

    if (_inputMode.test(Mode::Ansi))
    {
        // F1 to F4 map to the VT keypad function keys, which are SS3 sequences.
        // When combined with a modifier, we use CSI sequences with the modifier
        // embedded as a parameter (not standard - a modern terminal extension).
        defineKeypadKey(VK_F1, _ss3, L'P');
        defineKeypadKey(VK_F2, _ss3, L'Q');
        defineKeypadKey(VK_F3, _ss3, L'R');
        defineKeypadKey(VK_F4, _ss3, L'S');

        // F5 through F20 map to the top row VT function keys. They use standard
        // DECFNK sequences with the modifier embedded as a parameter. The first
        // five function keys on a VT terminal are typically local functions, so
        // there's not much need to support mappings for them.
        for (auto vk = VK_F5; vk <= VK_F20; vk++)
        {
            static constexpr std::array<uint8_t, 16> parameters = { 15, 17, 18, 19, 20, 21, 23, 24, 25, 26, 28, 29, 31, 32, 33, 34 };
            const auto parm = parameters.at(static_cast<size_t>(vk) - VK_F5);
            defineEditingKey(vk, parm);
        }

        // Cursor keys follow a similar pattern to the VT keypad function keys,
        // although they only use an SS3 prefix when the Cursor Key mode is set.
        // When combined with a modifier, they'll use CSI sequences with the
        // modifier embedded as a parameter (again not standard).
        const auto ckIntroducer = _inputMode.test(Mode::CursorKey) ? _ss3 : _csi;
        defineKeypadKey(VK_UP, ckIntroducer, L'A');
        defineKeypadKey(VK_DOWN, ckIntroducer, L'B');
        defineKeypadKey(VK_RIGHT, ckIntroducer, L'C');
        defineKeypadKey(VK_LEFT, ckIntroducer, L'D');
        defineKeypadKey(VK_CLEAR, ckIntroducer, L'E');
        defineKeypadKey(VK_HOME, ckIntroducer, L'H');
        defineKeypadKey(VK_END, ckIntroducer, L'F');

        // Editing keys follow the same pattern as the top row VT function
        // keys, using standard DECFNK sequences with the modifier embedded.
        defineEditingKey(VK_INSERT, 2);
        defineEditingKey(VK_DELETE, 3);
        defineEditingKey(VK_PRIOR, 5);
        defineEditingKey(VK_NEXT, 6);

        // Keypad keys depend on the Keypad mode. When reset, they transmit
        // the ASCII character assigned by the keyboard layout, but when set
        // they transmit SS3 escape sequences. When used with a modifier, the
        // modifier is embedded as a parameter value (not standard).
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad))
        {
            defineNumericKey(VK_MULTIPLY, L'j');
            defineNumericKey(VK_ADD, L'k');
            defineNumericKey(VK_SEPARATOR, L'l');
            defineNumericKey(VK_SUBTRACT, L'm');
            defineNumericKey(VK_DECIMAL, L'n');
            defineNumericKey(VK_DIVIDE, L'o');

            defineNumericKey(VK_NUMPAD0, L'p');
            defineNumericKey(VK_NUMPAD1, L'q');
            defineNumericKey(VK_NUMPAD2, L'r');
            defineNumericKey(VK_NUMPAD3, L's');
            defineNumericKey(VK_NUMPAD4, L't');
            defineNumericKey(VK_NUMPAD5, L'u');
            defineNumericKey(VK_NUMPAD6, L'v');
            defineNumericKey(VK_NUMPAD7, L'w');
            defineNumericKey(VK_NUMPAD8, L'x');
            defineNumericKey(VK_NUMPAD9, L'y');

            defineNumericKey(Enhanced + VK_RETURN, L'M');
        }
    }
    else
    {
        // In VT52 mode, the sequences tend to use the same final character as
        // their ANSI counterparts, but with a simple ESC prefix. The modifier
        // keys have no effect.

        // VT52 only support PF1 through PF4 function keys.
        defineKeyWithUnusedModifiers(VK_F1, L"\033P"s);
        defineKeyWithUnusedModifiers(VK_F2, L"\033Q"s);
        defineKeyWithUnusedModifiers(VK_F3, L"\033R"s);
        defineKeyWithUnusedModifiers(VK_F4, L"\033S"s);

        // But terminals with application functions keys would
        // map some of them as controls keys in VT52 mode.
        defineKeyWithUnusedModifiers(VK_F11, L"\033"s);
        defineKeyWithUnusedModifiers(VK_F12, L"\b"s);
        defineKeyWithUnusedModifiers(VK_F13, L"\n"s);

        // Cursor keys use the same finals as the ANSI sequences.
        defineKeyWithUnusedModifiers(VK_UP, L"\033A"s);
        defineKeyWithUnusedModifiers(VK_DOWN, L"\033B"s);
        defineKeyWithUnusedModifiers(VK_RIGHT, L"\033C"s);
        defineKeyWithUnusedModifiers(VK_LEFT, L"\033D"s);
        defineKeyWithUnusedModifiers(VK_CLEAR, L"\033E"s);
        defineKeyWithUnusedModifiers(VK_HOME, L"\033H"s);
        defineKeyWithUnusedModifiers(VK_END, L"\033F"s);

        // Keypad keys also depend on Keypad mode, the same as ANSI mappings,
        // but the sequences use an ESC ? prefix instead of SS3.
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad))
        {
            defineKeyWithUnusedModifiers(VK_MULTIPLY, L"\033?j"s);
            defineKeyWithUnusedModifiers(VK_ADD, L"\033?k"s);
            defineKeyWithUnusedModifiers(VK_SEPARATOR, L"\033?l"s);
            defineKeyWithUnusedModifiers(VK_SUBTRACT, L"\033?m"s);
            defineKeyWithUnusedModifiers(VK_DECIMAL, L"\033?n"s);
            defineKeyWithUnusedModifiers(VK_DIVIDE, L"\033?o"s);

            defineKeyWithUnusedModifiers(VK_NUMPAD0, L"\033?p"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD1, L"\033?q"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD2, L"\033?r"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD3, L"\033?s"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD4, L"\033?t"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD5, L"\033?u"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD6, L"\033?v"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD7, L"\033?w"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD8, L"\033?x"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD9, L"\033?y"s);

            defineKeyWithUnusedModifiers(Enhanced + VK_RETURN, L"\033?M"s);
        }
    }

    _focusInSequence = _csi + L"I"s;
    _focusOutSequence = _csi + L"O"s;
}
CATCH_LOG()
