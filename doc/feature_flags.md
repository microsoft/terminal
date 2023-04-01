# til::feature

Feature flags are controlled by an XML document stored at `src/features.xml`.

## Example Document

```xml
<?xml version="1.0" encoding="utf-8"?>
<featureStaging xmlns="http://microsoft.com/TilFeatureStaging-Schema.xsd">
    <feature>
        <!-- This will produce Feature_XYZ::IsEnabled() and TIL_FEATURE_XYZ_ENABLED (preprocessor) -->
        <name>Feature_XYZ</name>

        <description>Does a cool thing</description>

        <!-- GitHub deliverable number; optional -->
        <id>1234</id>

        <!-- Whether the feature defaults to enabled or disabled -->
        <stage>AlwaysEnabled|AlwaysDisabled</stage>

        <!-- Branch wildcards where the feature should be *DISABLED* -->
        <alwaysDisabledBranchTokens>
            <branchToken>branch/with/wildcard/*</branchToken>
            <!-- ... more branchTokens ... -->
        </alwaysDisabledBranchTokens>

        <!-- Just like alwaysDisabledBranchTokens, but for *ENABLING* the feature. -->
        <alwaysEnabledBranchTokens>
            <branchToken>...</branchToken>
        </alwaysEnabledBranchTokens>

        <!-- Brandings where the feature should be *DISABLED* -->
        <alwaysDisabledBrandingTokens>
            <!-- Valid brandings include Dev, Preview, Release, WindowsInbox -->
            <brandingToken>Release</brandingToken>
            <!-- ... more brandingTokens ... -->
        </alwaysDisabledBrandingTokens>

        <!-- Just like alwaysDisabledBrandingTokens, but for *ENABLING* the feature -->
        <alwaysEnabledBrandingTokens>
            <branchToken>...</branchToken>
        </alwaysEnabledBrandingTokens>

        <!-- Unequivocally disable this feature in Release -->
        <alwaysDisabledReleaseTokens />
    </feature>
</featureStaging>
```

## Notes

Features that are disabled for Release using `alwaysDisabledReleaseTokens` are
*always* disabled in Release, even if they come from a branch that would have
been enabled by the wildcard.

### Precedence

1. `alwaysDisabledReleaseTokens`
2. Enabled branches
3. Disabled branches
   * The longest branch token that matches your branch will win.
3. Enabled brandings
4. Disabled brandings
5. The feature's default state
