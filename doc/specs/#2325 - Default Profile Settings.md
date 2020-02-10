---
author: Mike Griese @zadjii-msft
created on: 2019-11-13
last updated: 2019-12-05
issue id: #2325
---

# Default Profile Settings

## Abstract

Oftentimes, users have some common settings that they'd like applied to all of
their profiles, without needing to manually edit the settings of each of them.
This doc will cover some of the many proposals on how to expose that
functionality to the user in our JSON settings model. In this first document,
we'll examine a number of proposed solutions, as well as state our finalized
design.

## Inspiration

During the course of the pull request review on [#3369], the original pull
request for this feature's implementation, it became apparent that the entire
team has differing opinions on how this feature should be exposed to the user.
This doc is born from that discussion.

## Solution Proposals

The following are a number of different proposals of different ways to achieve
the proposed functionality:

1. [`defaultSettings` Profile object in the global settings](#proposal-1-defaultsettings-profile-object-in-the-global-settings)
2. [`__default__` Profile object in the user's profiles](#proposal-2-__default__-profile-object-in-the-users-profiles)
3. [Change `profiles` to an object with a `list` of profiles and a `defaults`](#proposal-3-change-profiles-to-an-object-with-a-list-of-profiles-and-a-defaults-object)
   object
4. [`inheritFrom` in profiles](#proposal-4-inheritfrom-in-profiles)

### Proposal 1: `defaultSettings` Profile object in the global settings

```json
{
    "$schema": "https://aka.ms/terminal-profiles-schema",
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "defaultSettings":
    {
        "useAcrylic": true,
        "acrylicOpacity": 0.1,
        "fontFace": "Cascadia Code",
        "fontSize": 10
    },
    "requestedTheme" : "dark",
    "showTabsInTitlebar" : true,
    "profiles":
    [
        {
            "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "name": "Windows PowerShell",
            "commandline": "powershell.exe",
            "hidden": false
        },
        {
            "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "name": "cmd",
            "commandline": "cmd.exe",
            "hidden": false
        }
    ],
    "schemes": [],
    "keybindings": []
}
```
#### Benefits

##### Clearly encapsulates the default profile settings
Puts all the default profiles settings in one object. It's immediately obvious
when scanning the file where the defaults are.

##### Simple to understand
There's one object that applies to all the subsequent profiles, and that
object is the `defaultSettings` object.

#### Concerns

##### What do we name this setting?
People were concerned about the naming of this property. No one has a name that
we're quite happy with:

* `defaultSettings`: This kinda seems to conflict conceptually with
  "defaults.json". It's different, but is that obvious?
* `defaultProfileSettings`: Implies "settings of the default profile"
* `defaults`: This kinda seems to conflict conceptually with "defaults.json"
* `baseProfileSettings`: not the worst, but not terribly intuitive
* Others considered with less enthusiasm
    - `profiles.defaults`: people don't love the idea of a `.`, but hey, VsCode does it.
    - `inheritedSettings`
    - `rootSettings`
    - `globalSettings`: again maybe conflicts a bit with other concepts/properties
    - `profileSettings`
    - `profilePrototype`

##### Why is there this random floating profile in the global settings?

Users may be confused about the purpose of this random `Profile` that's in the
globals. What's that profile doing there? Is _it_ the default profile?

### Proposal 2: `__default__` Profile object in the user's profiles
```json
{
    "$schema": "https://aka.ms/terminal-profiles-schema",
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "requestedTheme" : "dark",
    "showTabsInTitlebar" : true,
    "profiles":
    [
        {
            "guid": "__default__",
            "useAcrylic": true,
            "acrylicOpacity": 0.1,
            "fontFace": "Cascadia Code",
            "fontSize": 10
        },
        {
            "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "name": "Windows PowerShell",
            "commandline": "powershell.exe",
            "hidden": false
        },
        {
            "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "name": "cmd",
            "commandline": "cmd.exe",
            "hidden": false
        }
    ],
    "schemes": [],
    "keybindings": []
}
```

#### Benefits
##### Encapsulates the default profile settings
Puts all the default profiles settings in one object. Probably not as clear as
proposal 1, since it could be _anywhere_ in the list of profiles.

##### Groups default profile settings with profiles
In this proposal, the default profile is grouped into the same list of objects
as the other profiles. All the profiles, and the defaults are all under the
`"profiles"` object. Makes sense.

#### Concerns
##### Mysterious `__defaults__` GUID
The only way to _definitively_ identify that this profile is special is by
giving it a constant string. This string is _not_ a guid, which again, would be
obvious.

##### Unintuitive
Adding a profile that has a mysterious `guid` value use that profile as the
"defaults" is _very_ unintuitive. Nothing aside from documentation would
indicate to the user "hey, add this magic profile blob to use as defaults across
all your profiles".

##### Why does this one profile object apply to all the others
It might be unintuitive that one profile from the list of profiles affects all
the others.

### Proposal 3: Change `profiles` to an object with a `list` of profiles and a `defaults` object

```json
{
    "$schema": "https://aka.ms/terminal-profiles-schema",
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "requestedTheme" : "dark",
    "showTabsInTitlebar" : true,
    "profiles":
    {
        "defaults": {
            "useAcrylic": true,
            "acrylicOpacity": 0.1,
            "fontFace": "Cascadia Code",
            "fontSize": 10
        },
        "list":[
            {
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "Windows PowerShell",
                "commandline": "powershell.exe",
                "hidden": false
            },
            {
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "name": "cmd",
                "commandline": "cmd.exe",
                "hidden": false
            }
        ]
    },
    "schemes": [],
    "keybindings": []
}
```

#### Benefits
##### Groups default profile settings with profiles
In this proposal, the default profile is grouped into the same object as the
list of profiles. All the profiles, and the defaults are all under the
`"profiles"` object. Makes sense.

##### Backwards compatible
Fortunately, we can add this functionality _without breaking the existing
schema_. With Jsoncpp, we can determine at runtime if an object is an _array_ or
an _object_. If it's an array, we can fall back to the current behavior, safe in
our knowledge that there's no defaults object. If the object is an array
however, we can then dig into the object to find the default profile and the
list of profiles.

#### Concerns
##### Substantial schema change
This is a pretty big delta to the settings schema. Instead of using `profiles`
as a list of `Profile` objects, it instead becomes an object, with a list inside
it.

As noted above, we could gracefully upgrade this. If the `profiles` object is a
list, then we can assume there's no `defaults`. This ensures that user's current
settings files don't break. This is not a major problem.

##### Adds another level of indentation to all profiles
Some people just hate having things indented this much. 4 layers of indentation
is quite a lot.

### Proposal 4: `inheritFrom` in profiles

```json
{
    "$schema": "https://aka.ms/terminal-profiles-schema",
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "requestedTheme" : "dark",
    "showTabsInTitlebar" : true,
    "profiles":
    [
        {
            "guid": "{11111111-1111-1111-1111-111111111111}",
            "hidden": true,
            "useAcrylic": true,
            "acrylicOpacity": 0.1,
            "fontFace": "Cascadia Code",
            "fontSize": 10
        },
        {
            "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "inheritFrom": "{11111111-1111-1111-1111-111111111111}",
            "name": "Windows PowerShell",
            "commandline": "powershell.exe",
            "hidden": false
        },
        {
            "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "inheritFrom": "{11111111-1111-1111-1111-111111111111}",
            "name": "cmd",
            "commandline": "cmd.exe",
            "hidden": false
        },
        {
            "guid": "{0caa0dad-ffff-5f56-a8ff-afceeeaa6101}",
            "inheritFrom": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "name": "This is another CMD",
            "commandline": "cmd.exe /c myCoolScript.bat",
            "hidden": false
        }
    ],
    "schemes": [],
    "keybindings": []
}
```

#### Benefits

##### Matches the existing settings model without major refactoring
Simply adding a new property to `Profile` would not majorly alter the structure
of the file.

##### Property name is unique
`inheritFrom` is very unique relative to other keys we already have.

##### Powerful
This lets the user have potentially many layers of settings grouping. These
layers would let the user separate out common settings however they like,
without forcing them to a single "default" profile. They could potentially have
many "default" profiles, e.g.
* one that's used for all their WSL profiles, with `startingDirectory` set to
  `~` and `fontFace` set to "Ubuntu Mono"
* One that's used for all their powershell profiles

etc.

#### Concerns

##### GUIDs are not human friendly

Using the guid in the `inheritFrom` field is the only way to be sure we're
uniquely identifying profiles. However, guids are notoriously un-friendly. The
above example manually uses `"{11111111-1111-1111-1111-111111111111}"` as the
guid of the "default" profile, but inheriting from other profiles with "real"
GUIDs would be less understandable. Consider the "This is another CMD" case,
where it's inheriting from the "cmd" profile. That `"inheritFrom"` value does
not mean at a quick glance "cmd".

##### We have to make sure that there are no cycles as we're layering

This is mostly a technical challenge, but this does make the implementation a
bit trickier.

##### How does this work with the settings UI?

When the user edits settings for a profile with the UI, do we only place the
changes in the top-most profile?

How do we communicate in the UI that a profile is inheriting settings from other
profiles?

##### Harder to mentally parse
Maybe not as easy to mentally picture how one profile inherits from another. The
user would probably need to manually build the tree of profile inheritance in
their own head to understand how a profile gets its settings.

## Conclusions

After discussion the available options, the team has settled on proposal 3. The
major selling points being:
* It groups the new "default profile settings" with the rest of the profile
  settings
* While being a schema change, it's not a _breaking_ schema change.
* When looking at the settings, it's easy to understand how they're related

We also like the idea of proposal 4, but felt that it was too heavy-handed of an
approach for this relatively simple feature. It's been added to the backlog of
terminal features, tracked in [#3818].

## Resources

* Default Profile for Common Profile Settings (the original issue) [#2325]
* Add support for "User Default" settings (the original PR) [#3369]
* Add support for inheriting and overriding another profile's settings [#3818]

<!-- Footnotes -->
[#2325]: https://github.com/microsoft/terminal/issues/2325
[#3369]: https://github.com/microsoft/terminal/pull/3369
[#3818]: https://github.com/microsoft/terminal/issues/3818
