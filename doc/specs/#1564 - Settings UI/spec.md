---
author: Kayla Cinnamon @cinnamon-msft
created on: 2020-06-29
last updated: 2020-06-29
issue id: #1564
---

# Settings UI Implementation

## Abstract

This spec describes the basic functionality of the settings UI, including disabling it, the navigation items, launch methods, and editing of settings. The specific layout of each page will defined in later design reviews.

## Inspiration

We have been wanting a settings UI since the dawn of Terminal time, so we need to define how it will interact with the application and how users should expect to interact with it.

## Solution Design

The settings UI will be the default experience. We will provide users an option to skip the settings UI and edit the raw JSON file.

### Ability to disable displaying the settings UI

Some users don't want a UI for the settings. A new global setting should be added to disable the settings UI and open the raw JSON file. Here is a setting proposal:

```json
"settingsUI": true
```

If people still like the UI but want to access the JSON file, we can provide an "Open the JSON file" button at the bottom of the navigation menu.

### Top-level navigation

The settings UI should have top-level navigation that aligns with the overall structure of the settings.json file. The following are the proposed navigation items:

- Globals
- Profiles
- Color schemes
- Key bindings

### Launch methods

#### Proposal 1 - Launch in a new window

Clicking the settings button in the dropdown menu will open the settings UI in a new window. This allows the user to edit their settings and see the Terminal live update with their changes.

In the Windows taskbar, the icon will appear as if Terminal has multiple windows open.

#### Proposal 2 - Launch in a new tab

Clicking the settings button in the dropdown menu will open the settings UI in a new tab. This helps us take steps toward supporting non-terminal content in a tab. However, tab tear-off should be implemented before we have this. This way, users can still see their changes take effect if they want by tearing out the settings UI.

### Editing and saving settings

#### Proposal 1 - Automatically save settings

As users edit fields in the settings UI, they are automatically saved and written to the JSON file. This allows the user to see their settings changes taking place in real time.

#### Proposal 2 - Implement a save button

Users will only see their settings changes take place once they click "Save". Clicking "Save" will write to the settings.json file. This aligns with the functionality that exists today by editing the settings.json file in a text editor and saving it.

A future option could be adding a TerminalControl inside the settings UI to preview what the changes will look like before actually saving them to the settings.json file.

## UI/UX Design

The purpose of the below image is to depict the navigation menu. The layout of the content in the page will be discussed in a future design review.

![Settings UI navigation](./navigation.png)

## Capabilities

### Accessibility

This will have to undergo full accessibility testing because it is a new UI element. All items inside the settings UI should be accessible by a screen reader and the keyboard.

### Security

This does not impact security.

### Reliability

This will not improve reliability.

### Compatibility

This will change the default experience to open the UI, rather than the JSON file in a text editor. This behavior can be reverted with the setting listed [above](#ability-to-disable-displaying-the-settings-ui).

### Performance, Power, and Efficiency

This does not affect performance, power, nor efficiency.

## Potential Issues

## Future considerations

- We will have to have design reviews for all of the content pages.
- We should have undo functionality. In a text editor, you can type `Ctrl+Z` however the settings UI is a bit more complex.
- Once we have a marketplace for themes and extensions, this should be added to the top-level navigation.
- As we add more features, the top-level navigation is subject to change in favor of improved usability.

## Resources
