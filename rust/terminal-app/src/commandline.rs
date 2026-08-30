//! Portable startup command-line semantics for Windows Terminal.
//!
//! This owner is deliberately independent of WinRT/XAML. It owns the product
//! behavior exercised by `LocalTests_TerminalApp/CommandlineTest.cpp`: command
//! splitting, startup-action parsing, implicit new-tab behavior, launch modes,
//! and execute-commandline conversion.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Commandline {
    args: Vec<String>,
}

impl Commandline {
    #[must_use]
    pub fn args(&self) -> &[String] {
        &self.args
    }
    #[must_use]
    pub fn argc(&self) -> usize {
        self.args.len()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LaunchMode {
    Fullscreen,
    Maximized,
    Focus,
    MaximizedFocus,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitDirection {
    Automatic,
    Down,
    Right,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitType {
    Manual,
    Duplicate,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FocusDirection {
    Left,
    Right,
    Up,
    Down,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct NewTerminalArgs {
    pub commandline: String,
    pub profile: String,
    pub starting_directory: String,
    pub tab_color: Option<u32>,
    pub color_scheme: String,
}

#[derive(Debug, Clone, PartialEq)]
pub enum StartupAction {
    NewTab(NewTerminalArgs),
    SplitPane {
        split_type: SplitType,
        direction: SplitDirection,
        size: f32,
        terminal: NewTerminalArgs,
    },
    NextTab,
    PrevTab,
    SwitchToTab(u32),
    MoveFocus(FocusDirection),
    SwapPane(FocusDirection),
    FocusPane(u32),
}

impl StartupAction {
    #[must_use]
    pub const fn kind(&self) -> &'static str {
        match self {
            Self::NewTab(_) => "new-tab",
            Self::SplitPane { .. } => "split-pane",
            Self::NextTab => "next-tab",
            Self::PrevTab => "prev-tab",
            Self::SwitchToTab(_) => "switch-to-tab",
            Self::MoveFocus(_) => "move-focus",
            Self::SwapPane(_) => "swap-pane",
            Self::FocusPane(_) => "focus-pane",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CommandlineError {
    InvalidOption(String),
    MissingValue(String),
    ConflictingOptions(String),
    InvalidValue(String),
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct AppCommandlineArgs {
    startup_actions: Vec<StartupAction>,
    launch_mode: Option<LaunchMode>,
    exit_message: String,
    should_exit_early: bool,
}

impl AppCommandlineArgs {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }
    #[must_use]
    pub fn startup_actions(&self) -> &[StartupAction] {
        &self.startup_actions
    }
    #[must_use]
    pub const fn launch_mode(&self) -> Option<LaunchMode> {
        self.launch_mode
    }
    #[must_use]
    pub fn exit_message(&self) -> &str {
        &self.exit_message
    }
    #[must_use]
    pub const fn should_exit_early(&self) -> bool {
        self.should_exit_early
    }

    pub fn parse_command(&mut self, command: &Commandline) -> Result<(), CommandlineError> {
        self.exit_message.clear();
        self.should_exit_early = false;
        let result = self.parse_command_inner(command);
        if let Err(error) = &result {
            self.exit_message = format!("{error:?}");
            self.should_exit_early = true;
        }
        result
    }

    fn parse_command_inner(&mut self, command: &Commandline) -> Result<(), CommandlineError> {
        if command.args.len() <= 1 {
            self.startup_actions.push(default_new_tab());
            return Ok(());
        }
        if command.args.len() == 2
            && matches!(command.args[1].as_str(), "/?" | "-?" | "-h" | "--help")
        {
            self.help();
            return Ok(());
        }

        let mut index = 1;
        while index < command.args.len() && self.apply_launch_option(&command.args[index])? {
            index += 1;
        }
        if index == command.args.len() {
            self.startup_actions.push(default_new_tab());
            return Ok(());
        }

        let tail = &command.args[index + 1..];
        match command.args[index].as_str() {
            "new-tab" | "nt" => {
                if tail.iter().any(|v| matches!(v.as_str(), "-h" | "--help")) {
                    self.help();
                } else {
                    self.startup_actions
                        .push(StartupAction::NewTab(parse_terminal(tail, false)?));
                }
            }
            "split-pane" | "sp" => {
                if tail.iter().any(|v| matches!(v.as_str(), "-h" | "--help")) {
                    self.help();
                } else {
                    self.startup_actions.push(parse_split(tail)?);
                }
            }
            "focus-tab" | "ft" => {
                if let Some(action) = parse_focus_tab(tail)? {
                    self.startup_actions.push(action);
                }
            }
            "move-focus" | "mf" => self
                .startup_actions
                .push(StartupAction::MoveFocus(parse_direction(tail)?)),
            "swap-pane" => self
                .startup_actions
                .push(StartupAction::SwapPane(parse_direction(tail)?)),
            "focus-pane" | "fp" => self
                .startup_actions
                .push(StartupAction::FocusPane(parse_focus_pane(tail)?)),
            _ => self
                .startup_actions
                .push(StartupAction::NewTab(parse_terminal(
                    &command.args[index..],
                    true,
                )?)),
        }
        Ok(())
    }

    pub fn validate_startup_commands(&mut self) {
        if !matches!(self.startup_actions.first(), Some(StartupAction::NewTab(_))) {
            self.startup_actions.insert(0, default_new_tab());
        }
    }

    fn help(&mut self) {
        self.exit_message = "wt - the Windows Terminal\nhelp".to_owned();
        self.should_exit_early = true;
    }

    fn apply_launch_option(&mut self, token: &str) -> Result<bool, CommandlineError> {
        match token {
            "-F" | "--fullscreen" => {
                if self.launch_mode.is_some() && self.launch_mode != Some(LaunchMode::Fullscreen) {
                    return Err(CommandlineError::ConflictingOptions(token.to_owned()));
                }
                self.launch_mode = Some(LaunchMode::Fullscreen);
                Ok(true)
            }
            "-M" | "--maximized" => {
                if self.launch_mode == Some(LaunchMode::Fullscreen) {
                    return Err(CommandlineError::ConflictingOptions(token.to_owned()));
                }
                self.launch_mode = Some(
                    if matches!(
                        self.launch_mode,
                        Some(LaunchMode::Focus | LaunchMode::MaximizedFocus)
                    ) {
                        LaunchMode::MaximizedFocus
                    } else {
                        LaunchMode::Maximized
                    },
                );
                Ok(true)
            }
            "-f" | "--focus" => {
                if self.launch_mode == Some(LaunchMode::Fullscreen) {
                    return Err(CommandlineError::ConflictingOptions(token.to_owned()));
                }
                self.launch_mode = Some(
                    if matches!(
                        self.launch_mode,
                        Some(LaunchMode::Maximized | LaunchMode::MaximizedFocus)
                    ) {
                        LaunchMode::MaximizedFocus
                    } else {
                        LaunchMode::Focus
                    },
                );
                Ok(true)
            }
            "-fM" | "-Mf" => {
                if self.launch_mode == Some(LaunchMode::Fullscreen) {
                    return Err(CommandlineError::ConflictingOptions(token.to_owned()));
                }
                self.launch_mode = Some(LaunchMode::MaximizedFocus);
                Ok(true)
            }
            _ => Ok(false),
        }
    }
}

fn default_new_tab() -> StartupAction {
    StartupAction::NewTab(NewTerminalArgs::default())
}

fn required(
    tokens: &[String],
    index: &mut usize,
    option: &str,
) -> Result<String, CommandlineError> {
    let next = index.saturating_add(1);
    let value = tokens
        .get(next)
        .cloned()
        .ok_or_else(|| CommandlineError::MissingValue(option.to_owned()))?;
    *index = next + 1;
    Ok(value)
}

fn parse_terminal(tokens: &[String], implicit: bool) -> Result<NewTerminalArgs, CommandlineError> {
    let mut out = NewTerminalArgs::default();
    let mut command = Vec::new();
    let mut index = 0;
    let mut command_started = false;
    while index < tokens.len() {
        let token = tokens[index].as_str();
        if command_started {
            command.push(tokens[index].clone());
            index += 1;
            continue;
        }
        match token {
            "--" => {
                command_started = true;
                index += 1;
            }
            "-p" | "--profile" => out.profile = required(tokens, &mut index, token)?,
            "-d" | "--startingDirectory" => {
                out.starting_directory = required(tokens, &mut index, token)?;
            }
            "--tabColor" => {
                let value = required(tokens, &mut index, token)?;
                out.tab_color = parse_color(&value);
            }
            "--colorScheme" => out.color_scheme = required(tokens, &mut index, token)?,
            _ if token.starts_with('-') || (implicit && token.starts_with('/')) => {
                return Err(CommandlineError::InvalidOption(token.to_owned()));
            }
            _ => {
                command_started = true;
                command.push(tokens[index].clone());
                index += 1;
            }
        }
    }
    if !command.is_empty() {
        out.commandline = quote_command(&command);
    }
    Ok(out)
}

fn parse_split(tokens: &[String]) -> Result<StartupAction, CommandlineError> {
    let mut terminal = NewTerminalArgs::default();
    let mut split_type = SplitType::Manual;
    let mut direction = SplitDirection::Automatic;
    let mut size = 0.5_f32;
    let mut horizontal = false;
    let mut vertical = false;
    let mut command = Vec::new();
    let mut index = 0;
    let mut command_started = false;
    while index < tokens.len() {
        let token = tokens[index].as_str();
        if command_started {
            command.push(tokens[index].clone());
            index += 1;
            continue;
        }
        match token {
            "--" => {
                command_started = true;
                index += 1;
            }
            "-H" | "--horizontal" => {
                horizontal = true;
                index += 1;
            }
            "-V" | "--vertical" => {
                vertical = true;
                index += 1;
            }
            "-D" | "--duplicate" => {
                split_type = SplitType::Duplicate;
                index += 1;
            }
            "-s" | "--size" => {
                let raw = required(tokens, &mut index, token)?;
                size = raw
                    .parse::<f32>()
                    .map_err(|_| CommandlineError::InvalidValue(raw.clone()))?;
                if !(0.01..=0.99).contains(&size) {
                    return Err(CommandlineError::InvalidValue(raw));
                }
            }
            "-p" | "--profile" => terminal.profile = required(tokens, &mut index, token)?,
            "-d" | "--startingDirectory" => {
                terminal.starting_directory = required(tokens, &mut index, token)?;
            }
            "--tabColor" => {
                let value = required(tokens, &mut index, token)?;
                terminal.tab_color = parse_color(&value);
            }
            "--colorScheme" => terminal.color_scheme = required(tokens, &mut index, token)?,
            _ if token.starts_with('-') => {
                return Err(CommandlineError::InvalidOption(token.to_owned()));
            }
            _ => {
                command_started = true;
                command.push(tokens[index].clone());
                index += 1;
            }
        }
    }
    if horizontal && vertical {
        return Err(CommandlineError::ConflictingOptions("-H/-V".to_owned()));
    }
    if horizontal {
        direction = SplitDirection::Down;
    } else if vertical {
        direction = SplitDirection::Right;
    }
    if !command.is_empty() {
        terminal.commandline = quote_command(&command);
    }
    Ok(StartupAction::SplitPane {
        split_type,
        direction,
        size,
        terminal,
    })
}

fn parse_focus_tab(tokens: &[String]) -> Result<Option<StartupAction>, CommandlineError> {
    if tokens.is_empty() {
        return Ok(None);
    }
    let mut next = false;
    let mut previous = false;
    let mut target = None;
    let mut index = 0;
    while index < tokens.len() {
        match tokens[index].as_str() {
            "-n" | "--next" => {
                next = true;
                index += 1;
            }
            "-p" | "--previous" => {
                previous = true;
                index += 1;
            }
            "-t" | "--target" => {
                let raw = required(tokens, &mut index, "--target")?;
                target = Some(
                    raw.parse::<u32>()
                        .map_err(|_| CommandlineError::InvalidValue(raw))?,
                );
            }
            other => return Err(CommandlineError::InvalidOption(other.to_owned())),
        }
    }
    if usize::from(next) + usize::from(previous) + usize::from(target.is_some()) > 1 {
        return Err(CommandlineError::ConflictingOptions("focus-tab".to_owned()));
    }
    Ok(if next {
        Some(StartupAction::NextTab)
    } else if previous {
        Some(StartupAction::PrevTab)
    } else {
        target.map(StartupAction::SwitchToTab)
    })
}

fn parse_direction(tokens: &[String]) -> Result<FocusDirection, CommandlineError> {
    if tokens.len() != 1 {
        return Err(CommandlineError::MissingValue("direction".to_owned()));
    }
    match tokens[0].to_ascii_lowercase().as_str() {
        "left" => Ok(FocusDirection::Left),
        "right" => Ok(FocusDirection::Right),
        "up" => Ok(FocusDirection::Up),
        "down" => Ok(FocusDirection::Down),
        other => Err(CommandlineError::InvalidValue(other.to_owned())),
    }
}

fn parse_focus_pane(tokens: &[String]) -> Result<u32, CommandlineError> {
    if tokens.len() != 2 || !matches!(tokens[0].as_str(), "-t" | "--target") {
        return Err(CommandlineError::MissingValue("--target".to_owned()));
    }
    tokens[1]
        .parse::<u32>()
        .map_err(|_| CommandlineError::InvalidValue(tokens[1].clone()))
}

fn parse_color(value: &str) -> Option<u32> {
    let digits = value.strip_prefix('#')?;
    if digits.len() != 6 {
        return None;
    }
    u32::from_str_radix(digits, 16).ok()
}

fn quote_command(tokens: &[String]) -> String {
    tokens
        .iter()
        .map(|token| {
            if token.contains(' ') {
                format!("\"{token}\"")
            } else {
                token.clone()
            }
        })
        .collect::<Vec<_>>()
        .join(" ")
}

#[must_use]
pub fn build_commands<S: AsRef<str>>(raw_args: &[S]) -> Vec<Commandline> {
    let mut commands = vec![Commandline { args: Vec::new() }];
    for raw in raw_args {
        split_argument(&mut commands, raw.as_ref());
    }
    commands
}

fn split_argument(commands: &mut Vec<Commandline>, raw: &str) {
    let mut start = 0;
    let bytes = raw.as_bytes();
    for index in 0..bytes.len() {
        if bytes[index] == b';' && (index == 0 || bytes[index - 1] != b'\\') {
            if start < index {
                commands
                    .last_mut()
                    .expect("command exists")
                    .args
                    .push(raw[start..index].replace("\\;", ";"));
            }
            commands.push(Commandline {
                args: vec!["wt.exe".to_owned()],
            });
            start = index + 1;
        }
    }
    if start < raw.len() {
        commands
            .last_mut()
            .expect("command exists")
            .args
            .push(raw[start..].replace("\\;", ";"));
    } else if raw.is_empty() {
        commands
            .last_mut()
            .expect("command exists")
            .args
            .push(String::new());
    }
}

pub fn parse_startup<S: AsRef<str>>(
    raw_args: &[S],
) -> Result<AppCommandlineArgs, CommandlineError> {
    let mut parser = AppCommandlineArgs::new();
    for command in build_commands(raw_args) {
        parser.parse_command(&command)?;
    }
    parser.validate_startup_commands();
    Ok(parser)
}

#[must_use]
pub fn convert_execute_commandline_to_actions(commandline: &str) -> Vec<StartupAction> {
    let mut raw = vec!["wt.exe".to_owned()];
    raw.extend(commandline.split_whitespace().map(ToOwned::to_owned));
    parse_startup(&raw).map_or_else(|_| Vec::new(), |parser| parser.startup_actions)
}
