// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    template<typename T>
    struct ConnectionStateHolder
    {
    public:
        ConnectionState State() const noexcept { return _connectionState; }
        TYPED_EVENT(StateChanged, ITerminalConnection, winrt::Windows::Foundation::IInspectable);

    protected:
        // Method Description:
        // - Attempt to transition to and signal the specified connection state.
        //   The transition will only be effected if the state is "beyond" the current state.
        // Arguments:
        // - state: the new state
        // Return Value:
        //   Whether we've successfully transitioned to the new state.
        bool _transitionToState(const ConnectionState state) noexcept
        {
            {
                std::lock_guard<std::mutex> stateLock{ _stateMutex };
                // only allow movement up the state gradient
                if (state < _connectionState)
                {
                    return false;
                }
                _connectionState = state;
            }
            // Dispatch the event outside of lock.
            _StateChangedHandlers(*static_cast<T*>(this), nullptr);
            return true;
        }

        // Method Description:
        // - Returns whether the state is one of the N specified states.
        // Arguments:
        // - "...": the states
        // Return Value:
        //   Whether we're in one of the states.
        template<typename... Args>
        bool _isStateOneOf(Args&&... args) const noexcept
        {
            static_assert((... && std::is_same<Args, ConnectionState>::value), "all queried connection states must be from the ConnectionState enum");
            std::lock_guard<std::mutex> stateLock{ _stateMutex };
            // dark magic: this is a C++17 fold expression that expands into
            // state == 1 || state == 2 || state == 3 ...
            return (... || (_connectionState == args));
        }

        // Method Description:
        // - Returns whether the state has reached or surpassed the specified state.
        // Arguments:
        // - state; the state to check against
        // Return Value:
        //   Whether we're at or beyond the specified state
        bool _isStateAtOrBeyond(const ConnectionState state) const noexcept
        {
            std::lock_guard<std::mutex> stateLock{ _stateMutex };
            return _connectionState >= state;
        }

        // Method Description:
        // - (Convenience:) Returns whether we're "connected".
        // Return Value:
        //   Whether we're "connected"
        bool _isConnected() const noexcept
        {
            return _isStateOneOf(ConnectionState::Connected);
        }

    private:
        std::atomic<ConnectionState> _connectionState{ ConnectionState::NotConnected };
        mutable std::mutex _stateMutex;
    };
}
