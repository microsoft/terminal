#!/bin/bash
# Script to find double ampersands (&&) in UI resource strings
# These are often errors where a single & should be used for menu accelerators

echo "Searching for double ampersands in resource files..."
echo "=================================================="
echo ""

# Search in .resw files (Windows Resource Files)
echo "1. Checking .resw files:"
find . -name "*.resw" -type f -exec grep -Hn "&amp;&amp;\|&&" {} \; 2>/dev/null

echo ""
echo "2. Checking .xaml files:"
find . -name "*.xaml" -type f -exec grep -Hn "&amp;&amp;\|&&.*Button\|&&.*Menu\|&&.*Text" {} \; 2>/dev/null

echo ""
echo "3. Checking .json localization files:"
find . -name "*.json" -type f -path "*/Resources/*" -exec grep -Hn "&&" {} \; 2>/dev/null

echo ""
echo "4. Checking for potential issues in string literals:"
find ./src -name "*.cpp" -o -name "*.h" -o -name "*.cs" | xargs grep -Hn '"[^"]*&&[^"]*"' 2>/dev/null | grep -v "^\s*//" | grep -v "operator"

echo ""
echo "=================================================="
echo "Search complete."
echo ""
echo "Note: In XML files, '&amp;' represents a single '&'"
echo "      So '&amp;&amp;' represents '&&' which should usually be '&amp;' (single '&')"
echo ""
echo "Common cases to fix:"
echo "  - Menu items: 'Save && Exit' → 'Save & Exit'"
echo "  - Button labels: 'Copy && Paste' → 'Copy & Paste'"
echo "  - In XML: '&amp;&amp;' → '&amp;'"
