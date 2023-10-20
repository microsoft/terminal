// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace til
{
    namespace details
    {
        template<typename T, typename Lock>
        class shared_mutex_guard
        {
        public:
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'shared_mutex>()' which may throw exceptions (f.6).)
            shared_mutex_guard(T& data, std::shared_mutex& mutex) noexcept :
                _data{ data },
                _lock{ mutex }
            {
            }

            shared_mutex_guard(const shared_mutex_guard&) = delete;
            shared_mutex_guard& operator=(const shared_mutex_guard&) = delete;

            shared_mutex_guard(shared_mutex_guard&&) = default;
            shared_mutex_guard& operator=(shared_mutex_guard&&) = default;

            [[nodiscard]] constexpr T* operator->() const noexcept
            {
                return &_data;
            }

            [[nodiscard]] constexpr T& operator*() const& noexcept
            {
                return _data;
            }

            [[nodiscard]] constexpr T&& operator*() const&& noexcept
            {
                return std::move(_data);
            }

        private:
            // We could reduce this to a single pointer member,
            // by storing a reference to the til::shared_mutex& class
            // and accessing its private members as a friend class.
            // But MSVC doesn't support strict aliasing. Nice!
            //
            // For instance if we had:
            //   struct foo { int a, b; };
            //   struct bar { foo& f; };
            //
            //   void test(bar& b) {
            //       b.f.a = 123;
            //       b.f.b = 456;
            //   }
            //
            // This would generate the following suboptimal assembly despite /O2:
            //   mov rax, QWORD PTR [rcx]
            //   mov DWORD PTR [rax], 123
            //
            //   mov rax, QWORD PTR [rcx]
            //   mov DWORD PTR [rax+4], 456
            T& _data;
            Lock _lock;
        };
    } // namespace details

    // shared_mutex is a std::shared_mutex which also contains the data it's protecting.
    // It only allows access to the underlying data by locking the mutex and thus
    // ensures you don't forget to do so, unlike with std::mutex/std::shared_mutex.
    template<typename T>
    class shared_mutex
    {
    public:
        // An exclusive, read/write reference to a til::shared_mutex's underlying data.
        // If you drop the guard the mutex is unlocked.
        using guard = details::shared_mutex_guard<T, std::unique_lock<std::shared_mutex>>;

        // A shared, read-only reference to a til::shared_mutex's underlying data.
        // If you drop the shared_guard the mutex is unlocked.
        using shared_guard = details::shared_mutex_guard<const T, std::shared_lock<std::shared_mutex>>;

        shared_mutex() = default;

        template<typename... Args>
        shared_mutex(Args&&... args) :
            _data{ std::forward<Args>(args)... }
        {
        }

        // Acquire an exclusive, read/write reference to T.
        // For instance:
        //   .lock()->foo = bar;
        [[nodiscard]] guard lock() const noexcept
        {
            return { _data, _mutex };
        }

        // Acquire a shared, read-only reference to T.
        // For instance:
        //   bar = .lock_shared()->foo;
        [[nodiscard]] shared_guard lock_shared() const noexcept
        {
            return { _data, _mutex };
        }

    private:
        mutable T _data{};
        mutable std::shared_mutex _mutex;
    };
} // namespace til
