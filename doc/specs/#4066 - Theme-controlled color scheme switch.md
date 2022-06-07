---
author: Ítalo Masserano arkthur/italo.masserano@gmail.com
created on: 2022-03-02
last updated: 2022-03-02
issue id: 4066
---

# Theme-controlled color scheme switch

## Abstract

The idea is for Windows Terminal to change automatically its color schemes according to what theme is selected, including the case where `system` theme is selected.

## Inspiration

I work remotely as a developer, so I have to spend a lot of hours in front of my PC screen. In my setup, right behind my desk I have a window, which is the only source of natural sunlight in my room.

Normally I like dark modes in all the programs and apps I use, but when there's too much sunlight, it becomes annoying, and sometimes even painful, to work in dark mode. So, I have all the programs and apps I use (at least, those that can) set to switch their color themes to what the system has.

The company I work for sent me a Macbook Pro, and my personal phone is an Android, both with automatic dark mode at sunset and light mode at sunrise, and in those devices it's been working relatively well. In Windows, as it is known, there's no such feature, so I manually change between dark and light mode when it's needed, and most of the programs and apps I use go along with this change. Windows Terminal, is not one of them.

The theme changes just as expected, but in an app like this, this change only affects the top of the window, leaving almost all of the screen at the mercy of what the color scheme is, and it doesn't depend on the theme, which defeats any attempt to make a good use of the `system` theme feature.

## Solution Design

Could be implemented in the form of:

```json
"colorScheme": {
    "light": "BlulocoLight",
    "dark": "BlulocoDark"
}
```
or:

```json
    "colorSchemeLight": "BlulocoLight",
    "colorSchemeDark": "BlulocoDark"
```

## UI/UX Design

In a first version it could look like the terminal in Visual Studio Code, and an improvement could be to have light mode specific color schemes, just like those already present in Windows terminal. A good idea could be to get an inspiration in Dark++ and Light++ VSCode color themes.

A user could benefit from a more healthy light level contrast between the screen their looking at and the environment they are, reducing the risk of headache or developing/intensifying eye problems, and any other related eye conditions. Plus, it adds to a more consistent experience between different programs and apps, and the system itself.

## Capabilities

### Accessibility

This feature improves accessibility more than any other capability, because the key is to be able to read and see anything better when the environment, both the external to the device, and the device's system itself, is in a certain mode (dark/light).

### Security

The proposed solution is based in the current way one sets Windows Terminal settings, so it isn't expected to add any security issues. 

### Reliability

Adding this feature would make Windows Terminal more reliable when it's expected that it changes it's visual theme/color scheme along with the whole system.

### Compatibility

The solution is not expected to break anything.

### Performance, Power, and Efficiency

It might increase the energy spent in the cases where people who were used to use the terminal in regular dark color schemes start using more light color schemes, but that is the case for any other program that shows lighter colors and I don't think the increment would be as high as to be even considered a downside.

## Potential Issues

Some users might not like the change in color schemes or be too used to the terminal being dark, but this may be avoided making the current schemes a default and adding this solution as an alternative setting.

## Future considerations

This solution might bring more attention to the color schemes setting, even more when considering light mode specific color schemes

## Resources

Inspired by what's been said in the issue comments. Credits to them.
