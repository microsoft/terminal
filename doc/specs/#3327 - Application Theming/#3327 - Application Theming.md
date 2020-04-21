---
author: <first-name> <last-name> <github-id>/<email>
created on: <yyyy-mm-dd>
last updated: <yyyy-mm-dd>
issue id: #3327
---

# Spec Title

## Abstract

This spec outlines how the Windows Terminal will enable users to create custom
"themes" for the application, enabling further customization of the window.
These themes will be implemented as objects containing a group of UI-specific
properties, so users can quickly apply a group of properties atomically.

## Inspiration

Much of the inspiration for this feature comes from VsCode and its themes. These
themes can be more than just different color palettes for the editor - these
themes can control the appearance of a variety of UI elements of the VsCode
window.


## Solution Design

### Requested Functionality ("User Stories")

The following is a long list of ideas of elements of the window that the user
should be able to customize:

* [ ] Pane Border colors (both the background, and the "focused" color) (#3061)
* [ ] Pane border width (#3062)
* [ ] Tab _Row_ and _Item_ Background color (#702/#1337/#2994/#3774/#1963)
    - Some users want to set these to the accent color
    - Some users want to set these to a specific custom color
    - Some users want this to use the color straight from the active Terminal,
      allowing the tab or titlebar to "blend into" the terminal
* [ ] Feature Request: Setting to hide/remove close ("x") button from tabs (#3335)
* [ ] Various different tab sizing modes
    - the current sizing, which is `SizeToContent`
    - Setting a min/max width on tabs
    - Configuring tabs to split the available space

Other lower-priority ideas:
* [ ] Enable hiding the tab icon altogether
* [ ] Enable forcing tab icons to monochrome
* [ ] Tab row height
* [ ] Tab row font size, font face
* [ ] Tab corner radius
* [ ] Margin between tabs? Padding within the tab?
* [ ] Left-justify / Center / right-justify tab text, when tabs are wider than
  their text?
* [ ] Control colors for light vs dark vs high-contrast modes
* [ ] Enable/disable a shadow underneath the tab row, between tabs and content
* [ ] Enable/disable a shadow cast by terminals on pane borders or a shadow cast
  by pane borders on Terminal panes
* [ ] Similarly to the tabs, styling the Status Bar (#3459)
    - Maybe enable it to have the same color as the active TermControl, causing
      the same "seamless" effect (see [this
      comment](https://github.com/microsoft/terminal/issues/3459#issuecomment-550501577))
    - Change font size, face, colors
    - Control the borders on the status bar - no top border would give the
      impression it's "seamless"

Additionally, the user should be able to easily switch from one installed theme
to another. The user should be able to copy a simple blob of settings from the
web and paste it into their settings to be able to easily try the theme out.

### Difference between "Themes" and "Schemes"

The avid follower of the Windows Terminal might know that the Terminal already
contains support for "color schemes". What makes themes different from these
schemes, and why should they be separate objects?

**Color Schemes** are objects that generally control the appearance of the
Terminal Control itself (the proverbial "black rectangle with text in it").
Primarily, color schemes are used for setting the "color table" of a terminal
instance, setting the values for each of the 16 colors in the terminal's color
table, and the default foreground and background colors. These are properties
that only apply to the contents of the terminal itself, and not necessarily the
entire application. Individual terminal control instances can have different
color schemes. Furthermore, these schemes are largely in-line with schemes
available on other platform's terminals. These schemes were hevily inspired by
the great work done at [iTerm2-Color-Schemes].

Alternatively, **Themes** are sets of properties that apply primarily to the
window of the application itself, but not necessarily the terminal content.
These properties apply globally to the entire window, as opposed to controlling
the appearance of individual terminals. These properties include things such as
the coloration and styling of the tabs in the tab row.

### Theme objects

Themes will be implemented largely similar to the color schemes implementation.
Currently, the terminal contains a list of available color schemes, and profiles
can chose to apply a scheme from the list of schemes. We'll add a list of
`themes`, and globally, the user will be able to specify one of these themes to
apply.

Take for example the following settings exerpt:

```json
{
    "applicationTheme": "My Boxy Theme",
    "themes": [
        {
            "name": "My Boxy Theme",
            "requestedTheme": "dark",
            "tab.radius": 0,
            "tab.padding": 5,
            "tab.background": "terminalBackground",
            "tab.textColor": "key:SystemAccentColorLight3",
            "tab.icon": "outline",
            "tab.closeButton": "hidden",
            "tabRow.background": "accent",
            "tabRow.shadows": false
        },
        {
            "name": "My small light theme",
            "tabBackground": "#80ff0000",
            "tabRowBackground": "#ffffffff",
            "tabHeight": 8,
            "requestedTheme": "light",
            "colorSheme": "Solarized Light",
            "tabIcon": "hidden",
            "tabCloseButton": "hover"
        }
    ]
}
```

> **FOR DISCUSSION**: I've given both a `tab.<property>` and a `tab<Property>`
> style here, for comparison. At the time of writing, I'm unsure of which is
> better. I'll be using `<element>.<property>` for the remainder of the doc.

In the above settings snippet, we see the following things:
1. A list of `themes` that the user can pick from. Each theme has a `name`
   property used to identify the theme, and a group of properties for the theme.
2. The user has set the `applicationTheme` to `"My Boxy Theme"`, the first theme
   in the list of themes. If the user wanted to switch to the other installed
   theme, `"My small light theme"`, they'd simply need to change this property.

### Exposed theme properties

Themes should be able to control a variety of elements of the Terminal UI. Some
of these settings will be easier to implement than others. As such, below is a
set of properties that seems appropriate to include as part of a "v1" theming
implementation. In [Future Considerations](#future-considerations), we'll
enumerate additional properties that could be added in the future to further
control the UI.

#### Theming v1 Properties

* `tab.cornerRadius`: Control the radius of the corners of the tab items.
  Accepts a `double`. If this is set to `0`, then the tabs will have squared-off
  corners. No particular limit is set on the max value accepted, though larger
  values might not be aesthetically pleasing.
* `tab.closeButton`: Control the visibility of the close button for a tab item.
  Accepts the following values:
    - `visible`: The default behavior of the tab item close button - always
      visible.
    - `hover`: The close button on a tab item only appears when the tab is
      hovered.
    - `hidden`: The close button on a tab is always hidden.
* `tab.icon`: Control the visibility, appearance of the tab icon
    - `visible`: The default behavior of the tab item icon - always visible, and
      in full color.
    - `outline`: The icon is always visible, but is only drawn as an outline,
      using `BitmapIconSource.ShowAsMonochrome(true)`
    - `hidden`: The icon is hidden
* `tab.background`: Control the color of the background of tab items. See below
  for accepted colors.
* `tabRow.background`: Control the color of the background of the tab row items.
  See below for accepted colors.
* `pane.borderColor`: Control the color of the border used to seperate panes.
  This is the color of the inactive border between panes.
* `pane.activeBorderColor`: Control the color of the border of the active pane
* `pane.borderWidth`: Control the width of the borders used to seperate panes.
* `window.requestedTheme`: If set, will override the `requestedTheme` property
  (currently in the globals), to manually control the `RequestedTheme` of the
  application window. This controls whether WinUI displays elements in the
  `light`, `dark` or `system` theme.
    - DISCUSSION: Naming for this property?

#### Theme Colors

For properties like `tab.background` and `tabRow.background`, these colors can
be one of:
* an `#rrggbb`, `#aarrggbb` color. (Alpha is ignored for `tabRow.background`)
* `accent` for the accent color
* `terminalBackground` to use the default background color of the active
  terminal instance.
* `terminalForeground` to use the default foreground color of the active
  terminal instance.
* `key:SomeXamlKey` to try and look `SomeXamlKey` up from our resources as a
  `Color`, and use that color for the value.
    - DISCUSSION: Does anyone want this?
    - is `accent` just `key:SystemAccentColor`? If it is, is it a reasonable
      alias that we'd want to provide anyways?

This will enable users to not only provide custom colors, but also use the
dynamic color of the active terminal instance as well.

Using `terminalBackground` with multiple concurrent panes with different
backgrounds could certainly lead to some odd behavior. The intention of the
setting is to provide a way for the tab/titlebar to "seamlessly" connect to the
terminal content. However, two panes side-by-side could have different
background colors, which might have an unexpected appearance. Since the user
must have opted in to theis behavior, they'll need to decide personally if
that's something that bothers them aesthetically. It's entirely possible that a
user doesn't use panes, and this wouldn't even be a problem for them.

<!-- We could maybe mitigate this by providing the user a way of specifying the
`tab.background` color as having both a "single pane" and "multiple pane" mode,
though I'm not sure I'm in love with this:

```json
    "tab.background": {"single": "terminalBackground", "multiple": null},
    "tab.background": {"single": "terminalBackground", "multiple": "#ff0000"},
    "tab.background": [ "terminalBackground", null ]
```

 Also shown is an array based implementation, as an option. Overall I'm not
 happy with this, so I think it shouldn't be in the final draft of the spec, but
 I'm leaving it for now as an option.
-->

### Implementation of theming

Largely, whenever possible, we should be able to implement this theming support
by modifying our application's `ResourceDictionary` with custom values to
control the appearance of UI elements.

For example, the `TabView` already exposes a number of XAML resources we can
modify to adjust it's appearance.
* `TabViewBackground` controls the appearance of the background of the tab view.
  In `showTabsInTitlebar: true` mode, this is the color of the titlebar.
* `TabViewItemHeaderBackground` and `TabViewItemHeaderBackgroundSelected`
  control the appearance of an individual tab.

By modifying the values of these brushes, we can control the appearance of the
tabs. So long as we only in-place modify the resources, XAML is smart enough to
be able to update it's appearance automatically. We can do this by querying the
`ResourceDictionary` for a given resource, and changing it's value, rather than
`insert`ing a new value into the `ResourceDictionary` to replace the old one.

In addition to the above properties, I propose adding a couple of our own
properties: `PaneBorderWidth`: To control the width of pane borders
`PaneBorderBrush`: To control the appearance of _inactive_ pane borders
`ActivePaneBorderBrush`: To control the appearance of _active_ pane borders

In order to respond to the live-updating of the `TermControl`'s background
color, we'l need to add some more specific logic beyond simply updating a XAML
resource when settings change. Whenever a `TermControl`'s background color
changes, or the active pane in a tab changes:
* If `tab.background == "terminalBackground"`:
    - If this control is the tab's active terminal control (and the tab doesn't
      have a custom color set by the color picker), update the tab's own
      `TabViewItem` with updated `TabViewItemHeaderBackground` and
      `TabViewItemHeaderBackgroundSelected` values.
        - Here, we _don't_ want to update the `App`'s resources, since those
          apply globally, and each tab might have a control with a different
          color.
        - The color set by the color picker should override the color from the
          theme (as the former is a run-time property set to override the
          latter).
* If `tabRow.background == "terminalBackground"`:
    - If this control is the active terminal of the active `Tab`, then we should
      make sure to change the value of the application's `TabViewBackground`.
      This will update the color of the titlebar area.
    - Make sure to register an event handler in the `NonClientIslandWindow` to
      listen for when the `Color` of the `TitlebarControl`'s `Background` brush
      changes. When that happens, the window will need to be invalidated, so
      that we can get a `WM_PAINT` and redraw the potion of the titlebar we're
      drawing with GDI.

The `tab.cornerRadius` might be a bit trickier to implement. Currently, there's
not a XAML resource that controls this, nor is this something that's exposed by
the TabView control. Fortunately, this is something that's exposed to us
programattically. We'll need to manually set that value on each `TabViewItem` as
we create new tabs. When we reload settings, we'll need to make sure to come
through and update those values manually.

### Tab Background Color, Overline Color, and the Tab Color Picker

Concurrently with the writing of this spec, work is being done to add a "color
picker" for tabs, that lets the user manually set a background color for tabs.
This may in the future cause some complications with setting a color for tabs
from the theme.

When both features are combined, the color set at runtime but the color picker
should override whatever color the user has set in the theme. When the color
picker "clears" the color it has set for the tab, it should revert to the color
from the theme (if one is set).

Also mentioned in the implementation of the color picker feature was the ability
to not set the entire color of the tab, but just the color of a tab "overline",
similar to the way Firefox (by default) styles the focused tab.

Currently, the `TabView` doesn't support a tab "overline" like this, however, in
the future where this is possible, we'd love to also support such an overline.
However, the story of setting the tab color manually becomes a bit more
confusing now.

* The user should be able to set both the `tab.background` and `tab.overline`
  colors in a theme.
* The user should be able to configure whether the color picker sets the
  `background` or the `overline` color of the tab.

The second setting added above will allow the user to change what's controlled
by the color picker. Similarly to how the color picker can set the background of
the tab to override the background from the theme, the user could configure the
color picker to be able to change the overline color, not the background color
of the tab. Then, when the user uses the color picker, the overline color will
be overridden by the color picker, instead of the tab background color.

### `requestedTheme`

`requestedTheme` is a setting which is currently a global property. This
controls whether the application is in `light`, `dark` or the `system` theme. If
a theme specifies the `window.requestedTheme` property, this value from the
theme should override the setting from the globals, because the theme the user
has selected would prefer a differnent theme for the window.

<!-- This is slightly confusing currently if you ask me. More discussion below
in "Potential Issues" -->

## UI/UX Design

[comment]: # What will this fix/feature look like? How will it affect the end user?



## Capabilities

[comment]: # Discuss how the proposed fixes/features impact the following key considerations:

### Accessibility

[comment]: # How will the proposed change impact accessibility for users of screen readers, assistive input devices, etc.

### Security

[comment]: # How will the proposed change impact security?

### Reliability

[comment]: # Will the proposed change improve reliabilty? If not, why make the change?

### Compatibility

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?

### Performance, Power, and Efficiency

## Potential Issues

It's totally possible for the user to set some sort of theme that just looks
bad. This is absolutely a "beauty in the eye of the beholder" situation - not
everyone is going to like the appearance of every theme. The goal of the
Terminal is to provide a basic theme that's appropriate for anyone, but empower
users to customize the terminal however they see fit. If the user chooses a
theme that's not particularily appealing, they can always change it back.

### Branding

Are we concerned that by enabling theming, the appearance of the Terminal won't
be as static, and won't necessarily have as specific a look? It might be harder
for potential users see a screenshot of the Terminal and _know_ "Thats the
Windows Terminal". Is this something we're really all that concerned about
though? If this is something users want (it is), then shouldn't that be what
matters?

### `requestedTheme` - Confusing?

Is the `requestedTheme` property now confusing with the addition of the
`applicationTheme` setting? This is a concern that was similarly raised in the
"Default Profile Settings" feature. How do we make it apparent that one of these
properties is the theme from the list of themes, and the other is one of
[`light`, `dark`, `system`]? Should we deprecate the `requestedTheme` property
entirely?

Alternatively, could we re-use the `requestedTheme` property? We could first try
looking up the theme to see if it's the name of a theme that's installed in the
user's settings, and if it's not, fall back to trying it as one of the original
enum values?

## Future considerations

#### Theming v2 Properties

* `tab.padding`:
* `tab.textColor`:
* `tabRow.shadows`:
* `tabRow.height`:
* `tabRow.underlineHeight`: Controls the height of a border placed between the tab row and the Terminal panes beneath it. This border doesn't exist currently.
* `tabRow.underlineColor`: Controls the color of the aforementioned underline

## Resources

[comment]: # Be sure to add links to references, resources, footnotes, etc.

[iTerm2-Color-Schemes]: https://github.com/mbadolato/iTerm2-Color-Schemes
