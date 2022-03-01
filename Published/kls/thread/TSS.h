/*
* Copyright (c) 2022 DWVoid and Infinideastudio Team
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <cstdint>
#include "kls/essential/Memory.h"

namespace kls::thread {
    namespace tss::detail {
        struct Cleanup {
            void (*fn)(void*, void*) noexcept;

            void* user;

            void operator()(void* p) const noexcept { fn(p, user); }

            explicit operator bool() const noexcept { return fn; }
        };

        constexpr uint32_t invalid_key = 0xffffffff;

        uint32_t create(Cleanup callback);

        void remove(uint32_t key) noexcept;

        void* get(uint32_t key) noexcept;

        void set(uint32_t key, void* p);

        template<class T>
        class PointerBase {
        public:
            using element_type = T;

            explicit PointerBase(Cleanup clean) : m_key(create(clean)) {}

            PointerBase(PointerBase const&) = delete;

            PointerBase& operator=(PointerBase const&) = delete;

            PointerBase(PointerBase&& other) noexcept : m_key(other.m_key) { other.m_key = invalid_key; }

            PointerBase& operator=(PointerBase&& other) noexcept {
                if (m_key != invalid_key) remove(m_key);
                m_key = other.m_key;
                other.m_key = invalid_key;
            }

            ~PointerBase() noexcept { remove(m_key); }

            T* get() const noexcept { return static_cast<T*>(detail::get(m_key)); }

            T* operator->() const noexcept { return get(); }

            T& operator*() const noexcept { return *get(); }

            bool operator!() const noexcept { return !get(); }

        protected:
            uint32_t m_key;
        };

        template<class T, class Alloc, bool IsSame>
        class PointerAllocBase;

        template<class T, class Alloc>
        class PointerAllocBase<T, Alloc, true> : public PointerBase<T> {
        public:
            explicit PointerAllocBase() : PointerBase<T>({ &cleanup, nullptr }) {}

            template<class ...Ts>
            void emplace(Ts &&... args) {
                if (const auto p = PointerBase<T>::get(); p) if (m_cleanup) m_cleanup(p);
                Alloc alloc{};
                detail::set(PointerBase<T>::m_key, allocator_construct(alloc, std::forward<Ts>(args)...));
            }

            void clear() noexcept {
                if (const auto p = PointerBase<T>::get(); p) {
                    detail::set(PointerBase<T>::m_key, nullptr);
                    if (m_cleanup) m_cleanup(p);
                }
            }

        private:
            const Cleanup m_cleanup{ &cleanup, nullptr };

            static void cleanup(void* p, void* u) noexcept {
                Alloc alloc{};
                if (p) allocator_destruct(alloc, static_cast<T*>(p));
            }
        };

        template<class T, class Alloc>
        class PointerAllocBase<T, Alloc, false> : public PointerBase<T> {
        public:
            explicit PointerAllocBase() : PointerBase<T>(get_cleanup()) {}

            template<class ...Ts>
            void emplace(Ts &&... args) {
                if (const auto p = PointerBase<T>::get(); p) if (m_cleanup) m_cleanup(p);
                detail::set(PointerBase<T>::m_key, essential::allocator_new(m_alloc, std::forward<Ts>(args)...));
            }

            void clear() noexcept {
                if (const auto p = PointerBase<T>::get(); p) {
                    detail::set(PointerBase<T>::m_key, nullptr);
                    if (m_cleanup) m_cleanup(p);
                }
            }

            PointerAllocBase(PointerAllocBase&&) = delete;

            PointerAllocBase& operator=(PointerAllocBase&&) = delete;

        private:
            Alloc m_alloc;
            const Cleanup m_cleanup = get_cleanup();

            [[nodiscard]] Cleanup get_cleanup() const noexcept { return { &cleanup, this }; }

            static void cleanup(void* p, void* u) noexcept {
                if (auto base = reinterpret_cast<PointerAllocBase*>(u); p) 
                    essential::allocator_delete(base->m_alloc, static_cast<T*>(p));
            }
        };
    }

    template<class T, class Alloc = std::allocator<T>>
    class Pointer : public tss::detail::PointerAllocBase<
        T, Alloc, std::is_same_v<typename std::allocator_traits<Alloc>::is_always_equal, std::true_type>
    > {
    };

    template<class T>
    class Pointer<T, void> : public tss::detail::PointerBase<T> {
    public:
        explicit Pointer() : tss::detail::PointerBase<T>({ nullptr, nullptr }), m_cleanup{ nullptr, nullptr } {}

        explicit Pointer(void (*cleanup)(void*, void*) noexcept, void* user) :
            tss::detail::PointerBase<T>({ cleanup, user }), m_cleanup{ cleanup, user } {}

        void reset(T* new_ptr = nullptr) noexcept {
            const auto old_ptr = tss::detail::PointerBase<T>::get();
            if (new_ptr != old_ptr) {
                if (m_cleanup) m_cleanup(old_ptr);
                tss::detail::set(tss::detail::PointerBase<T>::m_key, new_ptr);
            }
        }

        T* release() noexcept {
            const auto p = tss::detail::PointerBase<T>::get();
            if (p) tss::detail::set(tss::detail::PointerBase<T>::m_key, nullptr);
            return p;
        }

    private:
        const tss::detail::Cleanup m_cleanup;
    };

    template<typename T>
    inline T* get_pointer(Pointer<T> const& ptr) noexcept { return ptr.get(); }
}
