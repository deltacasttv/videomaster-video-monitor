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

        // Top-field lines come first, then bottom-field lines; the viewer expects them interleaved.
        void interleave_fields(uint8_t* destination, const BYTE* source, uint64_t size, int height)
        {
            const auto  line_size = size / static_cast<uint64_t>(height);
            const auto  field_lines = static_cast<uint64_t>(height) / 2;
            const auto* bottom_field = source + field_lines * line_size;
            for (uint64_t line = 0; line < field_lines; ++line)
            {
                memcpy(destination + (2 * line) * line_size, source + line * line_size, line_size);
                memcpy(destination + (2 * line + 1) * line_size, bottom_field + line * line_size,
                       line_size);
            }
        }
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
        m_image_height = image_height;
#if defined(__APPLE__)
        if (!m_monitor.init(m_window_width, m_window_height, m_window_title.c_str(), image_width,
                            image_height, input_format))
        {
            throw Deltacast::VideoMonitor::Exceptions::RendererException(
                "VideoViewer initialization failed");
        }
        m_monitor.start();
        m_monitor_ready = true;
#else
        if (m_monitor_thread.joinable())
        {
            stop();
        }

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
#endif
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
            // Only signal global stop if the window was closed by user.
            // If render_loop ended because stop() was called (normal teardown),
            // don't propagate as application-wide stop request.
            const bool closed_by_user = m_monitor.window_request_close();
            m_monitor.release();

            if (closed_by_user)
            {
                m_should_stop = true;
            }
        }
        catch (const Deltacast::VideoMonitor::Exceptions::VideoMonitorException& e)
        {
            spdlog::error("Renderer exception: {}", e.what());
            m_thread_exception = std::current_exception();
            m_monitor.release();
            m_monitor_ready = true;
            m_should_stop = true;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Renderer unexpected exception: {}", e.what());
            m_thread_exception = std::current_exception();
            m_monitor.release();
            m_monitor_ready = true;
            m_should_stop = true;
        }
    }

    auto WindowedRenderer::stop() -> bool
    {
        m_monitor.stop();
#if defined(__APPLE__)
        if (m_monitor_ready)
        {
            m_monitor.release();
            m_monitor_ready = false;
        }
#else
        if (m_monitor_thread.joinable())
        {
            m_monitor_thread.join();
            m_monitor_ready = false;
        }
#endif
        return true;
    }

#if defined(__APPLE__)
    void WindowedRenderer::render_iteration()
    {
        m_monitor.process_escape_key();
        m_monitor.render_iteration();
        if (m_monitor.window_request_close())
        {
            spdlog::warn("Window has been closed");
            m_should_stop = true;
        }
    }
#endif

    void WindowedRenderer::render_buffer(BYTE* buffer, ULONG buffer_size,
                                         bool fields_are_sequential)
    {
        uint8_t* monitor_data = nullptr;
        uint64_t monitor_data_size = 0;
        if (m_monitor.lock_data(&monitor_data, &monitor_data_size))
        {
            if (buffer != nullptr && monitor_data != nullptr && monitor_data_size == buffer_size)
            {
                const bool can_interleave = m_image_height > 0 && m_image_height % 2 == 0 &&
                                            monitor_data_size % m_image_height == 0;
                if (fields_are_sequential && can_interleave)
                {
                    interleave_fields(monitor_data, buffer, monitor_data_size, m_image_height);
                }
                else
                {
                    memcpy(monitor_data, buffer, monitor_data_size);
                }
            }
            m_monitor.unlock_data();
        }
        else
        {
            // Only set should_stop if the window was actually closed
            if (m_monitor.window_request_close())
            {
                spdlog::warn("Window has been closed");
                m_should_stop = true;
            }
        }
    }
}  // namespace Deltacast::VideoMonitor::Renderer