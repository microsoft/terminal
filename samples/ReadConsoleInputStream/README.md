# ReadConsoleInputStream Demo

This is a demo that shows how we can have a stream-oriented view of characters from the console while also listening to console events like mouse, menu, focus, buffer/viewport resize events. This is partcularly useful when working with VT100 streams and ConPTY.

This has always been tricky to do because ReadConsoleW/A doesn't allow retrieving events. Only ReadConsoleInputW/A returns events, but isn't stream-oriented. Using both doesn't work because ReadConsoleW/A flushes the input queue, meaning calls to ReadConsoleInputW/A will wait forever.

I do this by deriving a new Stream class which wraps ReadConsoleInputW and accepts a provider/consumer implementation of BlockingCollection<Kernel32.INPUT_RECORD>. This allows asynchronous monitoring of console events while simultaneously streaming the character input. I also use Mark Gravell's great System.IO.Pipelines utility classes and David Hall's excellent P/Invoke wrappers  to make this demo cleaner to read; both are pulled from NuGet.

**Note:**

In versions of windows 10 prior to 1809, the buffer resize event only fires for enlarging the viewport, as this would cause the buffer to be enlarged too. Now it fires even when shrinking the viewport, which won't change the buffer size.

NuGet packages used (GitHub links):

* [Pipelines.Sockets.Unofficial](https://github.com/mgravell/Pipelines.Sockets.Unofficial)
* [Vanara P/Invoke](https://github.com/dahall/Vanara)