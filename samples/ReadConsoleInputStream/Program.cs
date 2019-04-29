/*
 *  This is a demo that shows how we can have a stream-oriented view of characters from the console
 *  while also listening to console events like mouse, menu, focus, buffer/viewport(1) resize events.
 *
 *  This has always been tricky to do because ReadConsoleW/A doesn't allow retrieving events.
 *  Only ReadConsoleInputW/A returns events, but isn't stream-oriented. Using both doesn't work because
 *  ReadConsoleW/A flushes the input queue, meaning calls to ReadConsoleInputW/A will wait forever.
 *
 *  I do this by deriving a new Stream class which wraps ReadConsoleInputW and accepts a provider/consumer
 *  implementation of BlockingCollection<Kernel32.INPUT_RECORD>. This allows asynchronous monitoring of
 *  console events while simultaneously streaming the character input. I also use Mark Gravell's great
 *  System.IO.Pipelines utility classes (2) and David Hall's excellent P/Invoke wrappers (3) to make this
 *  demo cleaner to read; both are pulled from NuGet.
 *
 * (1) in versions of windows 10 prior to 1809, the buffer resize event only fires for enlarging
 *     the viewport, as this would cause the buffer to be enlarged too. Now it fires even when
 *     shrinking the viewport, which won't change the buffer size.
 *
 * (2) https://github.com/mgravell/Pipelines.Sockets.Unofficial
 *     https://www.nuget.org/packages/Pipelines.Sockets.Unofficial
 *
 * (3) https://github.com/dahall/Vanara
 *     https://www.nuget.org/packages/Vanara.Pinvoke.Kernel32
 *
 *  Oisin Grehan - 2019/4/21
 *
 *  https://twitter.com/oising
 *  https://github.com/oising
 */

using System;
using System.Collections.Concurrent;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

using Pipelines.Sockets.Unofficial;
using Vanara.PInvoke;

namespace Samples.Terminal
{
    internal class Program
    {
        private static async Task Main(string[] args)
        {
            // run for 90 seconds
            var timeout = TimeSpan.FromSeconds(90);

            // in reality this will likely never be reached, but it is useful to guard against
            // conditions where the queue isn't drained, or not drained fast enough. 
            const int maxNonKeyEventRetention = 128;

            var source = new CancellationTokenSource(timeout);
            var token = source.Token;
            var handle = Kernel32.GetStdHandle(Kernel32.StdHandleType.STD_INPUT_HANDLE);

            if (!Kernel32.GetConsoleMode(handle, out Kernel32.CONSOLE_INPUT_MODE mode))
                throw NativeMethods.GetExceptionForWin32Error(Marshal.GetLastWin32Error());

            mode |= Kernel32.CONSOLE_INPUT_MODE.ENABLE_WINDOW_INPUT;
            mode |= Kernel32.CONSOLE_INPUT_MODE.ENABLE_VIRTUAL_TERMINAL_INPUT;
            mode &= ~Kernel32.CONSOLE_INPUT_MODE.ENABLE_ECHO_INPUT;
            mode &= ~Kernel32.CONSOLE_INPUT_MODE.ENABLE_LINE_INPUT;

            if (!Kernel32.SetConsoleMode(handle, mode))
                throw NativeMethods.GetExceptionForLastWin32Error();

            // base our provider/consumer on a bounded queue to keep memory usage under control
            var events = new BlockingCollection<Kernel32.INPUT_RECORD>(
                new ConcurrentBoundedQueue<Kernel32.INPUT_RECORD>(maxNonKeyEventRetention));

            // Task that will consume non-key events asynchronously
            var consumeEvents = Task.Run(() =>
            {
                Console.WriteLine("consumeEvents started");

                try
                {
                    while (!events.IsCompleted)
                    {
                        // blocking call
                        var record = events.Take(token);

                        Console.WriteLine("record: {0}",
                            Enum.GetName(typeof(Kernel32.EVENT_TYPE), record.EventType));
                    }
                }
                catch (OperationCanceledException)
                {
                    // timeout
                }

                Console.WriteLine("consumeEvents ended");
            }, token);

            // Task that will watch for key events while feeding non-key events into our provider/consumer collection 
            var readInputAndProduceEvents = Task.Run(async () =>
            {
                //So, this is the key point - we cannot use the following or we lose all non-key events:
                // Stream stdin = Console.OpenStandardInput();
                
                // get a unicode character stream over console input 
                Stream stdin = new ReadConsoleInputStream(handle, events);

                // wrap in a System.IO.Pipelines.PipeReader to get clean async and span/memory usage 
                var reader = StreamConnection.GetReader(stdin);

                while (!token.IsCancellationRequested)
                {
                    // blocking call
                    var result = await reader.ReadAsync(token);

                    if (result.IsCanceled)
                        break;

                    var sequence = result.Buffer;
                    var segment = sequence.Start;

                    while (sequence.TryGet(ref segment, out var mem))
                    {
                        // decode back from unicode
                        var datum = Encoding.Unicode.GetString(mem.Span);
                        Console.Write(datum);
                    }

                    reader.AdvanceTo(sequence.End);
                }
            }, token);

            Console.WriteLine("Running");

            try
            {
                await Task.WhenAll(consumeEvents, readInputAndProduceEvents);
            }
            catch (OperationCanceledException)
            {
                // timeout
            }

            Console.WriteLine("press any key...");
            Console.ReadKey(true);
        }
    }
}