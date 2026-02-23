#!/bin/bash
# Script to find double ampersands (&&) in UI resource strings
# These are often errors where a single & should be used for menu accelerators

echo "Searching for double ampersands in UI resource files..."
echo "========================================================"
echo ""

issue_count=0

# Search in .resw files (Windows Resource Files)
echo "1. Checking .resw files (Windows Resources):"
resw_issues=$(find . -name "*.resw" -type f -exec grep -Hn "&amp;&amp;" {} \; 2>/dev/null)
if [ -n "$resw_issues" ]; then
    echo "$resw_issues"
    issue_count=$((issue_count + $(echo "$resw_issues" | wc -l)))
else
    echo "   ✓ No issues found"
fi

echo ""
echo "2. Checking .xaml files (UI markup with hardcoded text):"
xaml_issues=$(find . -name "*.xaml" -type f -exec grep -Hn 'Content="[^"]*&&[^"]*"\|Text="[^"]*&&[^"]*"\|Label="[^"]*&&[^"]*"\|PlaceholderText="[^"]*&&[^"]*"' {} \; 2>/dev/null | grep -v "x:Uid")
if [ -n "$xaml_issues" ]; then
    echo "$xaml_issues"
    issue_count=$((issue_count + $(echo "$xaml_issues" | wc -l)))
else
    echo "   ✓ No issues found"
fi

echo ""
echo "3. Checking for hardcoded Content/Text in XAML (localization issue):"
hardcoded=$(find ./src/cascadia -name "*.xaml" -type f -exec grep -Hn 'Content="[A-Z][a-z].*"\|Text="[A-Z][a-z].*"' {} \; 2>/dev/null | grep "x:Uid" | grep -v "x:Name\|AutomationProperties")
if [ -n "$hardcoded" ]; then
    echo "   ⚠ Warning: Found XAML elements with both x:Uid and hardcoded Content/Text:"
    echo "$hardcoded"
    echo "   These hardcoded values will override translations!"
else
    echo "   ✓ No hardcoded overrides found"
fi

echo ""
echo "4. Checking .json localization files:"
json_issues=$(find . -name "*.json" -type f -path "*/Resources/*" -exec grep -Hn '": ".*&&.*"' {} \; 2>/dev/null)
if [ -n "$json_issues" ]; then
    echo "$json_issues"
    issue_count=$((issue_count + $(echo "$json_issues" | wc -l)))
else
    echo "   ✓ No issues found"
fi

echo ""
echo "========================================================"
if [ $issue_count -eq 0 ]; then
    echo "✓ SUCCESS: No double ampersand issues found in UI strings!"
else
    echo "⚠ FOUND $issue_count potential issues"
    echo ""
    echo "Fix guide:"
    echo "  - In XML (.resw, .xaml): '&amp;&amp;' → '&amp;'"
    echo "  - In JSON: '&&' → '&'"
    echo "  - Remove hardcoded Content/Text attributes when x:Uid is present"
fi
echo ""
echo "Note: In XML files, '&amp;' represents a single '&'"
echo "      So '&amp;&amp;' represents '&&' (incorrect for UI text)"
echo "      Use '&amp;' for a single '&' in UI strings"
