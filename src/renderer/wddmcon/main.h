// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
HRESULT
WINAPI
WDDMConBeginUpdateDisplayBatch(
    _In_ HANDLE hDisplay);

HRESULT
WINAPI
WDDMConCreate(
    _In_ HANDLE* phDisplay);

VOID
    WINAPI
    WDDMConDestroy(
        _In_ HANDLE hDisplay);

HRESULT
WINAPI
WDDMConEnableDisplayAccess(
    _In_ HANDLE phDisplay,
    _In_ BOOLEAN fEnabled);

HRESULT
WINAPI
WDDMConEndUpdateDisplayBatch(
    _In_ HANDLE hDisplay);

HRESULT
WINAPI
WDDMConGetDisplaySize(
    _In_ HANDLE hDisplay,
    _In_ CD_IO_DISPLAY_SIZE* pDisplaySize);

HRESULT
WINAPI
WDDMConUpdateDisplay(
    _In_ HANDLE hDisplay,
    _In_ CD_IO_ROW_INFORMATION* pRowInformation,
    _In_ BOOLEAN fInvalidate);
