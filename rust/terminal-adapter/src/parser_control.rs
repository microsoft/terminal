//! Narrow parser-control operations owned by Adapter semantics.
//!
//! Microsoft `AdaptDispatch` owns a small back-edge into the live VT parser:
//! ANSI/VT52 grammar selection, C1 acceptance, and coding-system designation.
//! Keeping those operations here avoids inventing duplicate parser-mode state
//! inside the product dispatch aggregate while preserving the observable
//! Adapter contract in safe Rust.

use terminal_parser::state_machine::{ParserMode, StateMachine, StateMachineEngine};

/// Windows code page selected by the ISO-2022 coding-system designation.
pub const ISO_8859_1_CODE_PAGE: u32 = 28_591;
/// Windows code page selected by the UTF-8 coding-system designation.
pub const UTF8_CODE_PAGE: u32 = 65_001;

/// Portable semantic form of Microsoft's coding-system switch.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CodingSystem {
    Iso2022,
    Utf8,
}

impl CodingSystem {
    /// Converts the VT designator emitted by the parser after `ESC %`.
    #[must_use]
    pub const fn from_designator(designator: u64) -> Option<Self> {
        match designator {
            value if value == b'@' as u64 => Some(Self::Iso2022),
            value if value == b'G' as u64 => Some(Self::Utf8),
            _ => None,
        }
    }

    #[must_use]
    pub const fn output_code_page(self) -> u32 {
        match self {
            Self::Iso2022 => ISO_8859_1_CODE_PAGE,
            Self::Utf8 => UTF8_CODE_PAGE,
        }
    }

    #[must_use]
    pub const fn accepts_c1(self) -> bool {
        matches!(self, Self::Iso2022)
    }
}

/// Applies the Adapter-owned ANSI/VT52 parser mode directly to the live parser.
pub fn set_ansi_mode<E: StateMachineEngine>(machine: &mut StateMachine<E>, enabled: bool) {
    machine.set_parser_mode(ParserMode::Ansi, enabled);
}

/// Applies Microsoft's `AcceptC1Controls` back-edge to the live parser.
pub fn set_accept_c1_controls<E: StateMachineEngine>(machine: &mut StateMachine<E>, enabled: bool) {
    machine.set_parser_mode(ParserMode::AcceptC1, enabled);
}

/// Applies Microsoft's coding-system side effects and returns the code page the
/// native boundary must select. `ISO-2022` enables C1 parsing and selects
/// ISO-8859-1; UTF-8 disables C1 parsing and selects `CP_UTF8`.
pub fn designate_coding_system<E: StateMachineEngine>(
    machine: &mut StateMachine<E>,
    system: CodingSystem,
) -> u32 {
    machine.set_parser_mode(ParserMode::AcceptC1, system.accepts_c1());
    system.output_code_page()
}
