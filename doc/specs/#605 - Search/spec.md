---
author: Kaiyu Wang KaiyuWang16/kawa@microsoft.com
created on: 2019-10-16
last updated: 2019-10-16
issue id: #605
---

# Search in Terminal

## Abstract

This spec is for feature request #605 "Search". It goes over the details of a new feature that allows users to search text in Terminal, within one tab or from all tabs. Expected behavior and design of this feature is included. Besides, future possible follow-up works are also addressed. 

## Inspiration

One of the superior features of iTerm2 is it's content search. The search comes in two variants: search from active tab and search from all tabs. In almost any editor, there is an roughly equivalent string search. We also want to realize search experience in Terminal. There will be two variants, search within one tab or from multiple tabs. We will start with one-tab search implementation. 

## Solution Design

Our ultimate goal is to provide both search within one tab and search from all tabs experiences. But we can start with one-tab search. The search experience should have following features:

1. The search is triggered by keybindings "Ctrl + F". This coincides with other editors. In the future we will also consider adding a "Find" in the dropdown menu. 
2. The user search in a 
3. We can have multiple search methods. The simplest one is text exact match with case insensitive. Other match methods include case-sensitive exact match and regex match. In the first phrase, we will focus on case-insensitive exact match. 
4. The search starts from the line that the cursor is on, and go up from most recent lines. We automatically go around if we reach the end of the text buffer. 
5. The search dialog should not block terminal's view. 

Conhost already has a module for search. It realizes case sensitive or insensitive exact text match search, and it provides methods to select and color found word. However, we want to make search as a shared component between Terminal and Console host. Now search module is part of Conhost, and its dependencies include BufferOut and some other types in ConHost such as SCREEN_INFORMATION. In order to make Search a shared component, we need to remove its dependency on ConHost types. BufferOut is already a shared component, but we need to make sure there is no other Conhost dependency. 

## UI/UX Design

[comment]: # What will this fix/feature look like? How will it affect the end user?

## Capabilities

[comment]: # Discuss how the proposed fixes/features impact the following key considerations:

### Accessibility

This feature could help multitaksing terminal users. 

### Security

This should not introduce any new security issues.

### Reliability

This feature enable users to search for text in the terminal input/output history. This is a widely-used feature in most editors and thus improve the reliability of Terminal once added. 

### Compatibility

This feature won't break existing features of Terminal.

### Performance, Power, and Efficiency

This feature only launch in need. It does not impact the performance of Terminal. 

## Potential Issues

1. The search bar should not block command line view. 
2. Search should not block any program the Terminal is currently executing.

## Future considerations

In version 1, we want realize a case sensitive/insensitive exact text match. But we may consider the following features in version 2:

1. Add "Find" button in dropdown menu to trigger search. 
2. Search from all tabs. For Version 1 we just want to realize search within one tab. However, the community also requests search from all tabs. We put in our goals for Version 2. 
3. Regular experssion match. 
4. Search history.
5. High-light while you type. 
 

## Resources

Github Issue: https://github.com/microsoft/terminal/issues/605
