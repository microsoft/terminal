@echo off

rem bx - Build only the project in this directory without cleaning it first.
rem This is another script to help Microsoft developers feel at home working on
rem the terminal project.

call bcz exclusive no_clean %*
