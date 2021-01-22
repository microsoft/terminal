#pragma once
#include "SampleMonarch.h"
#include "SamplePeasant.h"
#include "../../types/inc/utils.hpp"

class AppState
{
public:
    bool areWeTheKing(const bool logPIDs = false);
    void initializeState();

    static winrt::MonarchPeasantSample::Monarch instantiateMonarch();

    void createMonarch();
    bool processCommandline();
    void remindKingWhoTheyAre(const winrt::MonarchPeasantSample::IPeasant& peasant);

    HANDLE hInput{ INVALID_HANDLE_VALUE };
    HANDLE hOutput{ INVALID_HANDLE_VALUE };
    winrt::MonarchPeasantSample::IPeasant peasant{ nullptr };
    winrt::MonarchPeasantSample::Monarch monarch{ nullptr };
    std::vector<winrt::hstring> args;

private:
    void _setupConsole();
    int _appLoop();

    winrt::MonarchPeasantSample::IPeasant _createOurPeasant();
};

bool monarchAppLoop(AppState& state); // Defined in MonarchMain.cpp
bool peasantAppLoop(AppState& state); // Defined in PeasantMain.cpp
