---
author: Kaiyu Wang KaiyuWang16/kawa@microsoft.com
created on: 2019-09-03
last updated: 2019-09-03
issue id: #1043
---

# Spec Title

## Abstract

This spec is for task #1043 “Be able to set an initial position for the terminal”. It goes over the details of a new feature that allows users to set the initial position and size of the terminal. Expected behavior and design of this feature is included. Besides, future possible follow-up works are also addressed.

## Inspiration

The idea is to allow users to set the initial position of the Terminal when they launch it, prevent the Terminal from appearing on unexpected position (e.g. outside of the screen bounds). We are also going to let users choose to maximize the window when they launch it.

## Solution Design

For now, the Terminal window is put on a default initial position. The program uses CW_USEDEFAULT in the screen coordinates for top-left corner. We have two different types of window – client window and non-client window. However, code path for window creation (WM_CREATE message is shared by the two types of windows) are almost the same for the two types of windows, except that there are some differences in calculation of the width and height of the window.

Four new properties should be added in the json settings file:

**useDefaultInitialPos**: true or false. Determine if we use CW_USEDEFAULT as the initial position of the Terminal window. If true, we ignore initialX and initially. Default is true;

**initialX**: int. If useDefaultInitialPos is false, this set the initial horizontal position of the top-left corner of the window. Default is 0.

**initiallY**: int. If useDefaultInitialPos is false, this set the initial vertical position of the top-left corner of the window. Default is 0.

**maximizeWhenLaunch**: true or false. Determine if the window should be maximized upon launch. If true, we ignore initialCols and initialRows. Default is false.

The flow chart of this process:

![Sol Design](images/FlowChart.png)

Step 2 to 6 should be done in AppHost::_HandleCreateWindow, which is consistent to the current code.

In step 4, we may need to consider the dpi of the current monitor and multi-monitor scenario when calculating the initial position of the window.

## UI/UX Design

Upon successful implementation, the user is able to add new properties to the json profile file, which is illustrated in the picture below:

![Sol Design](images/ProfileSnapshot.png)

The rest of the UI will be the same of the current Terminal experience, except that the initial position may be different.

## Capabilities

### Accessibility

This feature will not impact accessibility of Windows Terminal.

### Security

This should not introduce any new security issues.

### Reliability

This new feathre allows the users to set custom initial position of the Terminal App window, which helps them to avoid embarassing window launch situation such as launching outside the screen bounds. Thus, it improves the reliability.

### Compatibility

This feature won't break existing features of Terminal.

### Performance, Power, and Efficiency

More data reading and calculation will be included in Terminal Launch process, which may inversely influence the launch time. However, the impact is trivial. 

## Potential Issues

1. The dpi of the monitor may impact the initial position even if the json profile settings are the same. We need to consider the dpi of the user's monitor and calculate the initial position in the current screen coordinates. Testing with different monitor dpi are also necessary.

2. We need to consider multi-monitor scenario. If the user has multiple monitors, we must guarantee that the Terminal could be iniitalized as expected. More discussions on what should we do in this scenario with the github community is needed. 

## Future considerations

For now, this feature only allows the user to set initial positon and choose whether to maximize the window when launch. In the future, we may consider follow-up features like:

1. Save the position of the Terminal on exit, and restore the position on the next launch. This could be a true/false feature that users could choose to set.

2. We may need to consider multiple Terminal windows scenario. If the user opens multiple Terminal windows, then we need to consider how to save and restore the position. 

## Resources

Github issue:
https://github.com/microsoft/terminal/issues/1043
