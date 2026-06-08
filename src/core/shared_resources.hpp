/*
 * SPDX-FileCopyrightText: Copyright (c) DELTACAST.TV. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at * * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <exception>
#include <mutex>

namespace Deltacast::VideoMonitor
{
    struct SharedResources
    {
        std::atomic_bool stop_is_requested{ false };
        std::atomic_bool incoming_signal_changed{ false };

        // For communicating exceptions from worker threads to main
        mutable std::mutex thread_exception_mutex;
        std::exception_ptr thread_exception;

        void reset();

        void set_thread_exception(std::exception_ptr exc)
        {
            std::lock_guard<std::mutex> lock(thread_exception_mutex);
            thread_exception = exc;
        }

        auto get_thread_exception() const -> std::exception_ptr
        {
            std::lock_guard<std::mutex> lock(thread_exception_mutex);
            return thread_exception;
        }

        void clear_thread_exception()
        {
            std::lock_guard<std::mutex> lock(thread_exception_mutex);
            thread_exception = nullptr;
        }
    };
}  // namespace Deltacast::VideoMonitor