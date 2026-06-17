// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <json/json.h>

// JSON output (pretty-printed).
void PrintJson(const Json::Value& val);

// Human-readable formatters. The classic-COM server returns its results as
// JSON; these render that JSON for interactive (non --json) use.
void FormatWindowsHuman(const Json::Value& windows);    // array of window objects
void FormatTabsHuman(const Json::Value& tabs);          // array of tab objects
void FormatPanesHuman(const Json::Value& panes);        // array of pane objects
void FormatActivePaneHuman(const Json::Value& info);    // pane object
void FormatPaneStatusHuman(const Json::Value& status);  // process-status object
void FormatCreatedTabHuman(const Json::Value& result);  // tab-creation-result object
void FormatCreatedPaneHuman(const Json::Value& result); // tab-creation-result object
