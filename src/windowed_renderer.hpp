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

#include <videoviewer/videoviewer.hpp>


#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#else
#include <VideoMasterHD_Core.h>
#endif
#include <atomic>
#include <exception>
#include <string>
#include <thread>

namespace Deltacast::VideoMonitor::Renderer
{

    class WindowedRenderer
    {
     public:
        struct Config
        {
            std::string window_title;
            int         window_width;
            int         window_height;
            int         framerate_ms;
        };
        WindowedRenderer(const Config& config, std::atomic_bool& stop_is_requested);
        ~WindowedRenderer();

        WindowedRenderer(const WindowedRenderer&) = delete;
        auto operator=(const WindowedRenderer&) -> WindowedRenderer& = delete;
        WindowedRenderer(WindowedRenderer&&) = delete;
        auto operator=(WindowedRenderer&&) -> WindowedRenderer& = delete;

        auto init(int image_width, int image_height,
                  Deltacast::VideoViewer::InputFormat input_format) -> bool;
        void render_buffer(BYTE* buffer, ULONG buffer_size);
        auto stop() -> bool;

        auto get_thread_exception() const -> std::exception_ptr { return m_thread_exception; }

     private:
        std::string m_window_title;
        int         m_window_width;
        int         m_window_height;
        int         m_framerate_ms;

        Deltacast::VideoViewer m_monitor;
        std::thread            m_monitor_thread;

        std::atomic_bool&  m_should_stop;
        std::atomic_bool   m_monitor_ready;
        std::exception_ptr m_thread_exception;

        auto monitor(int image_width, int image_height,
                     Deltacast::VideoViewer::InputFormat input_format) -> bool;
    };
}  // namespace Deltacast::VideoMonitor::Renderer