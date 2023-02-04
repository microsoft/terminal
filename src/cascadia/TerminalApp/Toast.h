/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Toast.h

Module Description:
- This is a helper class for wrapping a TeachingTip with a timer to
  automatically dismiss it. Callers should add the TeachingTip wherever they'd
  like in the UI tree, then wrap that TeachingTip in a toast like so:

```
std::unique_ptr<Toast> myToast = std::make_unique<Toast>(MyTeachingTip());
```
- Then, you can show the TeachingTip with

```
myToast->Open();
```

  which will open the tip and close it after a brief timeout.

--*/

#pragma once
#include "pch.h"

class Toast : public std::enable_shared_from_this<Toast>
{
public:
    Toast(const MUXC::TeachingTip& tip);
    void Open();

private:
    MUXC::TeachingTip _tip;
    WUX::DispatcherTimer _timer;
};
