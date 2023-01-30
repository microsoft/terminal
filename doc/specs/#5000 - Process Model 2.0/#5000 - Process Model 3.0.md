---
author: Mike Griese @zadjii-msft
created on: 2023-01-27
last updated: 2023-01-27
issue id: #5000
---

# Windows Terminal Process Model 3.0

Everything is one process.

WIP branch: https://github.com/microsoft/terminal/compare/main...dev/migrie/f/process-model-v3-test-0

* [ ] One `App` per process.
* [ ] One `AppHost` per thread.
  * Which means one `AppLogic` per thread/window.
* [ ] `ContentManager` for storing GUID -> `ControlInteractivity`'s
  * Idea: Let's make it `map<guid, IInspectable>`, and then QI to get the `FrameworkElement`?
  * Remoting could host it then
* [ ] `WindowManager` tries to get the monarch. If there isn't one, then Terminal isn't running.
  * [ ] `Don't register as a `Monarch` host if we found one.
  * [ ] Wait to register as the monarch till we determine that we want to actually make a window.
  * [ ] `WindowManager::ProposeCommandline`: we only ever need to create a new window if we are the `Monarch`.
* [ ]
* [ ]
* [ ]
* [ ]
