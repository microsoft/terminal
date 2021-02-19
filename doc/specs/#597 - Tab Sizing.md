---
author: Kayla Cinnamon @cinnamon-msft
created on: 2020-01-03
last updated: 2020-01-03
issue id: 597
---

# Tab Sizing

## Abstract

This spec outlines the tab sizing feature. This is an application-level feature that is not profile-specific (at least for now).

Global properties that encompass tab sizing:

* `tabWidthMode` (accepts pre-defined values for tab sizing behavior)
* `tabWidthMin` (can never be smaller than the icon width)
* `tabWidthMax` (can never be wider than the tab bar)

Acceptable values for `tabWidthMode`:

* [default] `equal` (all tabs are sized the same, regardless of tab title length)
* `titleLength` (width of tab contains entire tab title)

## Inspiration

Other browsers and terminals have varying tab width behavior, so we should give people options.

## Solution Design

`tabWidthMode` will be a global setting that will accept the following strings:

* `equal`
    * All tabs are equal in width
    * If the tab bar has filled, tabs will shrink as additional tabs are added
    * Utilizes the `equal` setting from WinUI's TabView

* `titleLength`
    * Tab width varies depending on title length
    * Width of tab will fit the whole tab title
    * Utilizes the `sizeToContent` setting from WinUI's TabView

In addition to `tabWidthMode`, the following global properties will also be available:

* `tabWidthMin`
    * Accepts an integer
    * Value correlates to the minimum amount of pixels the tab width can be
    * If value is less than the width of the icon, the minimum width will be the width of the icon
    * If value is greater than the width of the tab bar, the maximum width will be the width of the tab bar
    * If not set, the tab will have the system-defined minimum width

* `tabWidthMax`
    * Accepts an integer
    * Value correlates to the maximum amount of pixels the tab width can be
    * If value is less than the width of the icon, the minimum width will be the width of the icon
    * If value is greater than the width of the tab bar, the maximum width will be the width of the tab bar
    * If not set, the tab will have the system-defined maximum width

If `tabWidthMode` is set to `titleLength`, the tab widths will fall between the `tabWidthMin` and `tabWidthMax` values if they are set, depending on the length of the tab title.

If `tabWidthMode` isn't set, the default experience will be `equal`. Justification for the default experience is the results from this [twitter poll](https://twitter.com/cinnamon_msft/status/1203093459055210496).

## UI/UX Design

[This tweet](https://twitter.com/cinnamon_msft/status/1203094776117022720) displays how the `equal` and `titleLength` values behave for the `tabWidthMode` property.

## Capabilities

### Accessibility

This feature could impact accessibility if the tab title isn't stored within the metadata of the tab. If the tab width is the width of the icon, then the title isn't visible. The tab title will have to be accessible by a screen reader.

### Security

This feature will not impact security.

### Reliability

This feature will not impact reliability. It provides users with additional customization options.

### Compatibility

This feature will not break existing compatibility.

### Performance, Power, and Efficiency

## Potential Issues

This feature will not impact performance, power, nor efficiency.

## Future considerations

* Provide tab sizing options per-profile
* A `tabWidthMode` value that will evenly divide the entirety of the tab bar by the number of open tabs
    * i.e. One tab will take the full width of the tab bar, two tabs will each take up half the width of the tab bar, etc.
