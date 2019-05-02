
// Usage from within windbg:
// 1. load symbols for the module containing conhost
// 2. .load jsprovider.dll
// 3. .scriptload <some-path-to-this-file>\WindbgExtension.js
//
// From here, commands can be called. For example, to get information about ServiceLocator::s_globals:
// dx @$scriptContents.ServiceLocatorVar("s_globals")


function initializeScript()
{
    host.diagnostics.debugLog("***> OpenConsole debugger extension loaded \n");
}

// Routine Description:
// - Displays information about a field found in the ServiceLocator class
// Arguments:
// - varName - the variable to display information for
// Return Value:
// - debugger object used to display information about varName
function ServiceLocatorVar(varName)
{
    return host.namespace.Debugger.Utility.Control.ExecuteCommand("dx Microsoft::Console::Interactivity::ServiceLocator::" + varName);
}
