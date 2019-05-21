# GUIConsole

This is an example of what the skeleton of a custom WPF console might look like.

The `GUIConsole.WPF` project is a WPF application targeting .NET 4.6.1. It creates a single WPF `Window` that acts as the console, and keeps the underlying console visible. 

The `GUIConsole.ConPTY` project is a .NET Standard 2.0 library that handles the creation of the console, and enables pseudoconsole behavior. `Terminal.cs` contains the publicly visible pieces that the WPF application will interact with. `Terminal.cs` exposes two things that allow reading from, and writing to, the console:
 * `ConsoleOutStream`, a `FileStream` hooked up to the pseudoconsole's output pipe. This will output VT100.
 * `WriteToPseudoConsole(string input)`, a method that will take the given string and write it to the pseudoconsole via its input pipe. This accepts VT100.
