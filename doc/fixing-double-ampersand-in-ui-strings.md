# Fixing Double Ampersand (&&) in UI Strings

## Problem Description

In Windows UI strings, a single ampersand (`&`) is used to denote keyboard accelerators (hotkeys). For example:
- `&Save` displays as "**S**ave" with 'S' underlined
- `Save & Exit` displays as "Save & Exit" (literal ampersand)

Sometimes, a double ampersand (`&&`) may be incorrectly used where a single ampersand should be:
- ❌ **Incorrect**: `Speichern && Beenden` 
- ✅ **Correct**: `Speichern & Beenden`

## How to Search for This Issue

### Method 1: Using the Search Script

Run the provided search script:
```bash
./tools/find-double-ampersand.sh
```

### Method 2: Manual Search Commands

#### Search in Resource Files (.resw)
```bash
# For XML-encoded ampersands
grep -rn "&amp;&amp;" --include="*.resw" ./src/

# For literal ampersands (less common in XML)
grep -rn ">.&&<" --include="*.resw" ./src/
```

#### Search in XAML Files
```bash
grep -rn "&amp;&amp;" --include="*.xaml" ./src/
```

#### Search in JSON Localization Files
```bash
grep -rn "&&" --include="*.json" ./src/cascadia/*/Resources/
```

#### Search in Source Code String Literals
```bash
grep -rn '"[^"]*&&[^"]*"' --include="*.cpp" --include="*.cs" ./src/ | grep -v "operator"
```

## How to Fix

### In .resw (Resource) Files

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
- `&amp;&amp;` = `&&` (incorrect)
- `&amp;` = `&` (correct)

### In XAML Files

**Before:**
```xml
<Button Content="Save &amp;&amp; Exit"/>
```

**After:**
```xml
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

4. **Font/ligature examples**:
   ```cpp
   // Some fonts render "&&" as a ligature  // ✅ Keep as is (documentation)
   ```

## Current Status

As of the last check on **February 23, 2026**, no instances of incorrect double ampersands were found in the Windows Terminal UI resource strings. All occurrences of `&&` in the codebase are legitimate uses (logical AND operators, comments, or test data).

## Prevention

When adding new UI strings:
1. Use a single `&` for literal ampersands in text
2. In XML files, escape it as `&amp;`
3. For menu accelerators, place `&` before the letter: `&Save`, `&Open`, `&Exit`
4. To show a literal ampersand in menus, use `&&` (escaped as `&amp;&amp;` in XML), but this is rare

## Automated Checking

Consider adding this check to your CI/CD pipeline:
```bash
# Add to .github/workflows or build script
./tools/find-double-ampersand.sh
if [ $? -ne 0 ]; then
  echo "Warning: Potential double ampersand issues found in UI strings"
fi
```
