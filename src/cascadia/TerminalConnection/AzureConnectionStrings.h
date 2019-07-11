#pragma once

// Messages to the user
const auto resource = L"Terminal";
const auto codeExpiry = L" This code will expire in 15 minutes.\r\n";
const auto newLogin = L"or enter n to login with a different account\r\n";
const auto removeStored = L"or enter r to remove the above saved connection settings.\r\n";
const auto invalidAccessInput = L"Please enter a valid number to access the stored connection settings, n to make a new one, or r to remove the stored ones.\r\n";
const auto nonNumberError = "Please enter a number.\r\n";
const auto numOutOfBoundsError = "Number out of bounds. Please enter a valid number. \r\n";
const auto noTenants = "Could not find any tenants.\r\n";
const auto noCloudAccount = "You have not set up your cloud shell account yet. Please go to https://shell.azure.com to set it up.\r\n";
const auto storePrompt = "Do you want to save these connection settings for future logins? [y/n]\r\n";
const auto invalidStoreInput = "Please enter y or n\r\n";
const auto requestingCloud = "Requesting for a cloud shell...";
const auto success = "Succeeded.\r\n";
const auto requestingTerminal = "Requesting for a terminal (this might take a while)...";
const auto tokensStored = "Your connection settings have been saved for future logins.\r\n";
const auto noTokens = "No tokens to remove. \r\n";
const auto tokensRemoved = "Tokens removed!\r\n";
