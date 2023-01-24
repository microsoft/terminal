---
author: Pankaj Bhojwani, pabhojwa@microsoft.com
created on: 2021-6-17
last updated: 2021-6-23
issue id: #1790
---

# Font features and axes of variation

## Abstract

This spec outlines how we can allow users to specify font features and axes of variation for fonts in Windows Terminal. Font features include things like being able to specify whether ligatures should be used as well as the specific stylistic set used for a font. Axes of variation commonly include things like weight and slant but can also include fancier things like shadow distance, depending on the font.

## Inspiration

Reference: [#1790](https://github.com/microsoft/terminal/issues/1790)

Currently, if a font has ligatures, we offer no way for a user to disable them. Many users would like the option to do so, and would also like the ability to choose stylistic sets for fonts - for example, at the time of this writing, Cascadia Code offers 4 stylistic sets but we offer no way for users to specify any of them.

In a similar vein, many fonts allow for setting variations on the font along certain attributes, commonly referred to as 'axes of variation'. We can offer users more font customization options by allowing them to configure these font variations.

## Solution Design

### Font features

It is already possible to pass in a list of [font feature structs](https://docs.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_font_feature) to DWrite for it to handle. A font feature struct contains only 2 things: 

1. A font feature tag
2. A parameter value

A font feature tag is constructed using a 4-character feature tag and the parameter value defines how the feature is applied. For most features, the parameter value is simply treated as a binary value - a value of 0 means the feature is not applied and a non-zero value means the feature is applied. For example, a font feature struct like {'ss03', 1} enables stylistic set 3 for the font and a font feature struct like {'liga', 0} disables ligatures. (Technically, the feature tag is _constructed_ with the 4-character tag and is not the 4-character tag itself, but they are treated the same in the example here for brevity's sake).

Currently, we pass in to DWrite a null value for the list of features to apply to the font. This causes DWrite to automatically apply a ['standard' list](https://github.com/fdwr/TextLayoutSampler/blob/master/DrawableObject.ixx#L802) of font features to the font. Naturally, passing in our own list of font features to DWrite means DWrite will _only_ apply the features we defined, and no longer apply the standard list. Since the standard list contains 11 features, we need to consider how we can allow users to specify 1 additional feature or delete 1 of the standard features without needing to redefine all the others.

We will do this by allowing users to define a dictionary in their settings.json file, where the keys are the 4-character feature tags and the values are the parameter values. This dictionary will then get applied to our internal dictionary (which will contain the standard list of 11 features with their parameter values), meaning that any new key-value pairs will get added to our dictionary and any existing key-value pairs will get updated. Finally, this 'merged' dictionary will be what we use to construct the list of features to pass into DWrite.

### Axes of variation

Specifying axes of variation is done in an extremely similar manner to the way font features are specified - a 4-character tag is used to specify which font axis is being modified and a numerical value is provided to specify the value the axis should be set to. For example, {'slnt', 20} specifies that the 'slant' axis should be set to 20.

There is also a standard list of axes of variation, and each axis has its own default. We will approach this the same way we approached font features, by allowing users to specify additional features or omit features without needing to redefine the defaults.

## UI/UX Design

Users will be able to add a new setting to their font objects (added in [#10433](https://github.com/microsoft/terminal/pull/10433)). The resultant font object may look something like this

```json
"font": {
    "face": "Cascadia Code",
    "size": 12,
    "features": {
        "ss03": 1,
        "liga": 0
    },
    "axes": {
        "slnt": 20.5
    }
}
```
There is one point to note here about clashing. For example, if a user has the old "weight" setting defined _as well as_ a "wght" axis defined, we will only use the "wght" axis value. We prioritize that value for a few reasons:

1. It is the more recent addition to our settings model. Thus, it is likely that a user that has defined both values probably just forgot to remove the old value.
2. It is the more precise value, it is a specific float value whereas the old "weight" setting is an enum (that eventually gets mapped to a float value).

## Capabilities

### Accessibility

Should not affect accessibility.

### Security

Should not affect security.

### Reliability

Aside from additional parsing required for the settings file (which inherently offers more locations for parsing to fail), we need to be careful about badly formed/nonexistent feature tags or axes specified in the user-defined dictionaries. We must make sure to ignore such declarations (perhaps alongside emitting a warning to the user) and only apply those that are correctly formed and exist.

### Compatibility

Older versions of Windows may not have the DWrite updates that allow for defining font features and axes of variation. We must make sure to fallback to the current implementation in these cases.

### Performance, Power, and Efficiency

Currently when rendering a run of text, if we detect that the given run is simple we will use a shortcut to obtain the glyphs needed, skipping over an expensive `GetGlyphs` call to DWrite. However, when the default feature list is changed in any way (either by adding a new feature or removing one of the defaults), there is no way for us to detect beforehand how the font glyphs would change.

This means that as long as the user requests a change to the default font feature list, we will _always_ skip the shortcut and call the expensive `GetGlyphs` function for every run of text.

This will naturally cause a performance cost that we will have to bear for this feature. However, it is worth noting that there are a fair number of glyphs that will cause a run of text to be deemed "not simple" (and thus cause us to call `GetGlyphs` anyway), for example when using Cascadia Code, any run of text that has the letters 'i', 'j', 'l', 'n', 'w' or 'x' is not considered simple (because those glyphs have localized variants).

## Potential Issues

See performance issues above.

## Future considerations

DWrite additionally offers the ability to vary the font features across runs of text. However, for our initial implementation of this feature, we will only apply font features to the entire buffer. If/when we decide to allow specifying font features for particular runs of text, we can lean into our existing mechanisms of splitting up runs of text to implement that.

We will also need to consider how we want to represent this in the settings UI. This is slightly more complex than other settings since users should be allowed to manually input 4-character tags.

## Resources

[DWRITE_FONT_FEATURE structure](https://docs.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_font_feature)

[DWRITE_FONT_AXIS_VALUE structure](https://docs.microsoft.com/en-us/windows/win32/api/dwrite_3/ns-dwrite_3-dwrite_font_axis_value)