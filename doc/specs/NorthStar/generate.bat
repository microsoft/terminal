@echo off
pushd .
cd Copilot
pandoc -s Overview.md -o ..\generated\Copilot\Overview.docx
pandoc -s Prompting.md -o ..\generated\Copilot\Prompting.docx
pandoc -s Implicit-Suggestions.md -o ..\generated\Copilot\Implicit-Suggestions.docx
pandoc -s Explain-that.md -o ..\generated\Copilot\Explain-that.docx
popd
