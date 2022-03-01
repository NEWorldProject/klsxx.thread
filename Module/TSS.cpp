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

#include <stack>
#include <mutex>
#include "kls/temp/STL.h"
#include <vector>
#include "kls/thread/TSS.h"
#include "kls/thread/SpinLock.h"

namespace kls::thread::tss::detail {
    class Host {
    public:
        static auto& get() {
            static Host instance{};
            return instance;
        }

        class Context {
        public:
            Context() noexcept { Host::get().register_context(this); }

            ~Context() {
                auto& host = Host::get();
                while (!m_storage.empty()) {
                    std::vector<void*> storage;
                    storage.swap(m_storage);
                    for (uint32_t key = 0, n = static_cast<uint32_t>(storage.size()); key < n; ++key) {
                        if (const auto value = storage[key]; value) {
                            if (const auto cleanup = host.m_cleanups[key]; cleanup) cleanup(value);
                        }
                    }
                }
                host.unregister_context(this);
            }

            [[nodiscard]] void* get_value(uint32_t key) const noexcept {
                if (key < m_storage.size()) return m_storage[key]; else return nullptr;
            }

            void set_value(uint32_t key, void* value) {
                if (key >= m_storage.size())
                    m_storage.resize(key + 1, nullptr);
                m_storage[key] = value;
            }

        private:
            friend class Host;

            Context* m_prev{ nullptr }, * m_next{ nullptr };
            std::vector<void*> m_storage;
        };

        uint32_t new_key(Cleanup cleanup) {
            std::lock_guard lock(m_mutex);
            // See if we can recycle some key
            uint32_t key;
            if (!m_freed_keys.empty()) {
                key = m_freed_keys.top();
                m_freed_keys.pop();
                m_cleanups[key] = cleanup;
            }
            else {
                key = static_cast<uint32_t>(m_cleanups.size());
                m_cleanups.push_back(cleanup);
            }
            return key;
        }

        void delete_key(uint32_t key) {
            std::unique_lock lock(m_mutex);
            kls::temp::vector<void*> storage;
            const auto cleanup = m_cleanups[key];
            m_cleanups[key] = { nullptr, nullptr };
            for (auto it = m_head; it; it = it->m_next) {
                if (it->m_storage.size() > key && it->m_storage[key] != nullptr) {
                    if (cleanup) storage.push_back(it->m_storage[key]);
                    it->m_storage[key] = nullptr;
                }
            }
            m_freed_keys.push(key);
            lock.unlock();

            // Run cleanup routines while the lock is released
            for (auto& it : storage) cleanup(it);
        }

    private:
        SpinLock m_mutex;
        Context* m_head{ nullptr }, * m_tail{ nullptr };
        std::vector<Cleanup> m_cleanups;
        std::stack<uint32_t> m_freed_keys;

        Host() = default;

        ~Host() = default;

        void register_context(Context* p) noexcept {
            std::lock_guard lock(m_mutex);
            if (!m_head) m_head = p; else (p->m_prev = m_tail, m_tail->m_next = p);
            m_tail = p;
        }

        void unregister_context(Context* p) noexcept {
            std::lock_guard lock(m_mutex);
            if (p->m_next) p->m_next->m_prev = p->m_prev; else m_tail = p->m_prev;
            if (p->m_prev) p->m_next->m_prev = p->m_prev; else m_head = p->m_next;
        }
    };

    static Host::Context& context() noexcept {
        static thread_local Host::Context context{};
        return context;
    }

    uint32_t create(Cleanup callback) { return Host::get().new_key(callback); }

    void remove(uint32_t key) noexcept {
        if (key == invalid_key) return;
        Host::get().delete_key(key);
    }

    void* get(uint32_t key) noexcept { return context().get_value(key); }

    void set(uint32_t key, void* p) { context().set_value(key, p); }
}
