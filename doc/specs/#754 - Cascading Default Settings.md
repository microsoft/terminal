---
author: Mike Griese @zadjii-msft
created on: 2019-05-31
last updated: 2019-07-31
issue id: 754
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

It also outlines a mechanism by which profiles could be dynamically added or
hidden from the profiles list, based on some external source.

## Inspiration

Largely inspired by the settings model that both VS Code (and Sublime Text) use.

### Goal: Minimize Re-Serializing `profiles.json`

We want to re-serialize the user settings file, `profiles.json`, as little as
possible. Each time we serialize the file, there's the possibility that we've
re-ordered the keys, as `jsoncpp` provides no ordering guarantee of the keys.
This isn't great, as each write of the file will randomly re-order the file.

One of our overarching goals with this change should be to re-serialize the user
settings file as little as possible.

### Goal: Minimize Content in `profiles.json`

We want the user to only have to make the minimal number of changes possible to
the user settings file. Additionally, the user should only have to have the
settings that they've changed in that file. If the user wants to change only the
`cursorColor` of a profile, they should only need to set that property in the
user settings file, and not need an entire copy of the `Profile` object in their
user settings file. That would create additional noise that's not relevant to
the user.

### Goal: Remove the Need to Reset Settings Entirely to get New Settings
One problem with the current settings design is that we only generate "default"
settings for the user when there's no settings file present at all. So, when we
want to do things like update the default profiles to have an icon, or add
support for generating WSL profiles, it will only apply to users for fresh
installs. Otherwise, a user needs to completely delete the settings file to have
the terminal re-generate the default settings.

This is fairly annoying to the end-user, so ideally we'll find a way to be able
to prevent this scenario.

### Goal: Prevent Roaming Settings from Failing
Another problem currently is that when settings roam to another machine, it's
possible that the second machine doesn't have the same applications installed as
the first, and some profiles might be totally invalid on the second machine.
Take for example, profiles for WSL distros. If you have and Ubuntu profile on
your first machine, and roam that profile to a second machine without Ubuntu
installed, then the Ubuntu profile would be totally broken on the second
machine.

While we won't be able to non-destructively prevent all failures of this case,
we should be able to catch it in certain scenarios.

## Solution Design

The settings are now composed from two files: a "Default" settings file, and a
"User" settings file.

When we load the settings, we'll perform the following steps, each mentioned in
greater detail below:
1. Load from disk the `defaults.json` (the default settings) -> DefaultsJson
1. Load from disk the `profiles.json` (the user settings) -> UserJson
1. Parse DefaultsJson to create all the default profiles, schemes, keybindings.
1. [Not covered in this spec] Check the UserJson to find the list of dynamic
   profile sources that should run.
1. Run all the _enabled_ dynamic profile generators. Those profiles will be
   added to the set of profiles.
   - During this step, check if any of the profiles added here don't exist in
     UserJson. If they _don't_, the generator created a profile that didn't
     exist before. Return a value indicating the user settings should be
     re-saved (with the new profiles added).
1. [Not covered in this spec] Layer the UserJson.globals.defaults settings to
   every profile in the set, both the defaults, and generated profiles.
1. Apply the user settings from UserJson. Layer the profiles on top of the
   existing profiles if possible (if both `guid` and `source` match). If a
   profile from the user settings does not already exist, make sure to apply the
   UserJson.globals.defaults settings first. Also layer Color schemes and
   keybindings.
   - If a profile has a `source` key, but there is not an existing profile with
     a matching `guid` and `source`, don't create a new Profile object for it.
     Either that generator didn't run, or the generator wanted to delete that
     profile, so we'll effectively hide the profile.
1. Re-order the list of profiles, to match the ordering in the UserJson. If a
   profile doesn't exist in UserJson, it should follow all the profiles in the
   UserJson. If a profile listed in UserJson doesn't exist, we can skip it
   safely in this step (the profile will be a dynamic profile that didn't get
   populated.)
1. Validate the settings.
1. If requested in step 5, write the modified settings back to `profiles.json`.

### Default Settings

We'll have a static version of the "Default" file **hardcoded within the
application package**. This `defaults.json` file will live within the
application's package, which will prevent users from being able to edit it.

```json
// This is an auto-generated file. Place any modifications to your settings in "profiles.json"
```

This disclaimer will help identify that the file shouldn't be modified. The file
won't actually be generated, but because it's shipped with our app, it'll be
overridden each time the app is updated. "Auto-generated" should be good enough
to indicate to users that it should not be modified.

Because the `defaults.json` file is hardcoded within our application, we can use
its text directly, without loading the file from disk. This should help save
some startup time, as we'll only need to load the user settings from disk.

When we make changes to the default settings, or we make changes to the settings
schema, we should make sure that we update the hardcoded `defaults.json` with
the new values. That way, the `defaults.json` file will always have the complete
set of settings in it.

### Layering settings

When we load the settings, we'll do it in three stages. First, we'll deserialize
the default settings that we've hardcoded. We'll then generate any profiles that
might come from dynamic profile sources. Then, we'll intelligently layer the
user's setting upon those we've already loaded. If a user wants to make changes
to some objects, like the default profiles, we'll need to make sure to load from
the user settings into the existing objects we created from the default
settings.

* We'll need to make sure that any profile in the user settings that has a GUID
  matching a default profile loads the user settings into the object created
  from the defaults.
* We'll need to make sure that there's only one action bound to each key chord
  for a keybinding. If there are any key chords in the user settings that match
  a default key chord, we should bind them to the action from the user settings
  instead.
* For any color schemes whose name matches the name of a default color scheme,
  we'll need to apply the user settings to the existing color scheme. For
  example, a user could override the `red` entry of the "Campbell" scheme to be
  `#ff9900` if they want. This would then apply to all profiles using the
  "Campbell" scheme.
* For profiles that were created from a dynamic profile source, they'll have
  both a `guid` and `source` guid that must _both_ match. If a user profile with
  a `source` set does not find a matching profile at load time, the profile will
  be ignored. See more details in the [Dynamic Profiles](#dynamic-profiles)
  section.

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

### Serializing User Settings

How can we tell that a setting should be written back to the user settings file?

If the value of the setting isn't the same as the defaults, then it could easily
be added to the user's `profiles.json`. We'll have to do a smart serialization
of the various settings models. We'll pass in the default version **of that
model** during the serialization. If that object finds that a particular setting
is the same as a default setting, then we'll skip serializing it.

What happens if a user has chosen to set the value to _coincidentally_ the same
value as the default value? We should keep that key in the user's settings file,
even though it is the same.

In order to facilitate this, we'll need to keep the originally parsed user
settings around in memory. When we go to serialize the settings, we'll check if
either the setting exists already in the user settings file, or the setting has
changed. If either is true, then we'll make sure to write that setting back out.

For serializing settings for the default profiles, we'll check if the setting is
in the user settings file, or if the value of the setting is different from the
version of that `Profile` from the default settings. For user-created profiles,
we'll compare the value of the setting with the value of the _default
constructed_ `Profile` object. This will help ensure that each profile in the
user's settings file maintains the minimal amount of info necessary.

When we're adding profiles due to their generation in a dynamic profile
generator, we'll need to serialize them, then insert them back into the
originally parsed json object to be serialized. We don't want the automatic
creation of a new profile to automatically trigger re-writing the entire user
settings file, but we do want newly created dynamic profiles to have an entry
the user can easily edit.

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
won't get any of these dynamic profiles. Furthermore, if any of the sources of
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

Additionally, each dynamic profile generator **must have a unique source guid**
to associate with the profile. When a dynamic profile is generated, the source's
guid will be added to the profile, to make sure the profile is correlated with
the source it came from.

We'll generate these dynamic profiles immediately after parsing the default
profiles and settings. When a generator runs, it'll be able to create unique
profile GUIDs for each source it wants to generate a profile for. It'll hand
back a list of Profile objects, with settings set up how the generator likes,
with GUIDs set.

After a dynamic profile generator runs, we will determine what new profiles need
to be added to the user settings, so we can append those to the list of
profiles. The deserializer will look at the list of generated profiles and check
if each and every one already has a entry in the user settings. The generator
will just blind hand back a list of profiles, and the deserializer will figure
out if any of them need to be added to the user settings. We'll store some sort
of result indicating that we want a save operation to occur. After the rest of
the deserializing is done, the app will then save the `profiles.json` file,
including these new profiles.

When we're serializing the settings, instead of comparing a dynamic profile to
the default-constructed `Profile`, we'll compare it to the state of the
`Profile` after the dynamic profile generator created it. It'd then only
serialize settings that are different from the auto-generated version. It will
also always make sure that the `guid` of the dynamic profile is included in the
user settings file, as a point for the user to add customizations to the dynamic
profile to. Additionally, we'll also make sure the `source` is always serialized
as well, to keep the profile correlated with the generator that created it.

We'll need to keep the state of these dynamically generated profiles around in
memory during runtime to be able to ensure the only state we're serializing is
that which is different from the initially generated dynamic profile.

When the generator is run, and determines that a new profile has been added,
we'll need to make sure to add the profile to the user's settings file. This
will create an easy point for users to customize the dynamic profiles. When
added to the user settings, all that will be added is the `name`, `guid`, and
`source`.

Additionally, a user might not want a dynamic profile generator to always run.
They might want to keep their Azure connections visible in the list of profiles,
even if its no longer a valid target. Or they might want to not automatically
connect to Azure to find new instances every time they launch the terminal. To
enable scenarios like this, we'll add an additional setting,
`disabledProfileSources`. This is an array of guids. If any guids are in that
list, then those dynamic profile generators _won't_ be run, suppressing those
profiles from appearing in the profiles list.

If a dynamic profile generator needs to "delete" a profile, this will also work
naturally with the above rules. Lets examine the case where the user has
uninstalled the Ubuntu distro. When the WSL generator runs, it won't create the
Ubuntu profile. When we get to the Ubuntu profile in the user's settings, it'll
have a `source`, but we won't already have a profile with that `guid` and
`source`. So we'll just ignore it, because whatever source for that profile
doesn't want it anymore. Effectively, this will act like it was "deleted",
though the artifacts still remain untouched in the user's json.

#### What if a dynamic profile is removed, but it's the default?

I'll direct our attention to [#1348] - Display a specific error for not finding
the default profile. When we're done loading, and we determine that the default
profile doesn't exist in the finalized list of profiles, we'll display a dialog
to the user. This includes both hidden profiles and dynamic profiles that have
been "deleted". We'll temporarily use the _first_ profile instead.

#### Dynamic profile GUID generation

In order to help facilitate the generation of stable, unique GUIDs for
dynamically generated profiles, we'll enforce a few methods on each generator.
The Generator should implement a method that returns its _unique_ namespace for
profiles it generates:

```c++
class IDynamicProfileGenerator
{
    ...
    virtual std::wstring GetNamespace() = 0;
    ...
}
```

For example, the WSL generator would return `Microsoft.Terminal.WSL`. The
Powershell Core generator would return `Microsoft.Terminal.PowershellCore`.
We'll use these names to be able to generate uuidv5 GUIDs that will be unique
(so long as the names are unique).

The generator should also be able to ask the app for two other pieces of
functionality:
* The generator should be able to ask the app for the generator's own namespace
  GUID
* The generator should be able to ask the app for a uuidv5 in the generator's
  namespace, given a specific name key.

These two functions will be exposed to the generator like so:

```c++
GUID GetNamespaceGuid(IDynamicProfileGenerator& generator);
GUID GetGuidForName(IDynamicProfileGenerator& generator, std::wstring& name);
```

The generator does not _need_ to use `GetGuidForName` to generate guids for it's
profiles. If the generator can determine another way to generate stable GUIDs
for its profiles, it's free to use whatever method it wants. `GetGuidForName` is
provided as a convenience.

It's not the responsibility of the dynamic profile generator to fill in the
`source` of the profiles it generates. The deserializer will make sure to go
through and fill in the guid for the generated profiles given the generator's
namespace GUID.

### Powershell Core & the Defaults

How do we handle the potential existence of Powershell Core in this model?
Powershell core is unique as far as the default profiles goes - it may or may
not exist on the user's system. Not only that, but depending on the user's
install of Powershell Core, it might have a path in either `Program Files` or
`Program Files(x86)`.

Additionally, if it _is_ installed, we set it as the default profile instead of
Windows Powershell.

Powershell core acts much like a dynamic profile. It has an installation source
that may or not be there. So we'll add a dynamic profile generator for
Powershell Core. This will automatically create a profile for Powershell Core if
necessary.

Unlike the other dynamic profiles, if Powershell Core is present on
_first_ launch of the terminal, we set that as the default profile. This can
still be done - we'll need to do some special-case work when we're loading the
user settings and we _don't_ find any existing settings. When that happens,
we'll generate all the default user settings. Before we commit them, we'll check
if the Powershell Core profile exists, and if it does, we'll set that as the
default profile before writing the settings to disk.

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
// To view the default settings, open <path-to-app-package>\defaults.json
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
            "source": "{2bde4a90-d05f-401c-9492-e40884ead1d8}",
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
  to its GUID. The `source` has been set, indicating that it came from a dynamic profile source.
* There are a few helpful comments scattered throughout the file to help point
  the user in the right direction.

### Re-ordering profiles

Since there are shortcuts to open the Nth profile in the list of profiles, we
need to expose a way for the user to change the order of the profiles. This was
not a problem when there was only a single list of profiles, but if the defaults
are applied _first_ to the list of profiles, then the user wouldn't be able to
change the order of the default profiles. Additionally, any profiles they add
would _always_ show up after the defaults.

To remedy this, we could scan the user profiles in the user settings first, and
create `Profile` objects for each of those profiles first. These `Profile`s
would only be initialized with their GUID temporarily, but they'd be placed into
the list of profiles in the order they appear in the user's settings. Then, we'd
load all the default settings, overlaying any default profiles on the `Profile`
objects that might already exist in the list of profiles. If there are any
default profiles that don't appear in the user's settings, they'll appear
_after_ any profiles in the user's settings. Then, we'll overlay the full user
settings on top of the defaults.

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

There could alternatively be a hidden option for the "Open Settings" button,
where holding <kbd>Alt</kbd> while clicking on the button would open the
`defaults.json` instead.

We could additionally add a `ShortcutAction` (to be bound to a keybinding) that
would `openDefaultSettings`, and we could bind that to
<kbd>ctrl</kbd>+<kbd>alt</kbd>+<kbd>\`</kbd>, similar to `openSettings` on
<kbd>ctrl</kbd>+<kbd>\`</kbd>.

### How does this work with the settings UI?

If we only have one version of the settings models (Globals, Profiles,
ColorShemes, Keybindings) at runtime, and the user changes one of the settings
with the settings UI, how can we tell that settings changed?

Fortunately, this should be handled cleanly by the algorithm proposed above, in
the "Serializing User Settings" section. We'll only be serializing settings that
have changed from the defaults, so only the actual changes they've made will be
persisted back to the user settings file.

## Capabilities
### Security

I don't think this will introduce any new security issues that weren't already
present

### Reliability
I don't think this will introduce any new reliability concerns that weren't
already present. We will likely improve our reliability, as dynamic profiles
that no longer exist will not cause the terminal to crash on startup anymore.

### Performance, Power, and Efficiency

By not writing the defaults to disk, we'll theoretically marginally improve the
load and save times for the `profiles.json` file, by simply having a smaller
file to load. However we'll also be doing more work to process the layering of
defaults and user settings, which will likely slightly increase the load times.
Overall, I expect the difference to be negligible due to these factors.

One potential concern is long-running dynamic profile generators. Because
they'll need to run on startup, they could negatively impact startup time. You
can read more below, in "Dynamic Profile Generators Need to be Enabled".

### Accessibility
N/A

## Potential Issues

### Profiles with the same `guid` as a dynamic profile but not the same `source`

What happens if the User settings has a profile with a `guid` that matches a
dynamic or default profile, but the user profile doesn't have a matching source?
This could happen trivially easily if the user deletes the `source` key from a
profile that has dynamically generated.

We could:
1. Treat the profile as an entirely separate profile
   - There's lots of other code that assumes each profile has only a unique GUID,
     so we'd have to change the GUID of this profile. This would mean writing out
     the user settings, which we'd like to avoid.
   - We'll still end up generating the entry for the dynamic profile in the
     user's settings, so we'll need to write out the user settings anyways.
   - This other profile will likely not have a commandline set, so it might not
     work at all.
1. Ignore the profile entirely.
   - When the dynamic profile generator runs, we're not going to find another
     entry in the user profiles with both a matching `guid` and a matching
     `source`. So we'll end up creating _another_ entry in the user profiles for
     the dynamic profile.
   - How could the user know that the profile is being ignored? There's nothing
     in the file itself that indicates obviously that this profile is now
     invalid.
1. Treat the user settings as part of the dynamic profile
   - In this scenario, the user profile continues to exist as part of the dynamic profile.
   - When the dynamic profile generator runs, we're not going to find another
     entry in the user profiles with both a matching `guid` and a matching
     `source`. So we'll end up creating _another_ entry in the user profiles for
     the dynamic profile.
     - These two entries will each be layered upon the dynamically generated
       profile, so the settings in the second profile entry will override
       settings from the first.
   - If the user disables the generator, or the profile source is removed, the
     dynamic profile will cease to exist. However, the profile without the
     `source` entry will remain, though likely will not work.
   - How do we order these profiles for the user? When we're parsing the user
     profiles list to build an ordering of profiles, do we use the first entry as
     the index for that profile?
1. (Variant of the above) Treat the profile as part of the dynamic profile, and
   re-insert the `source` key.
   - This will re-connect the user profile to the dynamic one.
   - We'll need to make sure to do this before determining the new dynamic
     profiles to add to the user settings.
   - Given all the scenarios are going to cause a user settings write anyways,
     this isn't terrible.
   - If the user _really_ wants to split the profile in their user settings from
     the dynamic one, they're free to always generate a new guid _and_ delete the
     `source` key.

Given the drawbacks associated with options 1-3, I propose we choose option 4 as
our solution to this case.

### Migrating Existing Settings

I believe that existing `profiles.json` files will smoothly update to this
model, without breaking. While in the new model, the `profiles.json` file can be
much more sparse, users who have existing `profiles.json` files will have full
settings in their user settings. We'll leave their files largely untouched, as
we won't touch keys that have the same values as defaults that are currently in
the `profiles.json` file. Fortunately though, users should be able to remove
much of the boilerplate from their `profiles.json` files, and trim it down just
to their modifications.

#### Migrating Powershell Core

Right now, default-generated Powershell Core profiles exist with a stable guid
we've generated for them. However, when we move Powershell Core to being a
dynamically generated profile, we'll have to ensure that we don't create a
duplicated "dynamic" entry for that profile. If we want to convert the existing
Powershell Core profiles into a dynamic profile, we'll need to make sure to add
a `source` key to the profile. Everything else in the profile can remain the
same. Once the `source` is added, we'll know to treat it as a dynamic profile,
and it'll respond dynamically.

This is actually something that will automatically be covered by the scenario
mentioned above in "Profiles with the same `guid` as a dynamic profile but not
the same `source`". When we encounter the existing Powershell Core profiles that
don't have a `source`, we'll automatically think they're the dynamically
generated ones, and auto-migrate them.

#### Migrating Existing WSL Profiles

Similar to the above, so long as we ensure the WSL dynamic profile generator
generates the _same_ GUIDs as it does currently, all the existing WSL profiles
will automatically be migrated to dynamic profiles.

### Dynamic Profile Generators Need to be Enabled
With the current proposal, profiles that are generated by a dynamic profile
generator _need_ that generator to be enabled for the profile to appear in the
list of profiles. If the generator isn't enabled, then the important parts of
the profile (name, commandline) will never be set, and the profile's settings
from the user settings will be ignored at runtime.

For generators where the generation of profiles might be a lengthy process, this
could negatively impact startup time. Take for example, some hypothetical
generator that needs to make web requests to generate dynamic profiles. Because
we need the finalized settings to be able to launch the terminal, we'll be stuck
loading until that generator is complete.

However, if the user disables that generator entirely, we'll never display that
profile to the user, even if they've done that setup before.

So the trade-off with this design is that non-existent dynamic profiles will
never roam to machines where they don't exist and aren't valid, but the
generators _must_ be enabled to use the dynamic profiles.

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
* **Multiple settings files** - This could enable us to place color schemes into
  a seperate file (like `colorschemes.json`) and put keybindings into their own
  file as well, and reduce the number of settings in the user's `profiles.json`.
  It's unclear if this is something that we need quite yet, but the same
  layering functionality that enables this scenario could also enable more than
  two sources for settings.
* **Global Default Profile Settings** - Say a user wants to override what the
  defaults for a profile are, so that they can set settings for _all_ their
  profiles at once? We could maybe introduce a profile in the user settings file
  with a special guid set to `"default`, that we look for first, and treat
  specially. We wouldn't include it in the list of profiles. When we're creating
  profiles, we'll start with that profile as our prototype, instead of using the
  default-constructed `Profile`. When we're serializing profiles, we'd again use
  that as the point of comparison to check if a setting's value has changed.
  There may be more unknowns with this proposal, so I leave it for a future
  feature spec.
  - We'll also want to make sure that when we're serializing default/dynamic
    profiles, we take into account the state from the global defaults, and we
    don't duplicate that information into the entries for those types of profiles
    in the user profiles.
* **Re-ordering profiles** - Under "Solution Design", we provide an algorithm
  for decoding the settings. One of the steps mentioned is parsing the user
  settings to determine the ordering of the profiles. It's possible in the
  future we may want to give the user more control over this ordering. Maybe
  we'll want to allow the user to manually index the profiles. Or, as discussed
  in issues like #1571, we may want to allow the user to further customize the
  new tab dropdown, beyond just the order of profiles. The re-ordering step
  would be a great place to add code to support this re-ordering, with whatever
  algorithm we eventually land on. Determining such an algorithm is outside the
  scope of this spec, however.

## Resources
N/A


<!-- Footnotes -->

[#1348]: https://github.com/microsoft/terminal/issues/1348
