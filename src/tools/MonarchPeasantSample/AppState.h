#pragma once
#include <conio.h>
#include "Monarch.h"
#include "Peasant.h"
#include "../../types/inc/utils.hpp"

class AppState
{
public:
    bool areWeTheKing(const bool logPIDs = false);
    void initializeState();

    HANDLE hInput = nullptr;
    HANDLE hOutput = nullptr;

    winrt::MonarchPeasantSample::IPeasant _peasant{ nullptr };
    winrt::MonarchPeasantSample::Monarch _monarch{ nullptr };

    static winrt::MonarchPeasantSample::Monarch instantiateAMonarch();

    void createMonarch();
    bool processCommandline();
    void remindKingWhoTheyAre(const winrt::MonarchPeasantSample::IPeasant& peasant);

    std::vector<winrt::hstring> args;

private:
    void _setupConsole();
    int _appLoop();

    winrt::MonarchPeasantSample::IPeasant _createOurPeasant();
};

bool monarchAppLoop(AppState& state);
bool peasantAppLoop(AppState& state);

/*

Useful test script:

pushd %OPENCON%\bin\x64\Debug\MonarchPeasantSample
wt -d . cmd /k MonarchPeasantSample.exe ; sp -d . cmd /k MonarchPeasantSample.exe ; sp -d . cmd /k MonarchPeasantSample.exe ; sp -d .
popd


BIG OLE TODO LIST:

* [x] The peasants need to be able to process commandlines passed to them by
  other peasants
* [x] press a key in a peasant window to "activate" it
* [x] Add a key to toggle the monarch through ["never", "lastActive", "always"]
  glomming behaviors
* [ ] Actually store a stack for the MRU peasant, not just the single MRU one
* [ ] The Monarch needs to wait on peasants, to remove them from the map when
  they're dead
* [ ] Actually implement the "list peasants" thing
* [ ] After an election, the entire MRU window state is lost, because it was
  only stored in the current monarch.
* [ ] Test:
    - Create a monarch(#1) & peasant(#2)
    - activate the peasant(#2)
    - exit the peasant(#2)
    - try running MonarchPeasantSample.exe -s 0 (or -s 2)
    - THIS WILL FAIL, but it _should_ just run the commandline in the monarch
      (in the case of `-s 0`) or in a new window (in the `-s 1` case)
*/
