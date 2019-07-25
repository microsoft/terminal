// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Messages to the user
const auto resource = L"Terminal";
const auto userAgent = L"Terminal/0.0";
const auto codeExpiry = L"\r\nThis code will expire in 15 minutes.\r\n";
const auto enterTenant = L"Please enter the desired tenant number\r\n";
const auto newLogin = L"or enter n to login with a different account\r\n";
const auto removeStored = L"or enter r to remove the above saved connection settings.\r\n";
const auto invalidAccessInput = L"Please enter a valid number to access the stored connection settings, n to make a new one, or r to remove the stored ones.\r\n";
const auto nonNumberError = L"Please enter a number.\r\n";
const auto numOutOfBoundsError = L"Number out of bounds. Please enter a valid number. \r\n";
const auto noTenants = L"Could not find any tenants.\r\n";
const auto noCloudAccount = L"You have not set up your cloud shell account yet. Please go to https://shell.azure.com to set it up.\r\n";
const auto storePrompt = L"Do you want to save these connection settings for future logins? [y/n]\r\n";
const auto invalidStoreInput = L"Please enter y or n\r\n";
const auto requestingCloud = L"Requesting for a cloud shell...";
const auto success = L"Succeeded.\r\n";
const auto requestingTerminal = L"Requesting for a terminal (this might take a while)...";
const auto tokensStored = L"Your connection settings have been saved for future logins.\r\n";
const auto noTokens = L"No tokens to remove. \r\n";
const auto tokensRemoved = L"Tokens removed!\r\n";
const auto exitStr = L"Exit.\r\n";
const auto authString = L"Authenticated.\r\n";
const auto internetOrServerIssue = L"Could not connect to Azure. You may not have internet or the server might be down.\r\n";

const auto ithTenant = L"Tenant %d: %s (%s)\r\n";
