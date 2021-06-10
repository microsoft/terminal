---
author: Pankaj Bhojwani, pabhojwa@microsoft.com
created on: 2021-6-8
last updated: 2021-6-8
issue id: #6049
---

# Font configuration objects

## Abstract

This spec outlines how we can support 'font configuration objects' in our settings, which will make editing all font-related settings easier and will also allow users to customize font behaviour based on certain text attributes. For example, bold text can have a different font configuration object as compared to italic text.

## Inspiration

Reference: [#6049](https://github.com/microsoft/terminal/issues/6049)

As we move towards allowing more font-related settings, it will be helpful to have them all in one object within the profile, rather than cluttered all over the profile itself. To elaborate, [#1790](https://github.com/microsoft/terminal/issues/1790) contains several work items regarding font configurability that we have yet to complete. As these get added in, it will be preferable to have a single object that contains all font-related settings.

## Solution Design

The implementation design for font configuration objects will be very similar to the way appearance configuration objects were implemented ([reference](https://github.com/microsoft/terminal/pull/8345)). There will be a default font object, that will contain settings for the rendering of font in the default case. Beyond that, a number of optional font objects can be created for various use cases such as the rendering of bold font or italic font. These optional font objects will have a smaller number of allowed parameters, and will inherit from the default font object (i.e. any parameters that they do not define will be taken from the default font object).

### Allowed parameters and allowed non-default font configurations

As of now, Windows Terminal supports 3 font-related settings: `fontFace`, `fontWeight` and `fontSize`. The default font object will be allowed to define all 3 of these settings, whereas the non-default font objects will only be allowed to define `fontFace` and `fontWeight`. We do not allow the non-default font objects to define a `fontSize` as we do not want there to be resizes as the text switches between different types of font like bold/italic.

In the initial stages of this feature, we will only allow two non-default font configurations: `italicFont` and `boldFont`.

### Inheritance

The font configuration class will be implemented such that it allows for inheritance between objects of the class. As described earlier, within the same profile, non-default font configuration objects will inherit from the default font configuration object.

*Between* profiles, each profile's default font configuration object will inherit from the default font configuration object defined in `profiles.defaults`. As for the non-default font configuration objects, these are treated as a single setting. I.e. a profile's `italicFont` font configuration object does *not* inherit from the `italicFont` object defined in `profiles.defaults`, it is simply its own setting. The `italicFont` configuration object defined in `profiles.defaults` only applies to profiles that do not define their own `italicFont`.

This design is in line with the way we handle `defaultAppearance` and `unfocusedAppearance`, and was deliberately chosen so we maintain consistency across these different classes.

## UI/UX Design

Users will be able to specify font configurations in their profiles that will look something like this:

```
"font":
{
    "face": "Cascadia Mono",
    "weight": "Normal",
    "size": 12,
    "variants": [
        "bold": {
            "face": "Consolas",
            "weight": "Extra-Bold"
        },
        "italic": {
            "weight": "Light"
        }
    ]
}
```

In the settings UI, the current way font settings are presented to the user can be preserved, and additional dropdowns can be added for `boldFont` and `italicFont`. Each dropdown will contain options to configure `fontFace` and `fontWeight`.

## Further implementation details

Currently, the `DxRenderer` is given a desired font and it creates a text layout for the default case and a separate text layout for the italic case. To support font configuration objects, the `DxRenderer` will need to be updated to be able to receive a collection of desired fonts, and it will create a separate text layout for each desired font. These text layouts will then be obtained by the renderer when needed, as it already does for the italic case.

To have the renderer behave in the default manner (i.e. the manner it does currently) for a particular font type (bold/italic), a null font configuration object should be passed in.

## Capabilities

### Accessibility

Should not affect accessibility.

### Security

Should not affect security.

### Reliability

We have had several issues when parsing `fontFace` from settings in the past. This will add further potential for parsing errors simply because there will be more places where we will need to parse `fontFace`. Unfortunate as it is, it is an unavoidable aspect of this feature.

### Compatibility

This feature changes the way we expect to parse font settings. However, we have to make sure we are still able to parse font settings the way they are currently, so as to not break functionality for legacy users.

This might call for a 'settings rewriter', which takes a settings file and rewrites legacy configurations into the current format, which will be used before we do any settings parsing.

### Performance, Power, and Efficiency

Rendering several different font faces will probably have a performance impact. Again, this is an unavoidable aspect of this feature.

## Potential Issues

Already covered in the sections above.

## Future considerations

Similar to the discussion we had regarding appearance configurations, we will need to figure out how we want to deal with overlapping configurations. I.e. if font configurations are defined for both bold and italic, how should font that is both bold and italic be handled?

## Resources


