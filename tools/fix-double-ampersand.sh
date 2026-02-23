#!/bin/bash
# Script to automatically fix double ampersands (&&) in UI resource strings
# CAUTION: This script modifies files. Review changes before committing!

set -e

echo "Fixing double ampersands in UI resource files..."
echo "=================================================="
echo ""
echo "⚠ WARNING: This script will modify files!"
echo "   Make sure you have committed your work or can review the changes."
echo ""
read -p "Continue? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
fi

fix_count=0

# Fix in .resw files (Windows Resource Files)
echo ""
echo "1. Fixing .resw files..."
resw_files=$(find ./src/cascadia -name "*.resw" -type f -exec grep -l "&amp;&amp;" {} \; 2>/dev/null || true)
if [ -n "$resw_files" ]; then
    for file in $resw_files; do
        echo "   Fixing: $file"
        # Replace &amp;&amp; with &amp; in value tags only
        sed -i 's/<value>\(.*\)&amp;&amp;\(.*\)<\/value>/<value>\1\&amp;\2<\/value>/g' "$file"
        fix_count=$((fix_count + 1))
    done
else
    echo "   ✓ No files to fix"
fi

# Fix hardcoded Content/Text in XAML files
echo ""
echo "2. Checking for hardcoded XAML attributes that should use localization..."
xaml_with_uid=$(find ./src/cascadia -name "*.xaml" -type f -exec grep -l 'x:Uid.*Content="\|Content=".*x:Uid' {} \; 2>/dev/null || true)
if [ -n "$xaml_with_uid" ]; then
    echo "   ⚠ Found XAML files with both x:Uid and hardcoded Content/Text"
    echo "   These require manual review:"
    for file in $xaml_with_uid; do
        echo "      - $file"
    done
    echo ""
    echo "   Manual fix required: Remove Content=\"...\" attribute when x:Uid is present"
else
    echo "   ✓ No hardcoded overrides found"
fi

# Fix in JSON files
echo ""
echo "3. Fixing .json localization files..."
json_files=$(find . -name "*.json" -type f -path "*/Resources/*" -exec grep -l '": ".*&&' {} \; 2>/dev/null || true)
if [ -n "$json_files" ]; then
    for file in $json_files; do
        echo "   Fixing: $file"
        # Replace && with & in JSON string values (but not in URLs or code)
        sed -i 's/\(": "[^"]*\)&&\([^"]*"\)/\1\&\2/g' "$file"
        fix_count=$((fix_count + 1))
    done
else
    echo "   ✓ No files to fix"
fi

echo ""
echo "=================================================="
if [ $fix_count -eq 0 ]; then
    echo "✓ No automatic fixes applied"
else
    echo "✓ Fixed $fix_count files"
    echo ""
    echo "Next steps:"
    echo "  1. Review the changes: git diff"
    echo "  2. Test the application"
    echo "  3. Commit if correct: git add -A && git commit -m 'Fix double ampersands in UI strings'"
fi
echo ""
echo "⚠ Remember: Always review changes before committing!"
