#pragma once

// For real code we'd do something like this
// HWND CreateExtensionWindow();
// But for now wwell just call main()

// Set the window procedure that the extension host library should call to.
// This lets the extension implementer handle window messages too.
void SetExtensionWindowProc(WNDPROC proc);
