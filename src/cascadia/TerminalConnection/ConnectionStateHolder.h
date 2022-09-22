// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    template<typename T>
    struct ConnectionStateHolder
    {
    public:
        ConnectionState State() const noexcept { return _connectionState; }
        TYPED_EVENT(StateChanged, ITerminalConnection, winrt::Windows::Foundation::IInspectable);

    protected:
#pragma warning(push)
#pragma warning(disable : 26447) // Analyzer is still upset about noexcepts throwing even with function level try.
        // Method Description:
        // - Attempt to transition to and signal the specified connection state.
        //   The transition will only be effected if the state is "beyond" the current state.
        // Arguments:
        // - state: the new state
        // Return Value:
        //   Whether we've successfully transitioned to the new state.
        bool _transitionToState(const ConnectionState state) noexcept
        try
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
#pragma warning(suppress : 26491) // We can't avoid static_cast downcast because this is template magic.
            _StateChangedHandlers(*static_cast<T*>(this), nullptr);
            return true;
        }
        CATCH_FAIL_FAST()

        // Method Description:
        // - Returns whether the state is one of the N specified states.
        // Arguments:
        // - "...": the states
        // Return Value:
        //   Whether we're in one of the states.
        template<typename... Args>
        bool _isStateOneOf(const Args&&... args) const noexcept
        try
        {
            // Dark magic! This function uses C++17 fold expressions. These fold expressions roughly expand as follows:
            // (... OP (expression_using_args))
            // into ->
            // expression_using_args[0] OP expression_using_args[1] OP expression_using_args[2] OP (etc.)
            // We use that to first check that All Args types are ConnectionState (type[0] == ConnectionState && type[1] == ConnectionState && etc.)
            // and then later to check that the current state is one of the passed-in ones:
            // (_state == args[0] || _state == args[1] || etc.)
            static_assert((... && std::is_same<Args, ConnectionState>::value), "all queried connection states must be from the ConnectionState enum");
            std::lock_guard<std::mutex> stateLock{ _stateMutex };
            return (... || (_connectionState == args));
        }
        CATCH_FAIL_FAST()

        // Method Description:
        // - Returns whether the state has reached or surpassed the specified state.
        // Arguments:
        // - state; the state to check against
        // Return Value:
        //   Whether we're at or beyond the specified state
        bool _isStateAtOrBeyond(const ConnectionState state) const noexcept
        try
        {
            std::lock_guard<std::mutex> stateLock{ _stateMutex };
            return _connectionState >= state;
        }
        CATCH_FAIL_FAST()
#pragma warning(pop)

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
