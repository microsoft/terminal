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
    void createMonarchAndPeasant();
    void remindKingWhoTheyAre(const winrt::MonarchPeasantSample::IPeasant& peasant);

private:
    void _setupConsole();
    int _appLoop();

    winrt::MonarchPeasantSample::IPeasant _createOurPeasant();
};

bool monarchAppLoop(AppState& state);
bool peasantAppLoop(AppState& state);
