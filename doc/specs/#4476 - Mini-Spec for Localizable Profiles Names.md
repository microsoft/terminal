---
author: Dustin L. Howett (@DHowett-MSFT)
created on: 2020-04-07
last updated: 2020-04-07
issue id: #4476
---

# Localizable Profile Names

## Abstract

One of the profiles we ship by default, "cmd", should be named "Command
Prompt". Unfortunately, "Command Prompt" is a localizable name in Windows. If
we ship with it as-is, we will be breaking with the display name used for cmd
in Windows.

It would be, broadly-speaking, possible to generically localize the entire
`defaults.json` file and the user settings template. To do so would add untold
burden on the localization team as they would have to maintain the structure of
the file, and the Terminal engineering team as we would need to make sure that
all localized copies of `defaults.json` and the user settings template match in
non-string content.

If we want to properly localize profile names (and other resources that will
eventually be used in our JSON settings), we will need to support resource
loading from JSON.

## Solution Design

I propose that we augment our JSON object model deserializer to support
_objects containing the `resourceKey` member_ and treat them as resource key
lookups at the time of loading.

We would then use this functionality when deserializing profile names.

### Profiles, Before

```json
{
    "name": "Command Prompt" // not localizable.
}
```

### Profiles, After

```json
{
    "name": { "resourceKey": "CommandPromptDisplayName" }
}
```

## Capabilities

### Accessibility

Localizable names are inherently more accessible.

### Security

A user would be able to set, as a display string, any resource from the
Terminal application resource compartment to appear as a profile name. Given
that all of our strings are intended for UI use, this does not represent a
significant concern.

### Reliability

This will not impact reliability.

### Compatibility

A change in profile name, especially one that happens after the product ships
(due to a changing i18n setting on the system), will impact users accustomed to
`wt -p profile`. I have no answer for how best to resolve this, except "get
used to it".

Since we also emit the name `cmd` in the user's settings template, we will want
to either remove that field (move it to a comment) or switch it to
`resourceKey` as well.

Existing users will not see localization changes in default profiles, as their
settings templates included hardcoded `name` fields.

### Performance, Power, and Efficiency

## Potential Issues

As above in Compatibility.

We need a fallback in case a resource cannot be found.

## Future considerations

If we allow the command palette's contents to be driven through JSON
configuration, localized resource strings will become important.
