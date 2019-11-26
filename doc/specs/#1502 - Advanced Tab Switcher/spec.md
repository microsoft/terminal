---
author: Leon Liang @leonMSFT, Mike Griese @zadjii-msft
created on: 2019-11-26
last updated: 2019-11-26
issue id: 1502
---

# Advanced Tab Switcher

## Abstract

Currently, the user is able to cycle through tabs on the tab bar. However, this horizontal cycling can be pretty inconvenient when the tab titles are long, or when there are too many tabs on the tab bar. In addition, a common use case is to switch between two tabs, e.g. when one tab is used as reference and the other is the actively worked-on tab. If the tabs are not right next to each other on the tab bar, it could be difficult to quickly swap between the two. To try to alleviate some of those pain points, we want to create an advanced tab switcher.

This spec will cover the design of the switcher, and how a user would interact with the switcher. It would be primarily keyboard driven, and would give a pop-up display of a vertical list of tabs. The tab switcher would also be able to display the tabs in Most Recently Used (MRU) order.

## Inspiration

This was mainly inspired by the tab switcher that's found in Visual Studio Code and Visual Studio.

VS Code's tab switcher appears directly underneath the tab bar.

![Visual Studio Code Tab Switcher](img/VSCodeTabSwitcher.png)

Visual Studio's tab switcher presents itself as a box in the middle of the editor.

![Visual Studio Tab Switcher](img/VSTabSwitcher.png)

I'm partial towards Visual Studio's implementation, where a box pops up in the center of the window. It allows for expansion in all four directions, which could be useful when adding future features such as Pane Navigation.

In terms of navigating the switcher, both VSCode and Visual Studio behave very similarly. Both open with the press of <kbd>ctrl+tab</kbd> and dismiss on release of <kbd>ctrl</kbd>. They both also allow the user to select the tab with the mouse and with <kbd>enter</kbd>. <kbd>esc</kbd> and a mouse click outside of the switcher dismiss the window as well.

## Solution Design

In addition to the current in-order tab list, namely `vector<shared_ptr<Tab>> _tabs`, we'll add `vector<weak_ptr<Tab>> _mruTabs` to represent the tabs in Most Recently Used order.
`_mruTabs` will use `weak_ptr` because the tab switcher doesn't actually own the tabs, the `_tabs` vector does.

We'll create a new class `TabSwitcherControl` inside of TerminalPage, which will be initialized with a `vector<>&`.
This is because it'll take in either `_tabs` or `_mruTabs` depending on what order the user would like the tabs to be displayed.
Whenever the user changes the setting that controls the tab order, it'll update to get a reference to the correct vector.

This vector of tabs will be used to create a ListView, so that the UI would diplay a vertical list of tabs.
Each element in the ListView would be associated with a Tab.
This ListView would be hidden until the user presses a keybinding to show the ListView.

The `TabSwitcherControl` will be able to call any tab's `SetFocused` function to bring the tab into focus.
The TabView's `SelectionChanged` handler listens for events where a new terminal control comes into focus, in which case `TerminalPage::_OnTabSelectionChanged` will be called.
By updating the MRU here, we can be sure that changing tabs from the TabSwitcher, clicking on a tab, or nextTab/prevTab-ing will keep the MRU up-to-date. Adding or closing tabs are handled in `_OpenNewTab` and `_CloseFocusedTab`, which will need to be modified to update both `_tabs` and `_mruTabs`.

## UI/UX Design

First, we'll give a quick overview of how the tab switcher UI would look like, then we'll dive into more detail on how the user would interact with the switcher.

The tab switcher will appear as a box in the center of the terminal. It'll have a maximum and minimum height and width. We want the switcher to stretch, but it shouldn't stretch to take up the entirety of the terminal. If the user has a ton of tabs or has really long tab names, the box should still be fairly contained and shouldn't run wild.
In the box will be a vertical list of tabs with their titles and their assigned number for quick switching, and only one line will be highlighted to signify that tab is currently selected.
There will only be 9 numbered tabs for quick switching, and the rest of the tabs will simply have an empty space where a number would be.

The list would look (roughly) like this:
```
1 cmd (highlighted)
2 cmd
3 Windows PowerShell
4 MSYS:/c/Users/booboo
5 Git Bash
6 cmd
7 /c/
8 /d/
9 /e/
  /f/
  /g/
  /h/
```

The highlighted line can move up or down, and if the user moves up while the highlighted line is already at the top of the list, the highlight will wrap around to the bottom of the list.
Similarly, it will wrap to the top if the highlight is at the bottom of the list and the user moves down.

If there's more tabs than the UI can display, the list of tabs will scroll up/down as the user keeps iterating up/down.
However, the number column does not move. The first nine tabs in the list will always be numbered.

To give an example of what happens after scrolling past the end, imagine a user is starting from the state in the mock above.
The user then iterates down past the end of the visible list four times. The below mock shows the result.

```
1 Git Bash
2 cmd
3 /c/
4 /d
5 /e/
6 /f/
7 /g/
8 /h/
9 /i/
  /j/
  /k/
  /l/ (highlighted)
```

`Git Bash` is now associated with number 1, and the `cmd`, `cmd`, `Windows Powershell`, and `MSYS:/c/Users/booboo` tabs are no longer visible.

### Using the Switcher

#### Opening the Tab Switcher

The user will press a keybinding named `OpenTabSwitcher` to bring up the UI.
The user will be able to change it to whatever they like. There will also be an optional `anchor` arg that may be provided to this keybinding.

- `OpenTabSwitcher`
    - _default_ = <kbd>ctrl+tab</kbd>
    - _available args_: `anchor`

If this arg is given, the UI will dismiss on the release of the _first_ key of this keybinding. This is because the user could have the key <kbd>q</kbd> or could have <kbd>ctrl+tab</kbd> set to open the switcher. If the user has a single key as the keybinding, then it makes sense that on the release of that one key, the UI would dismiss. If there's two keys, we don't want the user to have to hold both keys down to keep the UI open. Just the first key, namely <kbd>ctrl</kbd>, would act as the `anchor`.

#### Keeping it open

We use the term `anchor` to illustrate the idea that the UI stays visible as long as something is "anchoring" it down.

So, when the `OpenTabSwitcher` keybinding is given the `anchor` arg, the _first_ key of the keybinding will act as the `anchor` key.
Holding that`anchor` key down will keep the switcher visible, and once the `anchor` key is released, the switcher will dismiss.

If `OpenTabSwitcher` is not given the `anchor` arg, the switcher will stay visible even after the release of the key/keychord.

#### Switching through Tabs

The user will be able to navigate through the switcher with the following keybindings:

- `tabSwitchDown`: <kbd>tab</kbd> or <kbd>downArrow</kbd>
- `tabSwitchUp`: <kbd>shift+tab</kbd> or <kbd>upArrow</kbd>

As the user is cycling through the tab list, the selected tab will be highlighted but the terminal won't actually switch to the selected tab.

#### Closing the Switcher and Bringing a Tab into Focus

There are two _dismissal_ keybinds:

1. <kbd>enter</kbd> : brings the currently selected tab into focus and dismisses the UI.
2. <kbd>esc</kbd> : dismisses the UI without changing tab focus.

The following are ways a user can dismiss the UI, whether or not the `Anchor` arg is provided to `OpenTabSwitcher`.

1. The user can press a number associated with a tab to instantly switch to the tab and dismiss the switcher.
2. The user can click on a tab to instantly switch to the tab and dismiss the switcher.
3. The user can click outside of the UI to dismiss the switcher without bringing the selected tab into focus.
4. The user can press any of the dismissal keybinds.

If the `Anchor` arg is provided, then in addition to the above methods, the UI will dismiss upon the release of the `Anchor` key.

Pressing the `OpenTabSwitcher` keychord again will _not_ close the Switcher.

### Most Recently Used Order

We'll provide a setting that will allow the list of tabs to be presented in either _in-order_ (how the tabs are ordered on the tab bar), or _Most Recently Used Order_ (MRU).
MRU means that the tab that the terminal most recently visited will be on the top of the list, and the tab that the terminal has not visited for the longest time will be on the bottom.

### Numbered Tabs

Similar to how the user can currently switch to a particular tab with a combination of keys such as <kbd>ctrl+shift+1</kbd>, we want to have the tab switcher provide a number to the first nine tabs (1-9) in the list for quick switching. If there are more than nine tabs in the list, then the rest of the tabs will not have a number assigned.

## Capabilities

### Accessibility

TODO:

### Security

This shouldn't introduce any security issues.

### Reliability

- MRU ordering should be accurate, and should always be updated on every tab focus.
- Reordering tabs on the tab bar shouldn't change the MRU order.

### Compatibility

- The existing way of navigating horizontally through the tabs on the tab bar should not break. These should also be separate keybindings from the keybindings associated with using the tab switcher.
(TODO: Would this just be too many different keybindings?)
- The implementation should also take into account the addition of other features on top of this UI, such as Pane Navigation described below.

### Performance, Power, and Efficiency

- Since each tab focus change, tab addition, and tab close will need to update the MRU, we just need to be sure that updating the MRU isn't too slow.

## Potential Issues

We'll need to be wary about how the UI is presented depending on different sizes of the terminal.

Visual Studio's tab switcher is a fixed size, and is always in the middle. Even when the VS window is smaller than the tab switcher size, the tab switcher will show up larger than the VS window itself.

![Small Visual Studio Without Tab Switcher](img/VSMinimumSize.png)
![Small Visual Studio With Tab Switcher](img/VSMinimumSizeWithTabSwitcher.png)

Visual Studio Code only allows the user to shrink the window until it hits a minimum width and height. This minimum width and height gives its tab switcher enough space to show a meaningful amount of information.

![Small Visual Studio Code with Tab Switcher](img/VSCodeMinimumTabSwitcherSize.png)

Terminal can't really replicate Visual Studio's version of the tab switcher in this situation. The TabSwitcher needs to be contained within the Terminal.
So, if the TabSwitcher is always centered and has a percentage padding from the borders of the Terminal, it'll shrink as Terminal shrinks.
One thing I'm not too sure about is when the Terminal window is so small that not even a single tab title will be visible. Should it just not display the UI?

## Future considerations

### Pane Navigation

@zadiji-msft in [#1502] brought up the idea of pane navigation in additon to tab navigation, inspired by tmux.

![Tmux Tab and Pane Switching](img/tmuxPaneSwitching.png)

Tmux allows the user to navigate directly to a pane and even give a preview of the pane. It's a pretty cool way to let the user pin point exactly which pane to switch focus to.

### Tab Search by Name/Title

This is something that would be nice to have, especially when there's a large list of tabs you're cycling through.

### Tab Preview on Hover

Instead of just highlighting the tab name in the tab switcher, it would also show a preview of the tab contents in the terminal.
I'm not too sure about the implementation details of this one, especially around MRU. Tab Preview should not affect MRU, since
the user never actually wanted to _focus_ on the tab just yet.

## Resources

Feature Request: An advanced tab switcher [#1502]  
Ctrl+Tab toggle between last two windows like Alt+Tab [#973]  
The Command Palette Thread [#2046]

<!-- Footnotes -->
[#973]: https://github.com/microsoft/terminal/issues/973
[#1502]: https://github.com/microsoft/terminal/issues/1502
[#2046]: https://github.com/microsoft/terminal/issues/2046
