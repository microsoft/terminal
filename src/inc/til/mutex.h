// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    // shared_mutex is a std::shared_mutex which also contains the data it's protecting.
    // It only allows access to the underlying data by locking the mutex and thus
    // ensures you don't forget to do so, unlike with std::mutex/std::shared_mutex.
    template<typename T>
    class shared_mutex
    {
    public:
        // An exclusive, read/write reference to a til::shared_mutex's underlying data.
        // If you drop the guard the mutex is unlocked.
        class guard
        {
            friend class shared_mutex;

            guard(T& data, std::shared_mutex& mutex) :
                _data(data),
                _mutex(mutex)
            {
                _mutex.lock();
            }

        public:
            ~guard() noexcept
            {
                _mutex.unlock();
            }

            guard(const guard&) = delete;
            guard& operator=(const guard&) = delete;

            T* operator->() noexcept
            {
                return &_data;
            }

        private:
            // We could reduce this to a single pointer member,
            // by storing a reference to the outer mutex& class instead
            // and accessing its private members as a friend class.
            // The reason we don't do that is because C++ compilers usually
            // don't "cache" dereferenced pointers between mutations.
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
            std::shared_mutex& _mutex;
        };

        // A shared, read-only reference to a til::shared_mutex's underlying data.
        // If you drop the shared_guard the mutex is unlocked.
        class shared_guard
        {
            friend class shared_mutex;

            shared_guard(const T& data, std::shared_mutex& mutex) :
                _data(data),
                _mutex(mutex)
            {
                _mutex.lock_shared();
            }

        public:
            ~shared_guard() noexcept
            {
                _mutex.unlock_shared();
            }

            shared_guard(const guard&) = delete;
            shared_guard& operator=(const shared_guard&) = delete;

            const T* operator->() const noexcept
            {
                return &_data;
            }

        private:
            const T& _data;
            std::shared_mutex& _mutex;
        };

        shared_mutex() = default;

        template<typename... Args>
        shared_mutex(Args&&... init) :
            _data{ std::forward<Args>(args)... }
        {
        }

        // Acquire an exclusive, read/write reference to T.
        // For instance:
        //   .lock()->foo = bar;
        guard lock() const noexcept
        {
            return { _data, _mutex };
        }

        // Acquire a shared, read-only reference to T.
        // For instance:
        //   bar = .lock_shared()->foo;
        shared_guard lock_shared() const noexcept
        {
            return { _data, _mutex };
        }

    private:
        mutable T _data{};
        mutable std::shared_mutex _mutex;
    };
} // namespace til
