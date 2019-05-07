# "EchoCon" ConPTY Sample App
This is a very simple sample application that illustrates how to use the new Win32 Pseudo Console 
(ConPTY) by:

1. Creating an input and an output pipe
1. Calling `CreatePseudoConsole()` to create a ConPTY instance attached to the other end of the pipes
1. Spawning an instance of `ping.exe` connected to the ConPTY
1. Running a thread that listens for output from `ping.exe`, writing received text to the Console

# Pre-Requirements
To build and run this sample, you must install:
* Windows 10 Insider build 17733 or later
* [Latest Windows 10 Insider SDK](https://www.microsoft.com/en-us/software-download/windowsinsiderpreviewSDK)

# Running the sample
Once successfully built, running EchoCon should clear the screen and display the results of the 
echo command:

```
Pinging Rincewind [::1] with 32 bytes of data:
Reply from ::1: time<1ms
Reply from ::1: time<1ms
Reply from ::1: time<1ms
Reply from ::1: time<1ms

Ping statistics for ::1:
    Packets: Sent = 4, Received = 4, Lost = 0 (0% loss),
Approximate round trip times in milli-seconds:
    Minimum = 0ms, Maximum = 0ms, Average = 0ms
```

# Resources
For more information on the new Pseudo Console infrastructure and API, please review 
[this blog post](https://blogs.msdn.microsoft.com/commandline/2018/08/02/windows-command-line-introducing-the-windows-pseudo-console-conpty/)
