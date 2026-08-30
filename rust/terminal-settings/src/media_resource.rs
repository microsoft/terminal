//! Portable media-resource ownership for `SettingsModel`.
//!
//! Windows Terminal stores media values as unresolved paths, resolves them after
//! settings layering, and keeps the origin/base path that supplied each value.
//! This owner mirrors the deterministic layering and fallback rules while keeping
//! filesystem, environment, wallpaper and package lookup behind a platform trait.

use std::{
    cell::RefCell,
    collections::{BTreeMap, BTreeSet},
    rc::Rc,
};

use crate::settings_json::{self, JsonObject, JsonValue};

const DEFAULT_COMMANDLINE: &str = r"C:\Windows\System32\cmd.exe";

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum MediaOrigin {
    InBox,
    ProfilesDefaults,
    User,
    Fragment,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MediaResourceState {
    Pending,
    Resolved,
    Rejected,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MediaResource {
    identity: u64,
    path: String,
    resolved: String,
    state: MediaResourceState,
}

impl MediaResource {
    fn new(identity: u64, path: &str) -> Self {
        Self {
            identity,
            path: path.to_owned(),
            resolved: String::new(),
            state: MediaResourceState::Pending,
        }
    }

    #[must_use]
    pub const fn identity(&self) -> u64 {
        self.identity
    }

    #[must_use]
    pub fn path(&self) -> &str {
        &self.path
    }

    #[must_use]
    pub fn resolved(&self) -> &str {
        if self.state == MediaResourceState::Pending {
            &self.path
        } else {
            &self.resolved
        }
    }

    #[must_use]
    pub fn ok(&self) -> bool {
        self.state == MediaResourceState::Resolved
    }

    #[must_use]
    pub const fn state(&self) -> MediaResourceState {
        self.state
    }

    pub fn resolve(&mut self, path: &str) {
        self.resolved.clear();
        self.resolved.push_str(path);
        self.state = MediaResourceState::Resolved;
    }

    pub fn reject(&mut self) {
        self.resolved.clear();
        self.state = MediaResourceState::Rejected;
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MediaResourceSnapshot {
    pub identity: Option<u64>,
    pub path: String,
    pub resolved: String,
    pub ok: bool,
    pub state: MediaResourceState,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum MediaKind {
    Icon,
    Other,
}

#[derive(Debug, Clone)]
struct MediaSlot {
    origin: MediaOrigin,
    base_path: String,
    kind: MediaKind,
    resource: Rc<RefCell<MediaResource>>,
}

impl MediaSlot {
    fn identity(&self) -> u64 {
        self.resource.borrow().identity()
    }
}

#[derive(Debug, Clone, Default)]
enum IconSetting {
    #[default]
    Missing,
    Null {
        fallback: String,
    },
    Resource {
        slot: MediaSlot,
        fallback: String,
    },
}

#[derive(Debug, Clone, Default)]
struct AppearanceMedia {
    background_image: Option<MediaSlot>,
    pixel_shader: Option<MediaSlot>,
    pixel_shader_image: Option<MediaSlot>,
}

impl AppearanceMedia {
    fn identities(&self) -> impl Iterator<Item = u64> + '_ {
        self.background_image
            .iter()
            .chain(self.pixel_shader.iter())
            .chain(self.pixel_shader_image.iter())
            .map(MediaSlot::identity)
    }
}

#[derive(Debug, Clone)]
struct ProfileMedia {
    name: String,
    commandline: String,
    commandline_explicit: bool,
    icon: IconSetting,
    default_appearance: AppearanceMedia,
    unfocused_appearance: Option<AppearanceMedia>,
    bell_sounds: Vec<MediaSlot>,
}

impl ProfileMedia {
    fn new(name: &str, commandline: &str, icon: IconSetting) -> Self {
        Self {
            name: name.to_owned(),
            commandline: commandline.to_owned(),
            commandline_explicit: false,
            icon,
            default_appearance: AppearanceMedia::default(),
            unfocused_appearance: None,
            bell_sounds: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct MediaFragment<'a> {
    pub source: &'a str,
    pub base_path: &'a str,
    pub content: &'a str,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MediaResourceError {
    InvalidJson,
    ExpectedRootObject,
    ExpectedProfilesContainer,
    ExpectedProfileObject,
    ExpectedActionsArray,
    ExpectedActionObject,
    ExpectedMenuArray,
    ExpectedMenuObject,
    ExpectedBellSoundArray,
    ExpectedString,
}

#[derive(Debug, Default)]
pub struct MediaResourceSettings {
    profiles: BTreeMap<String, ProfileMedia>,
    actions: BTreeMap<String, Option<MediaSlot>>,
    resolution_slots: Vec<MediaSlot>,
    defaults_icon: Option<IconSetting>,
    defaults_commandline: String,
    user_base_path: String,
    next_identity: u64,
    next_synthetic_profile: u64,
}

impl MediaResourceSettings {
    /// Layers inbox settings, enabled fragments and user settings into the
    /// portable media-resource graph.
    ///
    /// # Errors
    ///
    /// Returns [`MediaResourceError`] when any supplied JSON document or media
    /// container has an invalid shape.
    pub fn from_layers(
        inbox: &str,
        user: &str,
        fragments: &[MediaFragment<'_>],
        user_base_path: &str,
    ) -> Result<Self, MediaResourceError> {
        let inbox = parse_root(inbox)?;
        let user = parse_root(user)?;
        let disabled = disabled_fragment_sources(&user)?;

        let mut settings = Self {
            defaults_commandline: DEFAULT_COMMANDLINE.to_owned(),
            user_base_path: user_base_path.to_owned(),
            ..Self::default()
        };

        settings.apply_profiles(&inbox, MediaOrigin::InBox, "", false)?;
        settings.apply_actions(&inbox, MediaOrigin::InBox, "")?;
        settings.apply_new_tab_menu(&inbox, MediaOrigin::InBox, "")?;

        for fragment in fragments {
            if disabled.contains(fragment.source) {
                continue;
            }
            let root = parse_root(fragment.content)?;
            settings.apply_profiles(&root, MediaOrigin::Fragment, fragment.base_path, true)?;
            settings.apply_actions(&root, MediaOrigin::Fragment, fragment.base_path)?;
            settings.apply_new_tab_menu(&root, MediaOrigin::Fragment, fragment.base_path)?;
        }

        settings.apply_profile_defaults(&user)?;
        let user_base_path = settings.user_base_path.clone();
        settings.apply_profiles(&user, MediaOrigin::User, &user_base_path, false)?;
        settings.apply_actions(&user, MediaOrigin::User, &user_base_path)?;
        settings.apply_new_tab_menu(&user, MediaOrigin::User, &user_base_path)?;
        Ok(settings)
    }

    /// Visits unresolved media resources with their effective origin and base
    /// path. Symbol/emoji icons resolve to themselves without invoking the
    /// external resolver, matching `SettingsModel`'s icon shortcut.
    pub fn resolve_media_resources<F>(&mut self, mut resolver: F)
    where
        F: FnMut(MediaOrigin, &str, &mut MediaResource),
    {
        for slot in self.resolution_slots.clone() {
            let mut resource = slot.resource.borrow_mut();
            if resource.path().is_empty() || resource.ok() {
                continue;
            }
            if slot.kind == MediaKind::Icon && is_symbol_icon(resource.path()) {
                let path = resource.path().to_owned();
                resource.resolve(&path);
                continue;
            }
            resolver(slot.origin, &slot.base_path, &mut resource);
        }
    }

    /// Replaces a profile icon at runtime. The new value is user-owned and is
    /// unresolved until [`Self::resolve_media_resources`] visits it.
    pub fn set_profile_icon(&mut self, name: &str, path: &str) -> bool {
        let Some(key) = self.profile_key_by_name(name) else {
            return false;
        };
        let mut profile = self.profiles.remove(&key).expect("profile key must exist");
        let old = icon_identity(&profile.icon);
        let slot = self.new_slot(
            MediaOrigin::User,
            &self.user_base_path.clone(),
            MediaKind::Icon,
            path,
        );
        let fallback = profile.commandline.clone();
        profile.icon = IconSetting::Resource { slot, fallback };
        self.profiles.insert(key, profile);
        if let Some(identity) = old {
            self.prune_if_unreferenced(identity);
        }
        true
    }

    #[must_use]
    pub fn profile_icon(&self, name: &str) -> Option<MediaResourceSnapshot> {
        let profile = self.profile_by_name(name)?;
        Some(icon_snapshot(&profile.icon, &profile.commandline))
    }

    #[must_use]
    pub fn profile_background(&self, name: &str, unfocused: bool) -> Option<MediaResourceSnapshot> {
        let profile = self.profile_by_name(name)?;
        let appearance = if unfocused {
            profile.unfocused_appearance.as_ref()?
        } else {
            &profile.default_appearance
        };
        appearance.background_image.as_ref().map(slot_snapshot)
    }

    #[must_use]
    pub fn profile_pixel_shader(
        &self,
        name: &str,
        unfocused: bool,
    ) -> Option<MediaResourceSnapshot> {
        let profile = self.profile_by_name(name)?;
        let appearance = if unfocused {
            profile.unfocused_appearance.as_ref()?
        } else {
            &profile.default_appearance
        };
        appearance.pixel_shader.as_ref().map(slot_snapshot)
    }

    #[must_use]
    pub fn profile_bell_sounds(&self, name: &str) -> Option<Vec<MediaResourceSnapshot>> {
        let profile = self.profile_by_name(name)?;
        Some(profile.bell_sounds.iter().map(slot_snapshot).collect())
    }

    #[must_use]
    pub fn action_icon(&self, id: &str) -> Option<MediaResourceSnapshot> {
        self.actions.get(id)?.as_ref().map(slot_snapshot)
    }

    fn profile_by_name(&self, name: &str) -> Option<&ProfileMedia> {
        self.profiles.values().find(|profile| profile.name == name)
    }

    fn profile_key_by_name(&self, name: &str) -> Option<String> {
        self.profiles
            .iter()
            .find_map(|(key, profile)| (profile.name == name).then(|| key.clone()))
    }

    fn apply_profile_defaults(&mut self, root: &JsonObject) -> Result<(), MediaResourceError> {
        let Some(JsonValue::Object(profiles)) = root.get("profiles") else {
            return Ok(());
        };
        let Some(JsonValue::Object(defaults)) = profiles.get("defaults") else {
            return Ok(());
        };

        if let Some(commandline) = defaults.get("commandline") {
            let commandline = commandline
                .as_str()
                .ok_or(MediaResourceError::ExpectedString)?;
            self.defaults_commandline.clear();
            self.defaults_commandline.push_str(commandline);
            for profile in self.profiles.values_mut() {
                if !profile.commandline_explicit {
                    profile.commandline.clear();
                    profile.commandline.push_str(commandline);
                }
            }
        }

        let Some(icon) = defaults.get("icon") else {
            return Ok(());
        };

        let old: Vec<u64> = self
            .profiles
            .values()
            .filter_map(|profile| icon_identity(&profile.icon))
            .collect();
        let fallback = self.defaults_commandline.clone();
        let setting = match icon {
            JsonValue::Null => IconSetting::Null { fallback },
            JsonValue::String(path) => {
                let base = self.user_base_path.clone();
                let slot =
                    self.new_slot(MediaOrigin::ProfilesDefaults, &base, MediaKind::Icon, path);
                IconSetting::Resource { slot, fallback }
            }
            _ => return Err(MediaResourceError::ExpectedString),
        };
        self.defaults_icon = Some(setting.clone());
        for profile in self.profiles.values_mut() {
            profile.icon = setting.clone();
        }
        for identity in old {
            self.prune_if_unreferenced(identity);
        }
        Ok(())
    }

    fn apply_profiles(
        &mut self,
        root: &JsonObject,
        origin: MediaOrigin,
        base_path: &str,
        allow_updates: bool,
    ) -> Result<(), MediaResourceError> {
        for object in profile_objects(root)? {
            let key = if allow_updates {
                object
                    .get("updates")
                    .and_then(JsonValue::as_str)
                    .map_or_else(|| self.profile_key(object), str::to_owned)
            } else {
                self.profile_key(object)
            };

            let name = object
                .get("name")
                .and_then(JsonValue::as_str)
                .unwrap_or(&key)
                .to_owned();
            let mut profile = self.profiles.remove(&key).unwrap_or_else(|| {
                ProfileMedia::new(
                    &name,
                    &self.defaults_commandline,
                    self.defaults_icon.clone().unwrap_or_default(),
                )
            });
            let old = self.apply_profile_object(&mut profile, object, origin, base_path)?;
            self.profiles.insert(key, profile);
            for identity in old {
                self.prune_if_unreferenced(identity);
            }
        }
        Ok(())
    }

    fn apply_profile_object(
        &mut self,
        profile: &mut ProfileMedia,
        object: &JsonObject,
        origin: MediaOrigin,
        base_path: &str,
    ) -> Result<Vec<u64>, MediaResourceError> {
        let mut old = Vec::new();
        if let Some(name) = object.get("name") {
            profile.name = name
                .as_str()
                .ok_or(MediaResourceError::ExpectedString)?
                .to_owned();
        }
        if let Some(commandline) = object.get("commandline") {
            profile.commandline = commandline
                .as_str()
                .ok_or(MediaResourceError::ExpectedString)?
                .to_owned();
            profile.commandline_explicit = true;
        }

        if let Some(icon) = object.get("icon") {
            if let Some(identity) = icon_identity(&profile.icon) {
                old.push(identity);
            }
            let fallback = profile.commandline.clone();
            profile.icon = match icon {
                JsonValue::Null => IconSetting::Null { fallback },
                JsonValue::String(path) => {
                    let slot = self.new_slot(origin, base_path, MediaKind::Icon, path);
                    IconSetting::Resource { slot, fallback }
                }
                _ => return Err(MediaResourceError::ExpectedString),
            };
        }

        old.extend(self.apply_appearance_fields(
            &mut profile.default_appearance,
            object,
            origin,
            base_path,
        )?);

        if let Some(unfocused) = object.get("unfocusedAppearance") {
            if let Some(previous) = &profile.unfocused_appearance {
                old.extend(previous.identities());
            }
            match unfocused {
                JsonValue::Null => profile.unfocused_appearance = None,
                JsonValue::Object(unfocused) => {
                    let mut appearance = profile.default_appearance.clone();
                    old.extend(self.apply_appearance_fields(
                        &mut appearance,
                        unfocused,
                        origin,
                        base_path,
                    )?);
                    profile.unfocused_appearance = Some(appearance);
                }
                _ => return Err(MediaResourceError::ExpectedProfileObject),
            }
        }

        if let Some(bell_sounds) = object.get("bellSound") {
            old.extend(profile.bell_sounds.iter().map(MediaSlot::identity));
            let JsonValue::Array(values) = bell_sounds else {
                return Err(MediaResourceError::ExpectedBellSoundArray);
            };
            let mut replacement = Vec::with_capacity(values.len());
            for value in values {
                let path = value.as_str().ok_or(MediaResourceError::ExpectedString)?;
                replacement.push(self.new_slot(origin, base_path, MediaKind::Other, path));
            }
            profile.bell_sounds = replacement;
        }
        Ok(old)
    }

    fn apply_appearance_fields(
        &mut self,
        appearance: &mut AppearanceMedia,
        object: &JsonObject,
        origin: MediaOrigin,
        base_path: &str,
    ) -> Result<Vec<u64>, MediaResourceError> {
        let mut old = Vec::new();
        for (key, target) in [
            ("backgroundImage", &mut appearance.background_image),
            ("experimental.pixelShaderPath", &mut appearance.pixel_shader),
            (
                "experimental.pixelShaderImagePath",
                &mut appearance.pixel_shader_image,
            ),
        ] {
            let Some(value) = object.get(key) else {
                continue;
            };
            if let Some(slot) = target.take() {
                old.push(slot.identity());
            }
            match value {
                JsonValue::Null => {}
                JsonValue::String(path) => {
                    *target = Some(self.new_slot(origin, base_path, MediaKind::Other, path));
                }
                _ => return Err(MediaResourceError::ExpectedString),
            }
        }
        Ok(old)
    }

    fn apply_actions(
        &mut self,
        root: &JsonObject,
        origin: MediaOrigin,
        base_path: &str,
    ) -> Result<(), MediaResourceError> {
        let Some(actions) = root.get("actions") else {
            return Ok(());
        };
        let JsonValue::Array(actions) = actions else {
            return Err(MediaResourceError::ExpectedActionsArray);
        };
        for action in actions {
            let JsonValue::Object(action) = action else {
                return Err(MediaResourceError::ExpectedActionObject);
            };
            let Some(id) = action.get("id").and_then(JsonValue::as_str) else {
                continue;
            };
            let Some(icon) = action.get("icon") else {
                continue;
            };
            let next = match icon {
                JsonValue::Null => None,
                JsonValue::String(path) => {
                    Some(self.new_slot(origin, base_path, MediaKind::Icon, path))
                }
                _ => return Err(MediaResourceError::ExpectedString),
            };
            // SettingsModel currently may still visit an inbox action icon that
            // is replaced by a later user action. Preserve that observable until
            // the upstream GH#19201 cleanup changes the native contract.
            self.actions.insert(id.to_owned(), next);
        }
        Ok(())
    }

    fn apply_new_tab_menu(
        &mut self,
        root: &JsonObject,
        origin: MediaOrigin,
        base_path: &str,
    ) -> Result<(), MediaResourceError> {
        let Some(menu) = root.get("newTabMenu") else {
            return Ok(());
        };
        let JsonValue::Array(entries) = menu else {
            return Err(MediaResourceError::ExpectedMenuArray);
        };
        self.collect_menu_entries(entries, origin, base_path)
    }

    fn collect_menu_entries(
        &mut self,
        entries: &[JsonValue],
        origin: MediaOrigin,
        base_path: &str,
    ) -> Result<(), MediaResourceError> {
        for entry in entries {
            let JsonValue::Object(entry) = entry else {
                return Err(MediaResourceError::ExpectedMenuObject);
            };
            if let Some(icon) = entry.get("icon") {
                let path = icon.as_str().ok_or(MediaResourceError::ExpectedString)?;
                self.new_slot(origin, base_path, MediaKind::Icon, path);
            }
            if let Some(children) = entry.get("entries") {
                let JsonValue::Array(children) = children else {
                    return Err(MediaResourceError::ExpectedMenuArray);
                };
                self.collect_menu_entries(children, origin, base_path)?;
            }
        }
        Ok(())
    }

    fn profile_key(&mut self, object: &JsonObject) -> String {
        if let Some(guid) = object.get("guid").and_then(JsonValue::as_str) {
            return guid.to_owned();
        }
        if let Some(name) = object.get("name").and_then(JsonValue::as_str) {
            return format!("name:{name}");
        }
        self.next_synthetic_profile += 1;
        format!("synthetic:{}", self.next_synthetic_profile)
    }

    fn new_slot(
        &mut self,
        origin: MediaOrigin,
        base_path: &str,
        kind: MediaKind,
        path: &str,
    ) -> MediaSlot {
        self.next_identity += 1;
        let slot = MediaSlot {
            origin,
            base_path: base_path.to_owned(),
            kind,
            resource: Rc::new(RefCell::new(MediaResource::new(self.next_identity, path))),
        };
        self.resolution_slots.push(slot.clone());
        slot
    }

    fn prune_if_unreferenced(&mut self, identity: u64) {
        if self.resource_is_referenced(identity) {
            return;
        }
        self.resolution_slots
            .retain(|slot| slot.identity() != identity);
    }

    fn resource_is_referenced(&self, identity: u64) -> bool {
        if self
            .defaults_icon
            .as_ref()
            .and_then(icon_identity)
            .is_some_and(|candidate| candidate == identity)
        {
            return true;
        }
        if self
            .actions
            .values()
            .flatten()
            .any(|slot| slot.identity() == identity)
        {
            return true;
        }
        self.profiles.values().any(|profile| {
            icon_identity(&profile.icon).is_some_and(|candidate| candidate == identity)
                || profile
                    .default_appearance
                    .identities()
                    .any(|candidate| candidate == identity)
                || profile
                    .unfocused_appearance
                    .as_ref()
                    .is_some_and(|appearance| {
                        appearance
                            .identities()
                            .any(|candidate| candidate == identity)
                    })
                || profile
                    .bell_sounds
                    .iter()
                    .any(|slot| slot.identity() == identity)
        })
    }
}

fn parse_root(input: &str) -> Result<JsonObject, MediaResourceError> {
    match settings_json::parse(input).map_err(|_| MediaResourceError::InvalidJson)? {
        JsonValue::Object(root) => Ok(root),
        _ => Err(MediaResourceError::ExpectedRootObject),
    }
}

fn disabled_fragment_sources(root: &JsonObject) -> Result<BTreeSet<String>, MediaResourceError> {
    let Some(value) = root.get("disabledProfileSources") else {
        return Ok(BTreeSet::new());
    };
    let JsonValue::Array(values) = value else {
        return Err(MediaResourceError::ExpectedString);
    };
    values
        .iter()
        .map(|value| {
            value
                .as_str()
                .map(str::to_owned)
                .ok_or(MediaResourceError::ExpectedString)
        })
        .collect()
}

fn profile_objects(root: &JsonObject) -> Result<Vec<&JsonObject>, MediaResourceError> {
    let Some(profiles) = root.get("profiles") else {
        return Ok(Vec::new());
    };
    let values = match profiles {
        JsonValue::Array(values) => values.as_slice(),
        JsonValue::Object(profiles) => match profiles.get("list") {
            None => return Ok(Vec::new()),
            Some(JsonValue::Array(values)) => values.as_slice(),
            Some(_) => return Err(MediaResourceError::ExpectedProfilesContainer),
        },
        _ => return Err(MediaResourceError::ExpectedProfilesContainer),
    };
    values
        .iter()
        .map(|value| {
            value
                .as_object()
                .ok_or(MediaResourceError::ExpectedProfileObject)
        })
        .collect()
}

fn icon_identity(icon: &IconSetting) -> Option<u64> {
    match icon {
        IconSetting::Resource { slot, .. } => Some(slot.identity()),
        IconSetting::Missing | IconSetting::Null { .. } => None,
    }
}

fn slot_snapshot(slot: &MediaSlot) -> MediaResourceSnapshot {
    let resource = slot.resource.borrow();
    MediaResourceSnapshot {
        identity: Some(resource.identity()),
        path: resource.path().to_owned(),
        resolved: resource.resolved().to_owned(),
        ok: resource.ok(),
        state: resource.state(),
    }
}

fn icon_snapshot(icon: &IconSetting, commandline: &str) -> MediaResourceSnapshot {
    match icon {
        IconSetting::Missing => fallback_snapshot("", commandline),
        IconSetting::Null { fallback } => fallback_snapshot("", fallback),
        IconSetting::Resource { slot, fallback } => {
            let snapshot = slot_snapshot(slot);
            if snapshot.state == MediaResourceState::Rejected {
                MediaResourceSnapshot {
                    identity: snapshot.identity,
                    path: snapshot.path,
                    resolved: fallback.clone(),
                    ok: true,
                    state: MediaResourceState::Resolved,
                }
            } else {
                snapshot
            }
        }
    }
}

fn fallback_snapshot(path: &str, fallback: &str) -> MediaResourceSnapshot {
    MediaResourceSnapshot {
        identity: None,
        path: path.to_owned(),
        resolved: fallback.to_owned(),
        ok: true,
        state: MediaResourceState::Resolved,
    }
}

fn is_symbol_icon(path: &str) -> bool {
    !path.is_ascii()
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MediaPathResolution {
    pub ok: bool,
    pub resolved: String,
}

pub trait MediaPlatform {
    fn file_exists(&self, path: &str) -> bool;
    fn environment(&self, name: &str) -> Option<String>;
    fn desktop_wallpaper(&self) -> Option<String>;
}

/// Resolves the portable portion of the native media-path algorithm.
///
/// File existence, environment lookup and desktop-wallpaper discovery are
/// injected through [`MediaPlatform`], leaving the Win32 adapter outside this
/// safe Rust owner.
#[must_use]
pub fn resolve_media_path<P: MediaPlatform>(
    path: &str,
    base_path: &str,
    platform: &P,
) -> MediaPathResolution {
    if path.eq_ignore_ascii_case("none") {
        return accepted("");
    }
    if path.eq_ignore_ascii_case("desktopWallpaper") {
        return platform
            .desktop_wallpaper()
            .map_or_else(rejected, |wallpaper| accepted(&wallpaper));
    }
    if path.starts_with("ms-resource:///") || path.starts_with("ms-appx:///") {
        return accepted(path);
    }
    if let Some(file_path) = path.strip_prefix("file:///") {
        return resolve_file_candidate(file_path, base_path, platform);
    }
    if let Some(rest) = path.strip_prefix("https://") {
        return resolve_leaf_candidate(rest, base_path, platform);
    }
    if let Some(rest) = path.strip_prefix("ms-appx://") {
        return resolve_leaf_candidate(rest, base_path, platform);
    }
    if path.contains("://") || path.starts_with("http:") || path.starts_with("https:") {
        return rejected();
    }
    if let Some(name) = environment_reference(path) {
        let Some(expanded) = platform.environment(name) else {
            return rejected();
        };
        return resolve_file_candidate(&expanded, base_path, platform);
    }
    resolve_file_candidate(path, base_path, platform)
}

fn resolve_leaf_candidate<P: MediaPlatform>(
    rest: &str,
    base_path: &str,
    platform: &P,
) -> MediaPathResolution {
    let Some(leaf) = rest.rsplit('/').next().filter(|leaf| !leaf.is_empty()) else {
        return rejected();
    };
    resolve_file_candidate(leaf, base_path, platform)
}

fn resolve_file_candidate<P: MediaPlatform>(
    path: &str,
    base_path: &str,
    platform: &P,
) -> MediaPathResolution {
    let candidate = if is_absolute_windows_path(path) {
        normalize_windows_path(path)
    } else {
        normalize_windows_path(&format!(r"{base_path}\{path}"))
    };
    if platform.file_exists(&candidate) {
        accepted(&candidate)
    } else {
        rejected()
    }
}

fn environment_reference(path: &str) -> Option<&str> {
    path.strip_prefix('%')?
        .strip_suffix('%')
        .filter(|name| !name.is_empty())
}

fn is_absolute_windows_path(path: &str) -> bool {
    let bytes = path.as_bytes();
    path.starts_with(r"\\")
        || (bytes.len() >= 3 && bytes[1] == b':' && matches!(bytes[2], b'\\' | b'/'))
}

fn normalize_windows_path(path: &str) -> String {
    let path = path.replace('/', r"\");
    if path.starts_with(r"\\?\") || path.starts_with(r"\\") {
        return path;
    }
    let bytes = path.as_bytes();
    let (prefix, rest) = if bytes.len() >= 3 && bytes[1] == b':' && bytes[2] == b'\\' {
        (&path[..3], &path[3..])
    } else {
        ("", path.as_str())
    };
    let mut parts = Vec::new();
    for part in rest.split('\\') {
        match part {
            "" | "." => {}
            ".." => {
                parts.pop();
            }
            _ => parts.push(part),
        }
    }
    format!("{prefix}{}", parts.join(r"\"))
}

fn accepted(path: &str) -> MediaPathResolution {
    MediaPathResolution {
        ok: true,
        resolved: path.to_owned(),
    }
}

fn rejected() -> MediaPathResolution {
    MediaPathResolution {
        ok: false,
        resolved: String::new(),
    }
}
