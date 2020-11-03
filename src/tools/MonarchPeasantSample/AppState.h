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
    // void createMonarchAndPeasant();
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

BIG OLE TODO LIST:

* [ ] The Monarch needs to wait on peasants, to remove them from the map when they're dead
* [ ] The peasants need to be able to process commandlines passed to them by other peasants
* [ ] press a key in a peasant window to "activate" it
* [ ] Add a key to toggle the monarch through ["never", "lastActive", "always"] glmming behaviors
* [ ] Actually implement the "list peasants" thing

*/
