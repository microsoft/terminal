#![forbid(unsafe_code)]

#[allow(clippy::assigning_clones, clippy::missing_errors_doc)]
mod commandline;
mod filtered_command;
mod fzf;

pub use commandline::{
    AppCommandlineArgs, Commandline, CommandlineError, FocusDirection, LaunchMode, NewTerminalArgs,
    SplitDirection, SplitType, StartupAction, build_commands,
    convert_execute_commandline_to_actions, parse_startup,
};
pub use filtered_command::FilteredCommand;
pub use fzf::{MatchResult, Pattern, TextRun, match_text, parse_pattern};
