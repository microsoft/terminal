use terminal_host::title_translation::translate_console_title;

#[test]
fn microsoft_test_translate_console_title_contract() {
    const SYSTEM_ROOT: &str = "c:\\windows";

    for unexpand in [true, false] {
        for substitute in [true, false] {
            let expected = if substitute { "foo_bar" } else { "foo\\bar" };
            assert_eq!(
                translate_console_title("foo\\bar", unexpand, substitute, SYSTEM_ROOT),
                expected
            );

            let expected = match (unexpand, substitute) {
                (true, true) => "%SystemRoot%_system32_cmd.exe",
                (true, false) => "%SystemRoot%\\system32\\cmd.exe",
                (false, true) => "c:_windows_system32_cmd.exe",
                (false, false) => "c:\\windows\\system32\\cmd.exe",
            };
            assert_eq!(
                translate_console_title(
                    "c:\\windows\\system32\\cmd.exe",
                    unexpand,
                    substitute,
                    SYSTEM_ROOT
                ),
                expected
            );

            let expected = if substitute {
                "x:_file_path"
            } else {
                "x:\\file\\path"
            };
            assert_eq!(
                translate_console_title("x:\\file\\path", unexpand, substitute, SYSTEM_ROOT),
                expected
            );
        }
    }
}
