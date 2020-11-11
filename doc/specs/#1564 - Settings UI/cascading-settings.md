---
authors: Carlos Zamora (@carlos-zamora) and Kayla Cinnamon (@cinnamon-msft)
created on: 2020-11-10
last updated: 2020-11-10
issue id: 1564
---

# Cascading Settings

## Abstract

Windows Terminal's settings model adhere's to a cascading settings architecture. This allows a settings object to be defined incrementally across multiple layers of declarations. The value for any global setting like `copyOnSelect`, for example, is set to your settings.json value if one is defined, otherwise defaults.json, and otherwise a system set value. Profiles in particular are more complicated in that they must also take into account the values in `profiles.defaults` and dynamic profile generators.

This spec explores how to represent this feature in the Settings UI.

## Inspiration

Cascading settings (and `profiles.defaults` by extension) provides some major benefits:
1. opt-in behavior for settings values provided in-box (i.e. reset to default)
2. easy way to apply a setting to all your Profiles
3. (future feature) simple way to base a Profile off of another Profile

The following terminal emulators approach this issue as follows.
| Terminal Emulator(s) | Relevant Features/Approach |
|--|--|
| ConEmu, Cmder | "Clone" a separate Profile |
| Fluent Terminal | "Restore Defaults" button on each page |
| iTerm2 | "Bulk Copy from Selected Profile..." and "Duplicate Profile" |

Other Settings UIs have approached this issue as follows:
| Project | Relevant Approach |
|--|--|
| Visual Studio | Present a dropdown with your options. An extra "\<inherit\>" option is shown to inherit a value from another place. |

## Solution Design

The XAML implementation will consist of introducing a `ContentControl` for each setting. The `ContentControl` simply wraps the XAML control used for a setting, then adds the chosen UI approach below.

The `ContentControl` will take advantage of the following TerminalSettingsModel APIs for each setting:
```c++
// Note: String and "Name" are replaced for each setting
bool HasName();
void ClearName();
String Name();
void Name(String val);
```

## UI/UX Design Proposals

### Full Parity with JSON

#### 1: Inobstructive text under a setting control

#### 2: <inherit> option

#### 3: Lock & Key Button

### Partial Parity with JSON

#### Add New --> Duplicate/Clone/Copy Profile

#### Reset Profile button

#### "Apply to all profiles" button

## Capabilities

[comment]: # Discuss how the proposed fixes/features impact the following key considerations:

### Accessibility

[comment]: # How will the proposed change impact accessibility for users of screen readers, assistive input devices, etc.

### Security

[comment]: # How will the proposed change impact security?

### Reliability

[comment]: # Will the proposed change improve reliability? If not, why make the change?

### Compatibility

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?

### Performance, Power, and Efficiency

## Potential Issues

[comment]: # What are some of the things that might cause problems with the fixes/features proposed? Consider how the user might be negatively impacted.

## Future considerations

[comment]: # What are some of the things that the fixes/features might unlock in the future? Does the implementation of this spec enable scenarios?

## Resources

[comment]: # Be sure to add links to references, resources, footnotes, etc.
