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

Should the settings schema ever change, the defaults file will change, without needing to
re-write the user's settings file.


## Inspiration

Largely inspired by the settings model that both VS Code (and Sublime Text) use.

## Solution Design

[comment]: # Outline the design of the solution. Feel free to include ASCII-art diagrams, etc.

## UI/UX Design

[comment]: # What will this fix/feature look like? How will it affect the end user?

## Capabilities

[comment]: # Discuss how the proposed fixes/features impact the following key considerations:

### Security

### Reliability

### Performance, Power, and Efficiency

### Accessibility

[comment]: # Be sure to carefully consider how this feature may be affect, impact, help, or hinder accessibilty.

## Potential Issues

[comment]: # What are some of the things that might cause problems with the fixes/features proposed? Consider how the user might be negatively impacted.

## Future considerations

[comment]: # What are some of the things that the fixes/features might unlock in the future? Does the implementation of this spec enable scenarios?

## Resources

[comment]: # Be sure to add links to references, resources, footnotes, etc.
