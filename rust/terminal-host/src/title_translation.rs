//! Portable console-title translation semantics from `TitleTests.cpp`.
//!
//! The host uses this helper when it needs a filesystem-safe title and may
//! first unexpand a Windows-directory prefix back to `%SystemRoot%`.

/// Translates a console title using the same observable rules exercised by
/// Microsoft's `TitleTests::TestTranslateConsoleTitle` vectors.
#[must_use]
pub fn translate_console_title(
    title: &str,
    unexpand_system_root: bool,
    substitute_backslashes: bool,
    system_root: &str,
) -> String {
    let mut translated = if unexpand_system_root {
        unexpand_prefix(title, system_root)
    } else {
        title.to_owned()
    };

    if substitute_backslashes {
        translated = translated.replace('\\', "_");
    }

    translated
}

fn unexpand_prefix(title: &str, system_root: &str) -> String {
    if title.len() < system_root.len() {
        return title.to_owned();
    }

    let (prefix, suffix) = title.split_at(system_root.len());
    if prefix.eq_ignore_ascii_case(system_root) && (suffix.is_empty() || suffix.starts_with('\\')) {
        format!("%SystemRoot%{suffix}")
    } else {
        title.to_owned()
    }
}

#[cfg(test)]
mod tests {
    use super::translate_console_title;

    #[test]
    fn unexpand_requires_a_path_boundary() {
        assert_eq!(
            translate_console_title("c:\\windows-old\\cmd.exe", true, false, "c:\\windows"),
            "c:\\windows-old\\cmd.exe"
        );
    }

    #[test]
    fn unexpand_is_case_insensitive() {
        assert_eq!(
            translate_console_title("C:\\WINDOWS\\System32\\cmd.exe", true, false, "c:\\windows"),
            "%SystemRoot%\\System32\\cmd.exe"
        );
    }
}
