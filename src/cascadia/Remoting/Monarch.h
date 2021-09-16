// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Monarch.g.h"
#include "Peasant.h"
#include "../cascadia/inc/cppwinrt_utils.h"
#include "WindowActivatedArgs.h"

// We sure different GUIDs here depending on whether we're running a Release,
// Preview, or Dev build. This ensures that different installs don't
// accidentally talk to one another.
//
// * Release: {06171993-7eb1-4f3e-85f5-8bdd7386cce3}
// * Preview: {04221993-7eb1-4f3e-85f5-8bdd7386cce3}
// * Dev:     {08302020-7eb1-4f3e-85f5-8bdd7386cce3}
constexpr GUID Monarch_clsid
{
#if defined(WT_BRANDING_RELEASE)
    0x06171993,
#elif defined(WT_BRANDING_PREVIEW)
    0x04221993,
#else
    0x08302020,
#endif
        0x7eb1,
        0x4f3e,
    {
        0x85, 0xf5, 0x8b, 0xdd, 0x73, 0x86, 0xcc, 0xe3
    }
};

namespace RemotingUnitTests
{
    class RemotingTests;
};

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct Monarch : public MonarchT<Monarch>
    {
        Monarch();
        Monarch(const uint64_t testPID);
        ~Monarch();

        uint64_t GetPID();

        uint64_t AddPeasant(winrt::Microsoft::Terminal::Remoting::IPeasant peasant);
        void SignalClose(const uint64_t peasantId);

        uint64_t GetNumberOfPeasants();

        winrt::Microsoft::Terminal::Remoting::ProposeCommandlineResult ProposeCommandline(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args);
        void HandleActivatePeasant(const winrt::Microsoft::Terminal::Remoting::WindowActivatedArgs& args);
        void SummonWindow(const Remoting::SummonWindowSelectionArgs& args);

        void SummonAllWindows();
        bool DoesQuakeWindowExist();
        Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo> GetPeasantInfos();

        TYPED_EVENT(FindTargetWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs);
        TYPED_EVENT(ShowNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(HideNotificationIconRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(WindowCreated, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(WindowClosed, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(QuitAllRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);

    private:
        uint64_t _ourPID;

        uint64_t _nextPeasantID{ 1 };
        uint64_t _thisPeasantID{ 0 };

        winrt::com_ptr<IVirtualDesktopManager> _desktopManager{ nullptr };

        std::unordered_map<uint64_t, winrt::Microsoft::Terminal::Remoting::IPeasant> _peasants;

        std::vector<Remoting::WindowActivatedArgs> _mruPeasants;

        winrt::Microsoft::Terminal::Remoting::IPeasant _getPeasant(uint64_t peasantID);
        uint64_t _getMostRecentPeasantID(bool limitToCurrentDesktop, const bool ignoreQuakeWindow);
        uint64_t _lookupPeasantIdForName(std::wstring_view name);

        void _peasantWindowActivated(const winrt::Windows::Foundation::IInspectable& sender,
                                     const winrt::Microsoft::Terminal::Remoting::WindowActivatedArgs& args);
        void _doHandleActivatePeasant(const winrt::com_ptr<winrt::Microsoft::Terminal::Remoting::implementation::WindowActivatedArgs>& args);
        void _clearOldMruEntries(const uint64_t peasantID);

        void _forAllPeasantsIgnoringTheDead(std::function<void(const winrt::Microsoft::Terminal::Remoting::IPeasant&, const uint64_t)> callback,
                                            std::function<void(const uint64_t)> errorCallback);

        void _identifyWindows(const winrt::Windows::Foundation::IInspectable& sender,
                              const winrt::Windows::Foundation::IInspectable& args);

        void _renameRequested(const winrt::Windows::Foundation::IInspectable& sender,
                              const winrt::Microsoft::Terminal::Remoting::RenameRequestArgs& args);

        void _handleQuitAll(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Windows::Foundation::IInspectable& args);

        // Method Description:
        // - Helper for doing something on each and every peasant.
        // - We'll try calling func on every peasant.
        // - If the return type of func is void, it will perform it for all peasants.
        // - If the return type is a boolean, we'll break out of the loop once func
        //   returns false.
        // - If any single peasant is dead, then we'll call onError and then add it to a
        //   list of peasants to clean up once the loop ends.
        // Arguments:
        // - func: The function to call on each peasant
        // - onError: The function to call if a peasant is dead.
        // Return Value:
        // - <none>
        template<typename F, typename T>
        void _forEachPeasant(F&& func, T&& onError)
        {
            using Map = decltype(_peasants);
            using R = std::invoke_result_t<F, Map::key_type, Map::mapped_type>;
            static constexpr auto IsVoid = std::is_void_v<R>;

            std::vector<uint64_t> peasantsToErase;

            for (const auto& [id, p] : _peasants)
            {
                try
                {
                    if constexpr (IsVoid)
                    {
                        func(id, p);
                    }
                    else
                    {
                        if (!func(id, p))
                        {
                            break;
                        }
                    }
                }
                catch (const winrt::hresult_error& exception)
                {
                    onError(id);

                    if (exception.code() == 0x800706ba) // The RPC server is unavailable.
                    {
                        peasantsToErase.emplace_back(id);
                    }
                    else
                    {
                        LOG_CAUGHT_EXCEPTION();
                        throw;
                    }
                }
            }

            for (const auto& id : peasantsToErase)
            {
                _peasants.erase(id);
                _clearOldMruEntries(id);
            }
        }

        friend class RemotingUnitTests::RemotingTests;
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(Monarch);
}
