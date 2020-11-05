To checkout the solution, and the branch I'm on, do the following:

```cmd
git clone https://github.com/microsoft/terminal.git
cd terminal
git checkout dev/migrie/oop-mixed-elevation-1
git submodule update --init --recursive
```

Then, to build the scratch projects I'm working with, do the following (in `cmd`):
_You may need to open the solution in VS once first, to make sure it installs all the dependencies!_

```cmd
.\tools\razzle.cmd
nuget restore
cd src\tools\ScratchWinRTClient
bx
```

(after the first build, `bx` will just rebuild the `ScratchWinRTClient` and `ScratchWinRTServer` projects.)

That should build both `ScratchWinRTClient` and `ScratchWinRTServer`. However, the client needs the server exe to live next to it, so you'll need to do the following (from the root of the solution)

```cmd
copy /y bin\x64\Debug\ScratchWinRTServer\ScratchWinRTServer.exe bin\x64\Debug\ScratchWinRTClient\
```

whenever the server's code changes.



Then, in one terminal window, run (again, from the root of the solution)

```cmd
bin\x64\Debug\ScratchWinRTClient\ScratchWinRTClient.exe
```

That'll print out a list of "Hosts" and their GUIDs. Copy one of those GUIDs, including braces. Then, in an elevated window, run:

```cmd
bin\x64\Debug\ScratchWinRTClient\ScratchWinRTClient.exe {the guid}
```

That'll hit the code in `createExistingObjectApp`, where we try to de-elevate
