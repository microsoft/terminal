using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;

using Vanara.PInvoke;

namespace Samples.Terminal
{
    /// <summary>
    /// Provides a Stream-oriented view over the console's input buffer key events
    /// while also collecting out of band events like buffer resize, menu etc in
    /// a caller-provided BlockingCollection.
    /// </summary>
    /// <remarks>The buffer contains unicode chars, not 8 bit CP encoded chars as we rely on ReadConsoleInputW.</remarks>
    public sealed class ReadConsoleInputStream : Stream
    {
        private const int BufferSize = 256;
        private const int BytesPerWChar = 2;
        private readonly BlockingCollection<Kernel32.INPUT_RECORD> _nonKeyEvents;
        private IntPtr _handle;

        /// <summary>
        /// Creates an instance of ReadConsoleInputStream over the standard handle for StdIn. 
        /// </summary>
        /// <param name="nonKeyEvents">A BlockingCollection provider/consumer collection for collecting non key events.</param>
        public ReadConsoleInputStream(BlockingCollection<Kernel32.INPUT_RECORD> nonKeyEvents) :
            this(Kernel32.GetStdHandle(Kernel32.StdHandleType.STD_INPUT_HANDLE), nonKeyEvents)
        {
        }

        /// <summary>
        /// Creates an instance of ReadConsoleInputStream over a caller-provided standard handle for stdin. 
        /// </summary>
        /// <param name="handle">A HFILE handle representing StdIn</param>
        /// <param name="nonKeyEvents">A BlockingCollection provider/consumer collection for collecting non key events.</param>
        internal ReadConsoleInputStream(HFILE handle,
            BlockingCollection<Kernel32.INPUT_RECORD> nonKeyEvents)
        {
            Debug.Assert(handle.IsInvalid == false, "handle.IsInvalid == false");

            _handle = handle.DangerousGetHandle();
            _nonKeyEvents = nonKeyEvents;
        }

        public override bool CanRead { get; } = true;

        public override bool CanWrite => false;

        public override bool CanSeek => false;

        public override long Length => throw new NotSupportedException("Seek not supported.");

        public override long Position
        {
            get => throw new NotSupportedException("Seek not supported.");
            set => throw new NotSupportedException("Seek not supported.");
        }

        protected override void Dispose(bool disposing)
        {
            _handle = IntPtr.Zero;

            base.Dispose(disposing);
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            ValidateRead(buffer, offset, count);

            Debug.Assert(offset >= 0, "offset >= 0");
            Debug.Assert(count >= 0, "count >= 0");
            Debug.Assert(buffer != null, "bytes != null");

            // Don't corrupt memory when multiple threads are erroneously writing
            // to this stream simultaneously.
            if (buffer.Length - offset < count)
                throw new IndexOutOfRangeException("IndexOutOfRange_IORaceCondition");

            int bytesRead;
            int ret;

            if (buffer.Length == 0)
            {
                bytesRead = 0;
                ret = Win32Error.ERROR_SUCCESS;
            }
            else
            {
                var charsRead = 0;
                bytesRead = 0;

                var records = new Kernel32.INPUT_RECORD[BufferSize];

                // begin input loop
                do
                {
                    var readSuccess = Kernel32.ReadConsoleInput(_handle, records, BufferSize, out var recordsRead);
                    Debug.WriteLine("Read {0} input record(s)", recordsRead);

                    // some of the arithmetic here is deliberately more explicit than it needs to be 
                    // in order to show how 16-bit unicode WCHARs are packed into the buffer. The console
                    // subsystem is one of the last bastions of UCS-2, so until UTF-16 is fully adopted
                    // the two-byte character assumptions below will hold. 
                    if (readSuccess && recordsRead > 0)
                    {
                        for (var index = 0; index < recordsRead; index++)
                        {
                            var record = records[index];

                            if (record.EventType == Kernel32.EVENT_TYPE.KEY_EVENT)
                            {
                                // skip key up events - if not, every key will be duped in the stream
                                if (record.Event.KeyEvent.bKeyDown == false) continue;

                                // pack ucs-2/utf-16le/unicode chars into position in our byte[] buffer.
                                var glyph = (ushort) record.Event.KeyEvent.uChar;

                                var lsb = (byte) (glyph & 0xFFu);
                                var msb = (byte) ((glyph >> 8) & 0xFFu);

                                // ensure we accommodate key repeat counts
                                for (var n = 0; n < record.Event.KeyEvent.wRepeatCount; n++)
                                {
                                    buffer[offset + charsRead * BytesPerWChar] = lsb;
                                    buffer[offset + charsRead * BytesPerWChar + 1] = msb;

                                    charsRead++;
                                }
                            }
                            else
                            {
                                // ignore focus events; not doing so makes debugging absolutely hilarious
                                // when breakpoints repeatedly cause focus events to occur as your view toggles
                                // between IDE and console.
                                if (record.EventType != Kernel32.EVENT_TYPE.FOCUS_EVENT)
                                {
                                    // I assume success adding records - this is not so critical
                                    // if it is critical to you, loop on this with a miniscule delay
                                    _nonKeyEvents.TryAdd(record);
                                }
                            }
                        }
                        bytesRead = charsRead * BytesPerWChar;
                    }
                    else
                    {
                        Debug.Assert(bytesRead == 0, "bytesRead == 0");
                    }

                } while (bytesRead == 0);
                
                Debug.WriteLine("Read {0} character(s)", charsRead);

                ret = Win32Error.ERROR_SUCCESS;
            }

            var errCode = ret;
            if (Win32Error.ERROR_SUCCESS != errCode)
                throw NativeMethods.GetExceptionForWin32Error(errCode);

            return bytesRead;
        }

        public override void Write(byte[] buffer, int offset, int count)
        {
            throw new NotImplementedException("Write operations not implemented.");
        }

        public override void Flush()
        {
            throw new NotSupportedException("Flush/Write not supported.");
        }

        public override void SetLength(long value)
        {
            throw new NotSupportedException("Seek not supported.");
        }

        public override long Seek(long offset, SeekOrigin origin)
        {
            throw new NotSupportedException("Seek not supported.");
        }

        private void ValidateRead(byte[] buffer, int offset, int count)
        {
            if (buffer == null)
                throw new ArgumentNullException(nameof(buffer));
            if (offset < 0 || count < 0)
                throw new ArgumentOutOfRangeException(offset < 0 ? nameof(offset) : nameof(count),
                    "offset or count cannot be negative numbers.");
            if (buffer.Length - offset < count)
                throw new ArgumentException("invalid offset length.");

            if (!CanRead) throw new NotSupportedException("Get read not supported.");
        }
    }
}