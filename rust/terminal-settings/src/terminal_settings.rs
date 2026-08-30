//! Portable `TerminalSettings` composition and command-line matching semantics.
//!
//! Microsoft's `TerminalSettingsTests` spans `SettingsModel` and the
//! `TerminalSettingsAppAdapter`. This module owns the deterministic profile,
//! binding, override, color, title and launch-position behavior while keeping
//! Win32 environment expansion, executable search and filesystem canonicalization
//! behind [`CommandLinePlatform`].

use std::collections::BTreeMap;

use crate::{
    color_scheme::Color,
    settings_json::{self, JsonObject, JsonValue},
};

const DEFAULT_HISTORY_SIZE: i32 = 9_001;
const DEFAULT_COMMANDLINE: &str = "cmd.exe";
const DEFAULT_CURSOR_COLOR: Color = Color::rgb(0xff, 0xff, 0xff);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TerminalSettingsError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedProfilesArray,
    ExpectedProfileObject,
    ExpectedKeybindingsArray,
    ExpectedKeybindingObject,
    ExpectedCommandObject,
    ExpectedSchemesArray,
    ExpectedSchemeObject,
    InvalidString,
    InvalidInteger,
    InvalidColor,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ShortcutKind {
    SplitPane,
    NewTab,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitDirection {
    Right,
    Down,
    Automatic,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct NewTerminalArgs {
    pub commandline: String,
    pub starting_directory: String,
    pub tab_title: String,
    pub profile: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BindingSnapshot {
    pub shortcut: ShortcutKind,
    pub split_direction: Option<SplitDirection>,
    pub terminal_args: NewTerminalArgs,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalSettingsSnapshot {
    pub profile_guid: Option<String>,
    pub commandline: String,
    pub starting_directory: String,
    pub starting_title: String,
    pub history_size: i32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProfileSelectionSnapshot {
    pub profile_guid: Option<String>,
    pub history_size: i32,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct LaunchPosition {
    pub x: Option<i32>,
    pub y: Option<i32>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ProfileRecord {
    name: String,
    guid: String,
    history_size: i32,
    commandline: String,
    starting_directory: String,
    tab_title: String,
    connection_type: bool,
    color_scheme: Option<String>,
    cursor_color: Option<Color>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct BindingRecord {
    shortcut: ShortcutKind,
    split_direction: Option<SplitDirection>,
    terminal_args: NewTerminalArgs,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ProfileSelection {
    Profile(usize),
    Defaults,
}

/// Platform operations used by Microsoft's command-line normalizer.
///
/// Safe Rust owns tokenization, progressive executable-candidate construction,
/// argument NUL projection and profile-prefix matching. A Windows adapter owns
/// `%VAR%` expansion plus SearchPath/filesystem canonicalization.
pub trait CommandLinePlatform {
    fn expand_environment(&self, command_line: &str) -> String;
    fn resolve_executable(&self, candidate: &str) -> Option<String>;
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalSettingsModel {
    profiles: Vec<ProfileRecord>,
    default_profile_index: usize,
    defaults_history_size: i32,
    defaults_commandline: String,
    warning_count: usize,
    bindings: BTreeMap<String, BindingRecord>,
    schemes: BTreeMap<String, Option<Color>>,
}

impl TerminalSettingsModel {
    /// Parses the subset of `CascadiaSettings` required to compose terminal
    /// settings and action content arguments.
    ///
    /// # Errors
    ///
    /// Returns [`TerminalSettingsError`] for malformed settings containers or
    /// values used by this owner.
    pub fn from_json(input: &str) -> Result<Self, TerminalSettingsError> {
        let root = parse_root(input)?;
        let (defaults_history_size, defaults_commandline) = parse_profile_defaults(&root)?;
        let profiles = parse_profiles(&root, defaults_history_size, &defaults_commandline)?;
        let (default_profile_index, warning_count) = resolve_default_profile(&root, &profiles)?;
        let bindings = parse_bindings(&root)?;
        let schemes = parse_schemes(&root)?;

        Ok(Self {
            profiles,
            default_profile_index,
            defaults_history_size,
            defaults_commandline,
            warning_count,
            bindings,
            schemes,
        })
    }

    #[must_use]
    pub fn active_profile_count(&self) -> usize {
        self.profiles.len()
    }

    #[must_use]
    pub const fn warning_count(&self) -> usize {
        self.warning_count
    }

    #[must_use]
    pub fn binding_count(&self) -> usize {
        self.bindings.len()
    }

    #[must_use]
    pub fn default_profile_guid(&self) -> Option<&str> {
        self.profiles
            .get(self.default_profile_index)
            .map(|profile| profile.guid.as_str())
    }

    #[must_use]
    pub fn profile_guid_by_name(&self, name: &str) -> Option<&str> {
        self.profiles
            .iter()
            .find(|profile| profile.name == name)
            .map(|profile| profile.guid.as_str())
    }

    #[must_use]
    pub fn terminal_args_for_binding(&self, key: &str) -> Option<BindingSnapshot> {
        let binding = self.bindings.get(&normalize_key(key))?;
        Some(BindingSnapshot {
            shortcut: binding.shortcut,
            split_direction: binding.split_direction,
            terminal_args: binding.terminal_args.clone(),
        })
    }

    /// Returns the effective profile selected for `NewTerminalArgs`.
    ///
    /// A command line without an explicit profile selects the best matching
    /// ordinary profile, or the profiles.defaults base layer when no profile
    /// command line matches.
    #[must_use]
    pub fn profile_for_args<P: CommandLinePlatform>(
        &self,
        args: &NewTerminalArgs,
        platform: &P,
    ) -> ProfileSelectionSnapshot {
        match self.select_profile(args, platform) {
            ProfileSelection::Profile(index) => {
                let profile = &self.profiles[index];
                ProfileSelectionSnapshot {
                    profile_guid: Some(profile.guid.clone()),
                    history_size: profile.history_size,
                }
            }
            ProfileSelection::Defaults => ProfileSelectionSnapshot {
                profile_guid: None,
                history_size: self.defaults_history_size,
            },
        }
    }

    /// Composes `TerminalSettings` from a profile index without additional args.
    #[must_use]
    pub fn create_with_profile(&self, index: usize) -> Option<TerminalSettingsSnapshot> {
        self.profiles.get(index).map(snapshot_from_profile)
    }

    /// Composes `TerminalSettings` and then layers `NewTerminalArgs` overrides using
    /// the same ordering as `TerminalSettings::CreateWithNewTerminalArgs`.
    #[must_use]
    pub fn create_with_new_terminal_args<P: CommandLinePlatform>(
        &self,
        args: Option<&NewTerminalArgs>,
        platform: &P,
    ) -> Option<TerminalSettingsSnapshot> {
        let empty = NewTerminalArgs::default();
        let args = args.unwrap_or(&empty);
        let selection = self.select_profile(args, platform);
        let mut result = match selection {
            ProfileSelection::Profile(index) => snapshot_from_profile(self.profiles.get(index)?),
            ProfileSelection::Defaults => TerminalSettingsSnapshot {
                profile_guid: None,
                commandline: self.defaults_commandline.clone(),
                starting_directory: String::new(),
                starting_title: String::new(),
                history_size: self.defaults_history_size,
            },
        };

        if !args.commandline.is_empty() {
            result.commandline.clone_from(&args.commandline);
        }
        if !args.starting_directory.is_empty() {
            result
                .starting_directory
                .clone_from(&args.starting_directory);
        }
        if !args.tab_title.is_empty() {
            result.starting_title.clone_from(&args.tab_title);
        } else if args.profile.is_empty() && !args.commandline.is_empty() {
            result.starting_title = promote_commandline_to_title(&args.commandline);
        }

        Some(result)
    }

    /// Applies a profile's scheme cursor first, then its explicit cursor color.
    #[must_use]
    pub fn cursor_color_for_profile(&self, index: usize) -> Option<Color> {
        let profile = self.profiles.get(index)?;
        let scheme_cursor = profile
            .color_scheme
            .as_ref()
            .and_then(|name| self.schemes.get(name))
            .copied()
            .flatten();
        Some(
            profile
                .cursor_color
                .or(scheme_cursor)
                .unwrap_or(DEFAULT_CURSOR_COLOR),
        )
    }

    fn select_profile<P: CommandLinePlatform>(
        &self,
        args: &NewTerminalArgs,
        platform: &P,
    ) -> ProfileSelection {
        if !args.profile.is_empty() {
            if let Some(index) = self.find_profile(&args.profile) {
                return ProfileSelection::Profile(index);
            }
            return ProfileSelection::Profile(self.default_profile_index);
        }

        if !args.commandline.is_empty() {
            return self
                .profile_for_commandline(&args.commandline, platform)
                .map_or(ProfileSelection::Defaults, ProfileSelection::Profile);
        }

        ProfileSelection::Profile(self.default_profile_index)
    }

    fn find_profile(&self, value: &str) -> Option<usize> {
        self.profiles
            .iter()
            .position(|profile| profile.name == value || profile.guid.eq_ignore_ascii_case(value))
    }

    fn profile_for_commandline<P: CommandLinePlatform>(
        &self,
        commandline: &str,
        platform: &P,
    ) -> Option<usize> {
        let needle = normalize_command_line(commandline, platform);
        let mut candidates = self
            .profiles
            .iter()
            .enumerate()
            .filter(|(_, profile)| !profile.connection_type)
            .map(|(index, profile)| {
                (
                    index,
                    normalize_command_line(&profile.commandline, platform),
                )
            })
            .collect::<Vec<_>>();
        candidates.sort_by(|left, right| right.1.len().cmp(&left.1.len()));
        candidates.into_iter().find_map(|(index, candidate)| {
            starts_with_ignore_ascii_case(&needle, &candidate).then_some(index)
        })
    }
}

fn snapshot_from_profile(profile: &ProfileRecord) -> TerminalSettingsSnapshot {
    TerminalSettingsSnapshot {
        profile_guid: Some(profile.guid.clone()),
        commandline: profile.commandline.clone(),
        starting_directory: profile.starting_directory.clone(),
        starting_title: if profile.tab_title.is_empty() {
            profile.name.clone()
        } else {
            profile.tab_title.clone()
        },
        history_size: profile.history_size,
    }
}

fn parse_root(input: &str) -> Result<JsonObject, TerminalSettingsError> {
    let value = settings_json::parse(input).map_err(|_| TerminalSettingsError::InvalidJson)?;
    value
        .as_object()
        .cloned()
        .ok_or(TerminalSettingsError::ExpectedRootObject)
}

fn parse_profile_defaults(root: &JsonObject) -> Result<(i32, String), TerminalSettingsError> {
    let mut history_size = DEFAULT_HISTORY_SIZE;
    let mut commandline = DEFAULT_COMMANDLINE.to_owned();
    let Some(JsonValue::Object(profiles)) = root.get("profiles") else {
        return Ok((history_size, commandline));
    };
    let Some(JsonValue::Object(defaults)) = profiles.get("defaults") else {
        return Ok((history_size, commandline));
    };
    if let Some(value) = defaults.get("historySize") {
        history_size = json_i32(value)?;
    }
    if let Some(value) = defaults.get("commandline") {
        commandline = json_string(value)?.to_owned();
    }
    Ok((history_size, commandline))
}

fn parse_profiles(
    root: &JsonObject,
    defaults_history_size: i32,
    defaults_commandline: &str,
) -> Result<Vec<ProfileRecord>, TerminalSettingsError> {
    let Some(profiles) = root.get("profiles") else {
        return Ok(Vec::new());
    };
    let values = match profiles {
        JsonValue::Array(values) => values.as_slice(),
        JsonValue::Object(profiles) => match profiles.get("list") {
            Some(JsonValue::Array(values)) => values.as_slice(),
            None => &[],
            Some(_) => return Err(TerminalSettingsError::ExpectedProfilesArray),
        },
        _ => return Err(TerminalSettingsError::ExpectedProfilesArray),
    };

    values
        .iter()
        .enumerate()
        .map(|(index, value)| {
            let object = value
                .as_object()
                .ok_or(TerminalSettingsError::ExpectedProfileObject)?;
            let name = optional_string(object, "name")?
                .unwrap_or_default()
                .to_owned();
            let guid = optional_string(object, "guid")?
                .map_or_else(|| generated_profile_guid(&name, index), str::to_owned);
            let history_size = object
                .get("historySize")
                .map(json_i32)
                .transpose()?
                .unwrap_or(defaults_history_size);
            let commandline = optional_string(object, "commandline")?
                .unwrap_or(defaults_commandline)
                .to_owned();
            let starting_directory = optional_string(object, "startingDirectory")?
                .unwrap_or_default()
                .to_owned();
            let tab_title = optional_string(object, "tabTitle")?
                .unwrap_or_default()
                .to_owned();
            let connection_type = optional_string(object, "connectionType")?.is_some_and(|value| {
                !value
                    .trim_matches(['{', '}'])
                    .chars()
                    .all(|ch| ch == '0' || ch == '-')
            });
            let color_scheme = optional_string(object, "colorScheme")?.map(str::to_owned);
            let cursor_color = optional_string(object, "cursorColor")?
                .map(parse_color)
                .transpose()?;
            Ok(ProfileRecord {
                name,
                guid,
                history_size,
                commandline,
                starting_directory,
                tab_title,
                connection_type,
                color_scheme,
                cursor_color,
            })
        })
        .collect()
}

fn resolve_default_profile(
    root: &JsonObject,
    profiles: &[ProfileRecord],
) -> Result<(usize, usize), TerminalSettingsError> {
    if profiles.is_empty() {
        return Ok((0, 0));
    }
    let Some(requested) = root.get("defaultProfile") else {
        return Ok((0, 0));
    };
    let requested = json_string(requested)?;
    if let Some(index) = profiles.iter().position(|profile| {
        profile.name == requested || profile.guid.eq_ignore_ascii_case(requested)
    }) {
        Ok((index, 0))
    } else {
        // CascadiaSettings emits the invalid-default warning plus the fixup
        // warning when validation retargets WindowSettings to profile zero.
        Ok((0, 2))
    }
}

fn parse_bindings(
    root: &JsonObject,
) -> Result<BTreeMap<String, BindingRecord>, TerminalSettingsError> {
    let Some(keybindings) = root.get("keybindings") else {
        return Ok(BTreeMap::new());
    };
    let JsonValue::Array(keybindings) = keybindings else {
        return Err(TerminalSettingsError::ExpectedKeybindingsArray);
    };
    let mut result = BTreeMap::new();
    for entry in keybindings {
        let entry = entry
            .as_object()
            .ok_or(TerminalSettingsError::ExpectedKeybindingObject)?;
        let command = entry
            .get("command")
            .and_then(JsonValue::as_object)
            .ok_or(TerminalSettingsError::ExpectedCommandObject)?;
        let action = command
            .get("action")
            .map(json_string)
            .transpose()?
            .unwrap_or_default();
        let (shortcut, split_direction) = match action {
            "splitPane" => {
                let direction = match command.get("split").and_then(JsonValue::as_str) {
                    Some("vertical") => SplitDirection::Right,
                    Some("horizontal") => SplitDirection::Down,
                    _ => SplitDirection::Automatic,
                };
                (ShortcutKind::SplitPane, Some(direction))
            }
            "newTab" => (ShortcutKind::NewTab, None),
            _ => continue,
        };
        let terminal_args = NewTerminalArgs {
            commandline: optional_string(command, "commandline")?
                .unwrap_or_default()
                .to_owned(),
            starting_directory: optional_string(command, "startingDirectory")?
                .unwrap_or_default()
                .to_owned(),
            tab_title: optional_string(command, "tabTitle")?
                .unwrap_or_default()
                .to_owned(),
            profile: optional_string(command, "profile")?
                .unwrap_or_default()
                .to_owned(),
        };
        for key in parse_keys(entry.get("keys"))? {
            result.insert(
                normalize_key(&key),
                BindingRecord {
                    shortcut,
                    split_direction,
                    terminal_args: terminal_args.clone(),
                },
            );
        }
    }
    Ok(result)
}

fn parse_keys(value: Option<&JsonValue>) -> Result<Vec<String>, TerminalSettingsError> {
    match value {
        None => Ok(Vec::new()),
        Some(JsonValue::String(value)) => Ok(vec![value.clone()]),
        Some(JsonValue::Array(values)) => values
            .iter()
            .map(|value| json_string(value).map(str::to_owned))
            .collect(),
        Some(_) => Err(TerminalSettingsError::InvalidString),
    }
}

fn parse_schemes(
    root: &JsonObject,
) -> Result<BTreeMap<String, Option<Color>>, TerminalSettingsError> {
    let Some(schemes) = root.get("schemes") else {
        return Ok(BTreeMap::new());
    };
    let JsonValue::Array(schemes) = schemes else {
        return Err(TerminalSettingsError::ExpectedSchemesArray);
    };
    let mut result = BTreeMap::new();
    for scheme in schemes {
        let scheme = scheme
            .as_object()
            .ok_or(TerminalSettingsError::ExpectedSchemeObject)?;
        let name = scheme
            .get("name")
            .map(json_string)
            .transpose()?
            .unwrap_or_default();
        let cursor = optional_string(scheme, "cursorColor")?
            .map(parse_color)
            .transpose()?;
        result.insert(name.to_owned(), cursor);
    }
    Ok(result)
}

fn optional_string<'a>(
    object: &'a JsonObject,
    key: &str,
) -> Result<Option<&'a str>, TerminalSettingsError> {
    match object.get(key) {
        None | Some(JsonValue::Null) => Ok(None),
        Some(JsonValue::String(value)) => Ok(Some(value)),
        Some(_) => Err(TerminalSettingsError::InvalidString),
    }
}

fn json_string(value: &JsonValue) -> Result<&str, TerminalSettingsError> {
    value.as_str().ok_or(TerminalSettingsError::InvalidString)
}

fn json_i32(value: &JsonValue) -> Result<i32, TerminalSettingsError> {
    let value = value
        .as_f64()
        .ok_or(TerminalSettingsError::InvalidInteger)?;
    if value.fract() != 0.0 || value < f64::from(i32::MIN) || value > f64::from(i32::MAX) {
        return Err(TerminalSettingsError::InvalidInteger);
    }
    Ok(value as i32)
}

fn parse_color(value: &str) -> Result<Color, TerminalSettingsError> {
    let hex = value
        .strip_prefix('#')
        .filter(|hex| hex.len() == 6)
        .ok_or(TerminalSettingsError::InvalidColor)?;
    let r = u8::from_str_radix(&hex[0..2], 16).map_err(|_| TerminalSettingsError::InvalidColor)?;
    let g = u8::from_str_radix(&hex[2..4], 16).map_err(|_| TerminalSettingsError::InvalidColor)?;
    let b = u8::from_str_radix(&hex[4..6], 16).map_err(|_| TerminalSettingsError::InvalidColor)?;
    Ok(Color::rgb(r, g, b))
}

fn generated_profile_guid(name: &str, index: usize) -> String {
    let mut hash = 0xcbf2_9ce4_8422_2325u64;
    for byte in name.bytes().chain(index.to_le_bytes()) {
        hash ^= u64::from(byte);
        hash = hash.wrapping_mul(0x0000_0100_0000_01b3);
    }
    format!(
        "{{00000000-0000-0000-0000-{:012x}}}",
        hash & 0xffff_ffff_ffff
    )
}

fn normalize_key(key: &str) -> String {
    key.trim().to_ascii_lowercase()
}

/// Portable Windows command-line tokenization.
///
/// This reproduces the argument values needed by `SettingsModel`. Rust does not
/// reproduce `CommandLineToArgvW`'s HLOCAL allocation or contiguous argv memory
/// layout; that native API-shape remains a classified boundary.
#[must_use]
pub fn command_line_to_argv(command_line: &str) -> Vec<String> {
    let chars = command_line.chars().collect::<Vec<_>>();
    let mut result = Vec::new();
    let mut index = 0usize;

    while index < chars.len() {
        while index < chars.len() && matches!(chars[index], ' ' | '\t') {
            index += 1;
        }
        if index == chars.len() {
            break;
        }

        let mut argument = String::new();
        let mut quoted = false;
        while index < chars.len() {
            if !quoted && matches!(chars[index], ' ' | '\t') {
                break;
            }

            let mut slashes = 0usize;
            while index < chars.len() && chars[index] == '\\' {
                slashes += 1;
                index += 1;
            }

            if index < chars.len() && chars[index] == '"' {
                argument.extend(std::iter::repeat_n('\\', slashes / 2));
                if slashes.is_multiple_of(2) {
                    quoted = !quoted;
                } else {
                    argument.push('"');
                }
                index += 1;
            } else {
                argument.extend(std::iter::repeat_n('\\', slashes));
                if index < chars.len() {
                    argument.push(chars[index]);
                    index += 1;
                }
            }
        }
        result.push(argument);
    }

    result
}

/// Normalizes a profile command line around an injected Windows platform seam.
#[must_use]
pub fn normalize_command_line<P: CommandLinePlatform>(command_line: &str, platform: &P) -> String {
    let expanded = platform.expand_environment(command_line);
    let argv = command_line_to_argv(&expanded);
    if argv.is_empty() {
        return expanded;
    }

    for start_of_arguments in 1..=argv.len() {
        let executable_candidate = argv[..start_of_arguments].join(" ");
        if let Some(mut normalized) = platform.resolve_executable(&executable_candidate) {
            for argument in &argv[start_of_arguments..] {
                normalized.push('\0');
                normalized.push_str(argument);
            }
            return normalized;
        }
    }

    expanded
}

fn starts_with_ignore_ascii_case(value: &str, prefix: &str) -> bool {
    value
        .get(..prefix.len())
        .is_some_and(|candidate| candidate.eq_ignore_ascii_case(prefix))
}

/// Promotes the first command-line component to a tab title per GH#6776.
#[must_use]
pub fn promote_commandline_to_title(command_line: &str) -> String {
    let start = usize::from(command_line.starts_with('"'));
    let tail = &command_line[start..];
    let terminator = if start == 1 {
        tail.find('"')
    } else {
        tail.find(' ')
    };
    tail[..terminator.unwrap_or(tail.len())].to_owned()
}

/// Parses the first two comma-separated optional integer components. A single
/// valid integer applies to both X and Y, matching `ParseCommaSeparatedPair`.
#[must_use]
pub fn launch_position_from_string(input: &str) -> LaunchPosition {
    let mut parts = input.split(',');
    let first = parts.next().and_then(parse_i32_token);
    let has_comma = input.contains(',');
    let second = if has_comma {
        parts.next().and_then(parse_i32_token)
    } else {
        first
    };
    LaunchPosition {
        x: first,
        y: second,
    }
}

fn parse_i32_token(value: &str) -> Option<i32> {
    value.parse::<i32>().ok()
}
