# Fixing Double Ampersand (&&) and UI Localization Issues

## Problem Description

### Issue 1: Double Ampersand in UI Strings

In Windows UI strings, a single ampersand (`&`) is used to denote keyboard accelerators (hotkeys). For example:
- `&Save` displays as "**S**ave" with 'S' underlined
- `Save & Exit` displays as "Save & Exit" (literal ampersand)

Sometimes, a double ampersand (`&&`) may be incorrectly used where a single ampersand should be:
- ❌ **Incorrect**: `Speichern && Beenden` 
- ✅ **Correct**: `Speichern & Beenden`

### Issue 2: Hardcoded UI Text Overriding Localization

XAML elements that have both `x:Uid` (for localization) and hardcoded `Content` or `Text` attributes will **always show the hardcoded text**, ignoring translations.

**Example - What was broken:**
```xaml
<ToggleButton x:Uid="TabColorCustomButton"
              Content="Custom" />  <!-- ❌ This overrides translations! -->
```

**Fixed version:**
```xaml
<ToggleButton x:Uid="TabColorCustomButton" />  <!-- ✅ Now uses localized text -->
```

## How to Search for These Issues

### Method 1: Using the Automated Scripts

**Find issues:**
```bash
./tools/find-double-ampersand.sh
```

**Auto-fix issues (with confirmation):**
```bash
./tools/fix-double-ampersand.sh
```

### Method 2: Manual Search Commands

#### Search for Double Ampersands in Resource Files (.resw)
```bash
# For XML-encoded ampersands (most common)
grep -rn "&amp;&amp;" --include="*.resw" ./src/

# For literal ampersands in value tags
grep -rn "<value>.*&&.*</value>" --include="*.resw" ./src/
```

#### Search for Hardcoded UI Text Overriding Localization
```bash
# Find XAML elements with both x:Uid and hardcoded Content/Text
grep -rn 'x:Uid.*Content="\|Content=".*x:Uid' --include="*.xaml" ./src/

# Alternative: Find any hardcoded Content with x:Uid in the same element
find ./src/cascadia -name "*.xaml" -exec grep -A 3 'x:Uid=' {} \; | grep 'Content="[A-Z]'
```

#### Search in XAML Files for Double Ampersands
```bash
grep -rn "&amp;&amp;" --include="*.xaml" ./src/
```

#### Search in JSON Localization Files
```bash
grep -rn '".*&&.*"' --include="*.json" ./src/cascadia/*/Resources/
```

## How to Fix

### Fix Type 1: Double Ampersands in Resource Files

#### In .resw (Resource) Files

**Before:**
```xml
<data name="SaveAndExit.Content" xml:space="preserve">
  <value>Speichern &amp;&amp; Beenden</value>
</data>
```

**After:**
```xml
<data name="SaveAndExit.Content" xml:space="preserve">
  <value>Speichern &amp; Beenden</value>
</data>
```

Note: In XML, `&amp;` is the escaped form of `&`, so:
- `&amp;&amp;` = `&&` (incorrect for UI text)
- `&amp;` = `&` (correct)

#### In XAML Files

**Before:**
```xml
<Button Content="Save &amp;&amp; Exit"/>
```

**After:**
```xml
<Button Content="Save &amp; Exit"/>
```

#### In JSON Files

**Before:**
```json
{
  "saveAndExit": "Save && Exit"
}
```

**After:**
```json
{
  "saveAndExit": "Save & Exit"
}
```

### Fix Type 2: Hardcoded Text Overriding Localization (Issue #19756)

This was the actual bug found and fixed in ColorPickupFlyout.xaml.

#### Problem: Custom Button
**Before (BROKEN - Always shows "Custom" in English):**
```xaml
<ToggleButton x:Name="CustomColorButton"
              x:Uid="TabColorCustomButton"
              Content="Custom"
              IsChecked="False" />
```

The hardcoded `Content="Custom"` attribute overrides the German translation "Benutzerdefiniert" that exists in Resources.resw files.

**After (FIXED - Shows localized text):**
```xaml
<ToggleButton x:Name="CustomColorButton"
              x:Uid="TabColorCustomButton"
              IsChecked="False" />
```

Now the `x:Uid` properly loads the translated text from each language's Resources.resw file.

#### Problem: OK Button
**Before (BROKEN - Missing translations):**
```xaml
<Button x:Name="OkButton"
        x:Uid="OkButton"
        Content="OK"
        Style="{ThemeResource AccentButtonStyle}" />
```

**After (FIXED):**
1. Remove hardcoded Content from XAML:
```xaml
<Button x:Name="OkButton"
        x:Uid="OkButton"
        Style="{ThemeResource AccentButtonStyle}" />
```

2. Add resource entry to all language files (e.g., de-DE/Resources.resw):
```xml
<data name="OkButton.Content" xml:space="preserve">
  <value>OK</value>
</data>
```
<Button Content="Save &amp; Exit"/>
```

### In JSON Files

**Before:**
```json
{
  "saveAndExit": "Save && Exit"
}
```

**After:**
```json
{
  "saveAndExit": "Save & Exit"
}
```

### In C++ String Literals

**Before:**
```cpp
const auto buttonText = L"Save && Exit";
```

**After:**
```cpp
const auto buttonText = L"Save & Exit";
```

## When NOT to Change

Do **NOT** change `&&` in these cases:

1. **Logical AND operators in code**:
   ```cpp
   if (isValid && isReady) { }  // ✅ Keep as is
   ```

2. **Comments explaining logical operators**:
   ```cpp
   // We use && to check both conditions  // ✅ Keep as is
   ```

3. **Test data or escape sequences**:
   ```cpp
   expectedOutput.push_back(L"\x1b[M &&");  // ✅ Keep as is (test data)
   ```

4. **Font/ligature examples in documentation**:
   ```cpp
   // Some fonts render "&&" as a ligature  // ✅ Keep as is (documentation)
   ```

5. **Shell commands or examples**:
   ```bash
   bcz dbg && runut  // ✅ Keep as is (command chain)
   ```

## Real-World Fix: Issue #19756

### What Was Broken

The ColorPickupFlyout had two localization problems:

1. **Custom button** - Hardcoded `Content="Custom"` prevented showing "Benutzerdefiniert" (German), the French translation, etc.
2. **OK button** - Hardcoded `Content="OK"` and missing resource entries in translation files

### Files Changed

- `src/cascadia/TerminalApp/ColorPickupFlyout.xaml` - Removed hardcoded Content attributes
- `src/cascadia/TerminalApp/Resources/*/Resources.resw` (88 files) - Added OkButton.Content entries

### Result

✅ Color picker now properly displays translated text in all languages
✅ German users see "Benutzerdefiniert" instead of "Custom"
✅ All 88 supported languages can translate the OK button

## Current Status

As of **February 23, 2026**:

✅ **No double ampersand issues found** in UI resource strings  
✅ **Fixed hardcoded text overriding localization** in ColorPickupFlyout  
✅ **Added OkButton.Content** to all 88 language resource files  
✅ **Created detection tools** to prevent future issues

## Prevention

### For Developers

When adding new UI strings:

1. **Never hardcode Content/Text when using x:Uid**
   ```xaml
   <!-- ❌ BAD -->
   <Button x:Uid="MyButton" Content="Click Me" />
   
   <!-- ✅ GOOD -->
   <Button x:Uid="MyButton" />
   ```

2. **Use single `&` for literal ampersands in text**
   - In XML files, escape as `&amp;`
   - In JSON, use `&` directly

3. **For menu accelerators, place `&` before the letter**
   - `&Save`, `&Open`, `&Exit`
   - Displays as: **S**ave (S underlined), **O**pen (O underlined), **E**xit (E underlined)

4. **To show a literal ampersand in menus, use `&&`**
   - This is rare, but valid when you actually want to display `&`
   - In XML: `&amp;&amp;` → displays as `&`

### Automated Checking

Add to CI/CD pipeline or pre-commit hooks:

```bash
#!/bin/bash
# .git/hooks/pre-commit or in CI pipeline

echo "Checking for UI localization issues..."
./tools/find-double-ampersand.sh

if [ $? -ne 0 ]; then
  echo "❌ Found localization issues. Run ./tools/fix-double-ampersand.sh to fix."
  exit 1
fi

echo "✅ No localization issues found"
```

### For Translators

When translating resource files:

1. **Preserve ampersands for keyboard shortcuts**
   - English: `&File` → German: `&Datei`
   - The accelerator key may change based on the language

2. **Don't double ampersands unless showing literal `&`**
   - Use `&amp;` in XML, not `&amp;&amp;`

3. **Test your translations in the UI**
   - Verify menu shortcuts work
   - Ensure text displays correctly

## Tools Provided

### 1. find-double-ampersand.sh
**Purpose:** Scan codebase for localization issues  
**Usage:** `./tools/find-double-ampersand.sh`  
**Output:** Lists files with problems and suggestions

**Features:**
- Detects double ampersands in .resw files
- Finds hardcoded Content/Text overriding x:Uid
- Checks JSON localization files
- Distinguishes UI strings from code operators

### 2. fix-double-ampersand.sh  
**Purpose:** Automatically fix common issues  
**Usage:** `./tools/fix-double-ampersand.sh`  
**Safety:** Asks for confirmation before modifying files

**What it fixes:**
- Replaces `&amp;&amp;` with `&amp;` in .resw value tags
- Replaces `&&` with `&` in JSON string values
- Reports XAML files needing manual review

## Summary

This document and tooling addresses two related localization issues:

1. **Double Ampersand Bug** - Rare but easily detectable issue where `&&` appears in UI text
2. **Hardcoded Text Override** - Common issue where XAML attributes override localized resources

Both break the internationalization (i18n) of Windows Terminal, causing non-English users to see English text or incorrect symbols.

**Related Issues:**
- [#19756](https://github.com/microsoft/terminal/issues/19756) - right-click menu UI needs to be localized

**Contributing:**
If you find similar issues, use these tools to detect and fix them, then submit a pull request!
