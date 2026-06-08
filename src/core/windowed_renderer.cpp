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

#include "windowed_renderer.hpp"
#include "exceptions.hpp"

#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#else
#include <VideoMasterHD_Core.h>
#endif
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <videoviewer/videoviewer.hpp>

namespace Deltacast::VideoMonitor::Renderer
{
    namespace
    {
        constexpr int monitor_wait_timeout_ms = 100;
    }  // namespace
    WindowedRenderer::WindowedRenderer(const Config& config, std::atomic_bool& stop_is_requested)
        : m_window_title(config.window_title), m_window_width(config.window_width),
          m_window_height(config.window_height), m_framerate_ms(config.framerate_ms),
          m_should_stop(stop_is_requested), m_monitor_ready(false), m_thread_exception(nullptr)
    {
    }

    WindowedRenderer::~WindowedRenderer()
    {
        stop();
    }

    void WindowedRenderer::init(int image_width, int image_height,
                                Deltacast::VideoViewer::InputFormat input_format)
    {
        m_monitor_ready = false;
        m_thread_exception = nullptr;
        m_monitor_thread = std::thread(&WindowedRenderer::monitor, this, image_width, image_height,
                                       input_format);
        while (!m_monitor_ready)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(monitor_wait_timeout_ms));
        }

        if (m_thread_exception)
        {
            if (m_monitor_thread.joinable())
            {
                m_monitor_thread.join();
            }
            std::rethrow_exception(m_thread_exception);
        }

    }

    void WindowedRenderer::monitor(int image_width, int image_height,
                                   Deltacast::VideoViewer::InputFormat input_format)
    {
        try
        {
            if (!m_monitor.init(m_window_width, m_window_height, m_window_title.c_str(),
                                image_width, image_height, input_format))
            {
                throw Deltacast::VideoMonitor::Exceptions::RendererException(
                    "VideoViewer initialization failed");
            }

            m_monitor_ready = true;
            m_monitor.render_loop(m_framerate_ms);
            m_monitor.release();
            m_should_stop = true;
        }
        catch (const Deltacast::VideoMonitor::Exceptions::VideoMonitorException& e)
        {
            spdlog::error("Renderer exception: {}", e.what());
            m_thread_exception = std::current_exception();
            m_monitor_ready = true;
            m_should_stop = true;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Renderer unexpected exception: {}", e.what());
            m_thread_exception = std::current_exception();
            m_monitor_ready = true;
            m_should_stop = true;
        }
    }

    auto WindowedRenderer::stop() -> bool
    {
        m_monitor.stop();
        if (m_monitor_thread.joinable())
        {
            m_monitor_thread.join();
            m_monitor_ready = false;
        }

        return true;
    }

    void WindowedRenderer::render_buffer(BYTE* buffer, ULONG buffer_size)
    {
        uint8_t* monitor_data = nullptr;
        uint64_t monitor_data_size = 0;
        if (m_monitor.lock_data(&monitor_data, &monitor_data_size))
        {
            if (buffer != nullptr && monitor_data != nullptr && monitor_data_size == buffer_size)
            {
                memcpy(monitor_data, buffer, monitor_data_size);
            }
            m_monitor.unlock_data();
        }
        else
        {
            spdlog::warn("Window has been closed");
            m_should_stop = true;
        }
    }
}  // namespace Deltacast::VideoMonitor::Renderer