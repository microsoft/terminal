---
author: Mike Griese @zadjii-msft
created on: 2019-05-31
last updated: 2019-05-31
issue id: #754
---

# Cascading Default + User Settings

## Abstract

This spec outlines adding support for a cascading settings model. In this model,
there are two settings files, instead of one.

1. The default settings file
2. The user's settings file

The default settings file would be a static, read-only file shipped with the
terminal. The user settings file would then contain all the user's chosen
customizations to the settings. These two files would then be composed together
when the app is launched, so that the runtime settings are the union of both the
defaults and whatever modifications the user has chosen. This will enable the
app to always use a default schema that it knows will be valid, and minimize the
settings that the user needs to customize.

Should the settings schema ever change, the defaults file will change, without
needing to re-write the user's settings file.


## Inspiration

Largely inspired by the settings model that both VS Code (and Sublime Text) use.

## Solution Design

The settings are now composed from two files: a "Default" settings file, and a
"User" settings file.

We'll have a static version of the "Default" file hardcoded within the
application. We'll write the hardcoded version of this file out to an actual
file on disk, `defaults.json`, however, that file will not actually be used. The
defaults file will contain some sort of disclaimer like:

```json
// This is an auto-generated file. Place any modifications to your settings in "profiles.json"
```

This disclaimer will help identify that the file shouldn't be modified.

### Goal: Minimize Writing to `profiles.json`

This isn't great, as each write of the file will randomly re-order the file.

### Layering settings


### Hiding Default Profiles

What if a user doesn't want to see one of the profiles that we've included in the default profiles?

We could alternatively add a `hidden` key to each profile, which defaults to false. When we want to mark a profile as hidden, we'd just set that value to `true`, instead of trying to look up the profile's guid.

### Dynamic Profiles

<!-- How do we handle things like WSL distros, or Azure Connections, which we've
generated at runtime?

Currently, for things like that, we're only generating them on first launch
without a settings file. If we add support for some new dynamic profile, and you
already have a settings file, then you won't get the new types of profiles.
However, on the upside, if you don't have a settings file, the new profiles will
appear automatically, without having to do anything.

We could add a button to force the lookup of these profiles. The button could be
somewhere in the settings dialog. So long as we use stable GUIDs for each
dynamic profile, we could ensure that if a profile already exists in their
settings file, then we'd just add it.

However, this loses the automatic generation behavior that the dynamic profiles
currently have.

Is there a good way that we could auto-generate these profiles constantly, and

We could have the auto-generation logic determine that a profile source is no
longer valid, and add it to the `hiddenProfiles` array.

Downsides to auto-generating:
 * Any time we determine that a new profile should be added, we need to write
   out the `profiles.json` file.
 * Any time a profile source is removed, we need to update `hiddenProfiles` in
   `profiles.json`.
   - What if someone really wants to keep a hidden profile around for some
     reason?
   - Perhaps we could introduce a `autoloadWslProfiles` setting, that controls
     whether that behavior should run or not. Default to true, so the user can
     turn that behavior off if they want.
     - (similar with any azure shells)

<hr> -->

Sometimes, we may want to auto-generate a profile on the user's behalf. Consider
the case of WSL distros on their machine, or VMs running in Azure they may want
to auto-connect to. These _dynamic_ profiles have a source that might be added
or removed after the app is installed, and they will be different from user to
user.

Currently, these profiles are only generated when a user first launches the
Terminal. If they already have a `profiles.json` file, then we won't run the
auto-generation behavior. This is obviously not great - if any new types of
dynamic profiles are added, then users that already have the Terminal installed
won't get any of these dynamic profiles. Furthemore, if any of the sources of
these dynamic profiles are removed, then the app won't auto-remove the
associated profile.

In the new model, with a combined defaults & user settings, how should these
dynamic profiles work?

I propose we add functionality to automatically search for these profile sources
and add/remove them on _every_ Terminal launch. To make this functionality work
appropriately, we'll need to introduce a constraint on dynamic profiles.

**For any dynamic profiles, they must be able to be generated using a stable
GUID**. For example, any time we try adding the "Ubuntu" profile, we must be
able to generate the same GUID every time. This way, when a dynamic profile
generator runs, it can check if that profile source already has a profile
associated with it, and do nothing (as to not create many duplicate "Ubuntu"
profiles, for example).

When a dynamic profile generator runs, it will determine what new profiles need
to be added to the user settings, and it can append those to the list of
profiles. The generator will return some sort of result indicating that it wants
a save operation to occur. The app will then save the `profiles.json` file,
including these new profiles.

If a dynamic profile generator has determined that a profile should no longer be
used, we don't want to totally remove the profile from the file - perhaps
there's some data the user would like from those settings. Instead, we'll simply
add the profile's GUID to the `hiddenProfiles` list. That way, the profile will
remain in their settings, but will not appear in the list of profiles.

Should a dynamic profile generator try to create a profile that's already in the
`hiddenProfiles` list, it'll make sure to remove that profile from the
`hiddenProfiles` list, making the original visible again.

Additionally, a user might not want a dynamic profile generator to always run.
They might want to keep their Azure connections visible in the list of profiles,
even if its no longer a valid target. Or they might want to not automatically
connect to Azure to find new instances every time they launch the terminal. To
enable scenarios like this, **each dynamic profile generator must provide a way
to disable it**. For the above listed cases, the two settings might be something
like `autoloadWslProfiles` and `autoloadAzureConnections`. These will be set to
true in the default settings, but the user can override them to false if they so
chose.

### Powershell Core & the Defaults

How do we handle the potential existence of powershell core in this model?
Powershell core is unique as far as the default profiles goes - it may or may
not exist on the user's system. Not only that, but depending on the user's
install of powershell core, it might have a path in either `Program Files` or
`Program Files(x86)`.

Additionally, if it _is_ installed, we set it as the default profile instead of
Windows Powershell.

Should we include it in the default profiles? If we wanted to do this, we'd need
to:
* hide it in the user settings if it's not installed.
* Set the commandline in the user settings. If the user deletes this value, then
  that profile would be unable to launch, unlike all the other profiles that are
  in the user settings (by default).
* Make sure to set the default profile in the user settings.

Or, should we only add it to the user settings?
* We'd only  hide it in the user settings if it's not installed.
* Set the commandline in the user settings. If the user deletes this value, then
  that profile would be unable to launch, unlike all the other profiles that are
  in the user settings (by default).
* Make sure to set the default profile in the user settings.

Currently there are only a few things that we actually set for the Powershell Core profile

### Unbinding a Keybinding

### Removing the Globals Object

### Default `profiles.json`

Below is an example of what the default

```js
{
    "profiles": [
        {
            // Make changes here to the cmd.exe profile
            "guid": "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
        }
    ],
    // To hide a profile, add its guid to this array
    "hiddenProfiles": [],

    // Add custom color schemes to this array
    "schemes": [],

    // Add any keybinding overrides to this array.
    // To unbind a default keybinding, set the command to "unbound"
    "keybindings": []
}

```

## UI/UX Design

### Two Files - `defaults.json` and `profiles.json`
#### Opening `defaults.json`
How do we open both these files to show to the user (for the interim period
before a proper Settings UI is created)? Currently, the "Settings" button only
opens a single json file, `profiles.json`. We could keep that button doing the
same thing, though we want the user to be able to also view the default settings
file, to be able to inspect what settings they wish to change.

We could have the "Settings" button open _both_ files at the same
time. I'm not sure that `ShellExecute` (which is used to open these files)
provides any ordering guarantees, so it's possible that the `defaults.json`
would open in the foreground of the default json editor, while making in unclear
that there's another file they should be opening instead. Additionally, if
there's _no_ `.json` editor for the user, I believe the shell will attempt
_twice_ to ask the user to select a program to open the file with, and it might
not be clear that they need to select a program in both dialogs.

Alternatively, we could make the defaults file totally inaccessible from the
Terminal UI, and instead leave a comment in the auto-generated `profiles.json`
like so:

```json
// To view the default settings, open the defaults.json file in this directory
```

The "Settings" button would then only open the file the user needs to edit, and
provide them instructions on how to open the defaults file.

### How does this work with the settings UI?

How can we tell that a setting should be written back to the user settings file?

If we only have one version of the settings model (Globals, Profiles,
ColorShemes, Keybindings), and the user changes one of the settings with the
settings UI, how can we tell that settings changed?

If the value of the setting isn't the same as the defaults, then it could easily
be added to the user's `profiles.json`. We'll have to do a smart serialization
of the various settings models. We'll pass in the default version of that model
during the serialization. If that object finds that a particular setting is the
same as a default setting, then we'll skip serializing it.

What happens if a user has chosen to set the value to _coincidentally_ the same
value as the default value? We should keep that key in the user's settings file,
even though it is the same.

In order to facilitate this, we'll need to keep the originally parsed user
settings around in memory. When we go to serialize the settings, we'll check if
either the setting exists already in the user settings file, or the setting has
changed. If either is true, then we'll make sure to write that setting back out.

## Capabilities
### Security
### Reliability
### Performance, Power, and Efficiency
### Accessibility
N/A

## Potential Issues

### Migrating Existing Settings

## Future considerations

## Resources
