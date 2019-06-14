---
author: Mike Griese @zadjii-msft
created on: 2019-05-31
last updated: 2019-06-13
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

### Goal: Minimize Writing to `profiles.json`

We want to write the user settings file, `profiles.json`, as little as possible.
Each time we write the file, there's the possiblity that we've re-ordered the
keys, as `jsoncpp` provides no ordering guarantee of the keys. This isn't great,
as each write of the file will randomly re-order the file.

One of our overarching goals with this change should be to re-write the user
settings file as little as possible.

## Solution Design

The settings are now composed from two files: a "Default" settings file, and a
"User" settings file.

### Default Settings

We'll have a static version of the "Default" file hardcoded within the
application. We'll write the hardcoded version of this file out to an actual
file on disk, `defaults.json`, however, that file will not actually be used. The
defaults file will contain some sort of disclaimer like:

```json
// This is an auto-generated file. Place any modifications to your settings in "profiles.json"
```

This disclaimer will help identify that the file shouldn't be modified.

The `defaults.json` file will be written out whenever we detect that it doesn't exist.

**TODO**: We also need to write it out if it changes, though ideally without
actually loading the file at first load. Perhaps we could start another
thread/`fire_and_forget` once we're loaded to re-scan the file and re-write it
if it has changed.

Because the `defaults.json` file is hardcoded within our application, we can use
its text directly, without loading the file from disk. This should help save
some startup time, as we'll only need to load the user settings from disk.

When we make changes to the default settings, or we make changes to the settings
schema, we should make sure that we update the hardcoded `defaults.json` with
the new values. That way, the `defaults.json` file will always have the complete
set of setings in it.


### Layering settings

When we load the settings, we'll do it in two stages. First, we'll deserialize the
default settings that we've hardcoded. Then, we'll intelligently layer the
user's setting upon those we've already loaded. Some objects, like the default
profiles, we'll need to make sure to load from the user settings into the
existing objects we created from the default settings.

* We'll need to make sure that any profile in the user settings that has a GUID
  matching a default profile loads the user settings into the object created
  from the defaults.
* We'll need to make sure that there's only one action bound to each key chord
  for a keybinding. If there are any key chords in the user settings that match
  a default key chord, we should bind them to the action from the user settings
  instead.
* For any color schemes who's name matches the name of a default color scheme,
  we'll need to apply the user settings to the existing color scheme.

### Hiding Default Profiles

What if a user doesn't want to see one of the profiles that we've included in
the default profiles?

We will add a `hidden` key to each profile, which defaults to false. When we
want to mark a profile as hidden, we'd just set that value to `true`, instead of
trying to look up the profile's guid.


So, if someone wanted to hide the default cmd.exe profile, all they'd have to do
is add `"hidden": true` to the cmd.exe entry in their user settings, like so:

```js
{
    "profiles": [
        {
            // Make changes here to the cmd.exe profile
            "guid": "{6239a42c-1de4-49a3-80bd-e8fdd045185c}",
            "hidden": true
        }
    ],
```

#### Hidden Profiles and the Open New Tab shortcuts

Currently, there are keyboard shortcuts for "Open New Tab With Profile
&lt;N&gt;". These shortcuts will open up the Nth profile in the new tab
dropdown. Considering we're adding the ability to remove profiles from that
list, but keep them in the overall list of profiles, we'll need to make sure
that the handler for that event still opens the Nth _visible_ profile.

### Dynamic Profiles

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
mark that profile as `"hidden": true`. That way, the profile will remain in
their settings, but will not appear in the list of profiles.

Should a dynamic profile generator try to create a profile that's already
`hidden`, it'll make sure to set `hidden` back to false, making the original
visible again.

Additionally, a user might not want a dynamic profile generator to always run.
They might want to keep their Azure connections visible in the list of profiles,
even if its no longer a valid target. Or they might want to not automatically
connect to Azure to find new instances every time they launch the terminal. To
enable scenarios like this, **each dynamic profile generator must provide a way
to disable it**. For the above listed cases, the two settings might be something
like `autoloadWslProfiles` and `autoloadAzureConnections`. These will be set to
true in the default settings, but the user can override them to false if they so
chose.

#### What if a dynamic profile is removed, but it's the default?

We should probably add an extra validation step at the end of serializing to
ensure that the default profile isn't a hidden profile. If the user's set
default profile is a hidden profile, we'll just set the first non-hidden profile
as the default profile.

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
* We'd only generate it in the user settings if it's installed.
* Set the commandline in the user settings. If the user deletes this value, then
  that profile would be unable to launch, unlike all the other profiles that are
  in the user settings (by default).
* Make sure to set the default profile in the user settings.

Currently there are only a few things that we actually set for the Powershell
Core profile - the name, commandline, icon, and the color scheme. I don't
believe it would add too much bloat to the user's settings to generate it in the
user's settings by default.

As a third option, we could treat the Powershell Core profile like a dynamic
profile, and have it autogenerated if it exists, and auto-hidden if it's
uninstalled. We'd need to additionally include `autoloadPowershellCoreProfile`,
set to true in the default settings. However, none of the other profile
generators need to be able to change the default profile.

Considering that the first option would still need to create an entry for
powershell core in the user's settings file, I'd lean towards the third option -
making it a dynamic profile just like WSL distros.

### Unbinding a Keybinding

How can a user unbind a key that's part of the default keybindings? What if a
user really wants <kbd>ctrl</kbd>+<kbd>t</kbd> to fall through to the
commandline application attached to the shell, instead of opening a new tab?

We'll need to introduce a new keybinding command that should indicate that the
key is unbound. We'll load the user keybindings and layer them on the defaults
as described above. If during the deserializing we find an entry that's bound to
the command `"unbound"` or any other string that we don't understand, instead of
trying to _set_ the keybinding, we'll _clear_ the keybinding with a new method
`AppKeyBindings::ClearKeyBinding(chord)`.

### Removing the Globals Object

As a part of #[1005](https://github.com/microsoft/terminal/pull/1005), all the
global settings were moved to their own object within the serialized settings.
This was to try and make the file easier to parse as a user, considering global
settings would be intermingled with profiles, keybindings, color schemes, etc.
Since this change will make the user settings dramatically easier to navigate,
we should probably remove the `globals` object, and have globals at the root
level again.

### Default `profiles.json`

Below is an example of what the default user settings file might look like when
it's first generated, taking all the above points into consideration.

```js
// To view the default settings, open the defaults.json file in this directory
{
    "defaultProfile" : "{574e775e-4f2a-5b96-ac1e-a2962a402336}",
    "profiles": [
        {
            // Make changes here to the cmd.exe profile
            "guid": "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
        },
        {
            // Make changes here to the Windows Powershell profile
            "guid": "{086a83cd-e4ef-418b-89b1-3f6523ff9195}",
        },
        {
            "guid": "{574e775e-4f2a-5b96-ac1e-a2962a402336}",
            "name" : "Powershell Core",
            "commandline" : "pwsh.exe",
            "icon" : "ms-appx:///ProfileIcons/{574e775e-4f2a-5b96-ac1e-a2962a402336}.png",
        }
    ],

    // Add custom color schemes to this array
    "schemes": [],

    // Add any keybinding overrides to this array.
    // To unbind a default keybinding, set the command to "unbound"
    "keybindings": []
}

```

Note the following:
* cmd.exe and powershell.exe are both in the file, as to give users an easy
  point to extend the settings for those default profiles.
* Powershell Core is included in the file, and the default profile has been set
  to its GUID.
* There are a few helpful comments scattered throughout the file to help point
  the user in the right direction.

## UI/UX Design

### Opening `defaults.json`
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

I don't think this will introduce any new security issues that weren't already
present

### Reliability
I don't think this will introduce any new reliability concerns that weren't
already present

### Performance, Power, and Efficiency

By not writing the defaults to disk, we'll theoretically marginally improve the
load and save times for the `profiles.json` file, by simply having a smaller
file to load.

### Accessibility
N/A

## Potential Issues
### Migrating Existing Settings

I believe that existing `profiles.json` files will smoothly update to this
model, without breaking. While in the new model, the `profiles.json` file can be
much more sparse, users who have existing `profiles.json` files will have full
settings in their user settings. We'll leave their files largely untouched, as
we won't touch keys that have the same values as defaults that are currently in
the `profiles.json` file. Fortunately though, users should be able to remove
much of the boilerplate from their `profiles.json` files, and trim it down just
to their modifications.

## Future considerations
* It's possible that a very similar layering loading mechanism could be used to
  layer per-machine settings with roaming settings. Currently, there's only one
  settings file, and it roams to all your devices. This could be problematic,
  for example, if one of your machines has a font installed, but another
  doesn't. A proposed solution to that problem was to have both roaming settings
  and per-machine settings. The code to layer settings from the defaults and the
  user settings could be re-used to handle layer the roaming and per-machine
  settings.
* What if an extension wants to generate their own dynamic profiles? We've
  already outlined a contract that profile generators would have to follow to
  behave correctly. It's possible that we could abstract our implementation into
  a WinRT interface that extensions could implement, and be triggered just like
  other dynamic profile generators.

## Resources
N/A
