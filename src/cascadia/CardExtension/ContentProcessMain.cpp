// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "resource.h"
#include "../types/inc/User32Utils.hpp"
#include <WilErrorReporting.h>

using namespace winrt;

winrt::hstring semiAdvancedCard{
    LR"({
  "$schema": "http://adaptivecards.io/schemas/adaptive-card.json",
  "type": "AdaptiveCard",
  "version": "1.0",
  "body": [
    {
      "type": "Container",
      "items": [
        {
          "type": "TextBlock",
          "text": "Publish Adaptive Card schema",
          "weight": "bolder",
          "size": "medium"
        },
        {
          "type": "ColumnSet",
          "columns": [
            {
              "type": "Column",
              "width": "auto",
              "items": [
                {
                  "type": "Image",
                  "url": "https://pbs.twimg.com/profile_images/3647943215/d7f12830b3c17a5a9e4afcc370e3a37e_400x400.jpeg",
                  "size": "small",
                  "style": "person"
                }
              ]
            },
            {
              "type": "Column",
              "width": "stretch",
              "items": [
                {
                  "type": "TextBlock",
                  "text": "Matt Hidinger",
                  "weight": "bolder",
                  "wrap": true
                },
                {
                  "type": "TextBlock",
                  "spacing": "none",
                  "text": "Created {{DATE(2017-02-14T06:08:39Z, SHORT)}}",
                  "isSubtle": true,
                  "wrap": true
                }
              ]
            }
          ]
        },
        {
          "type": "TextBlock",
          "text": "Now that we have defined the main rules and features of the format, we need to produce a schema and publish it to GitHub. The schema will be the starting point of our reference documentation.",
          "wrap": true
        },
        {
            "type": "Input.Text",
            "id": "comment",
            "isMultiline": true,
            "placeholder": "Enter your comment"
        }
      ]
    }
  ],
  "actions": [
    {
    "type": "Action.Submit",
    "title": "OK"
    },
    {
      "type": "Action.OpenUrl",
      "title": "View",
      "url": "https://adaptivecards.io"
    },
    {
      "type": "Action.OpenUrl",
      "title": "Hey look I came from an extension",
      "url": "https://adaptivecards.io"
    }
  ]
})"
};
namespace winrt::CardExtension::implementation
{
    class MyCard : public winrt::implements<MyCard, winrt::TerminalApp::ICardExtension>
    {
    public:
        winrt::hstring GetJson()
        {
            return semiAdvancedCard;
        }
    };
}

// We keep a weak ref to our ContentProcess singleton here.
// Why?
//
// We need to always return the _same_ ContentProcess when someone comes to
// instantiate this class. So we want to track the single instance we make. We
// also want to track when the last outstanding reference to this object is
// removed. If we're keeping a strong ref, then the ref count will always be > 1

winrt::weak_ref<winrt::TerminalApp::ICardExtension> g_weak{ nullptr };
wil::unique_event g_canExitThread;

struct ExtensionFactory : implements<ExtensionFactory, IClassFactory>
{
    ExtensionFactory(winrt::guid g) :
        _guid{ g } {};

    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        if (!g_weak)
        {
            // Instantiate the ContentProcess here

            auto strong{ winrt::make<winrt::CardExtension::implementation::MyCard>() };

            // Now, create a weak ref to that ContentProcess object.
            winrt::weak_ref<winrt::TerminalApp::ICardExtension> weak{ strong };

            // Stash away that weak ref for future callers.
            g_weak = weak;
            return strong.as(iid, result);
        }
        else
        {
            auto strong = g_weak.get();
            // !! LOAD BEARING !! If you set this event in the _first_ branch
            // here, when we first create the object, then there will be _no_
            // referernces to the ContentProcess object for a small slice. We'll
            // stash the ContentProcess in the weak_ptr, and return it, and at
            // that moment, there will be 0 outstanding references, it'll dtor,
            // and we'll ExitProcess.
            //
            // Instead, set the event here, once there's already a reference
            // outside of just the weak one we keep. Experimentation showed this
            // waw always hit when creating the ContentProcess at least once.
            g_canExitThread.SetEvent();
            return strong.as(iid, result);
        }
    }

    HRESULT __stdcall LockServer(BOOL) noexcept final
    {
        return S_OK;
    }

private:
    winrt::guid _guid;
};

//winrt::guid GUID(const winrt::hstring& s)
//{
//    struct __declspec(uuid(s))
//    {
//    } foo;
//    __uuidof(decltype(foo));
//}
//
//#define GUID(guid, name)               \
//    struct __declspec(uuid(guid)) \
//    {                                  \
//    } foo;                                 \
//    __uuidof(decltype(foo));

struct __declspec(uuid("76b3f18c-89ed-4a29-98ac-2096395e7c32")) hack
{
};

static void doContentProcessThing(const HANDLE& eventHandle)
{
    // 76b3f18c-89ed-4a29-98ac-2096395e7c32
    winrt::guid extensionGuid{ __uuidof(hack) };
    // winrt::guid test{ GUID("76b3f18c-89ed-4a29-98ac-2096395e7c32") };
    // !! LOAD BEARING !! - important to be a MTA for these COM calls.
    winrt::init_apartment();
    DWORD registrationHostClass{};
    check_hresult(CoRegisterClassObject(extensionGuid,
                                        make<ExtensionFactory>(extensionGuid).get(),
                                        CLSCTX_LOCAL_SERVER,
                                        REGCLS_MULTIPLEUSE,
                                        &registrationHostClass));

    // Signal the event handle that was passed to us that we're now set up and
    // ready to go.
    SetEvent(eventHandle);
    CloseHandle(eventHandle);
}

void TryRunAsContentProcess()
{
    HANDLE eventHandle{ INVALID_HANDLE_VALUE };
    g_canExitThread = wil::unique_event{ CreateEvent(nullptr, true, false, nullptr /*L"ContentProcessReady"*/) };

    doContentProcessThing(eventHandle);

    WaitForSingleObject(g_canExitThread.get(), INFINITE);
    // This is the conhost thing - if we ExitThread the main thread, the
    // other threads can keep running till one calls ExitProcess.
    ExitThread(0);
}
