//! Pure process-name classification from `ConsoleShimPolicy`.
//!
//! Querying a process module path remains a Windows platform boundary. This
//! module captures only the deterministic filename classification used to
//! enable the cmd.exe and PowerShell compatibility shims.

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct ConsoleShimPolicy {
    pub is_cmd: bool,
    pub is_powershell: bool,
}

/// Classifies a process path using the ASCII-insensitive comparisons from the
/// C++ shim policy.
///
/// Both slash styles are accepted so the pure contract can be exercised on
/// Linux and Windows while retaining Windows filename semantics.
#[must_use]
pub fn classify_process_path(path: &str) -> ConsoleShimPolicy {
    let filename = path.rsplit(['/', '\\']).next().unwrap_or_default();

    let is_cmd = filename.eq_ignore_ascii_case("cmd.exe");
    let is_powershell = filename.eq_ignore_ascii_case("powershell.exe")
        || filename.eq_ignore_ascii_case("pwsh.exe");

    ConsoleShimPolicy {
        is_cmd,
        is_powershell,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cmd_is_ascii_case_insensitive() {
        assert_eq!(
            classify_process_path(r"C:\Windows\System32\CMD.EXE"),
            ConsoleShimPolicy {
                is_cmd: true,
                is_powershell: false,
            }
        );
    }

    #[test]
    fn inbox_and_core_powershell_are_recognized() {
        for path in [
            r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
            r"C:\Program Files\PowerShell\7\PwSh.ExE",
        ] {
            assert_eq!(
                classify_process_path(path),
                ConsoleShimPolicy {
                    is_cmd: false,
                    is_powershell: true,
                }
            );
        }
    }

    #[test]
    fn unrelated_executables_enable_no_shim() {
        assert_eq!(
            classify_process_path(r"C:\Windows\System32\conhost.exe"),
            ConsoleShimPolicy::default()
        );
    }

    #[test]
    fn filename_only_and_forward_slashes_are_supported() {
        assert!(classify_process_path("cmd.exe").is_cmd);
        assert!(classify_process_path("C:/Program Files/PowerShell/7/pwsh.exe").is_powershell);
    }

    #[test]
    fn empty_or_trailing_separator_is_not_a_known_client() {
        assert_eq!(classify_process_path(""), ConsoleShimPolicy::default());
        assert_eq!(
            classify_process_path(r"C:\Windows\"),
            ConsoleShimPolicy::default()
        );
    }
}
