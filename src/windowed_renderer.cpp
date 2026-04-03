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

#include <cstring>
#include <iostream>

#include <spdlog/spdlog.h>

WindowedRenderer::WindowedRenderer(std::string window_title, int window_width, int window_height,
                                   int framerate_ms, std::atomic_bool& stop_is_requested)
    : m_window_title(window_title), m_window_width(window_width), m_window_height(window_height),
      m_framerate_ms(framerate_ms), m_should_stop(stop_is_requested), m_monitor_ready(false),
      m_thread_exception(nullptr)
{
}

WindowedRenderer::~WindowedRenderer()
{
    stop();
}

bool WindowedRenderer::init(int image_width, int image_height,
                            Deltacast::VideoViewer::InputFormat input_format)
{
    m_monitor_thread = std::thread(&WindowedRenderer::monitor, this, image_width, image_height,
                                   input_format);
    while (!m_monitor_ready)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;
}

bool WindowedRenderer::monitor(int image_width, int image_height,
                               Deltacast::VideoViewer::InputFormat input_format)
{
    try
    {
        if (!m_monitor.init(m_window_width, m_window_height, m_window_title.c_str(), image_width,
                            image_height, input_format))
        {
            throw Deltacast::VideoMonitor::RendererInitializationException(
                "VideoViewer initialization failed");
        }

        m_monitor_ready = true;
        m_monitor.render_loop(m_framerate_ms);
        m_monitor.release();

        return true;
    }
    catch (const Deltacast::VideoMonitor::ApplicationException& e)
    {
        spdlog::error("Renderer exception: {}", e.what());
        m_thread_exception = std::current_exception();
        m_should_stop = true;
        return false;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Renderer unexpected exception: {}", e.what());
        m_thread_exception = std::current_exception();
        m_should_stop = true;
        return false;
    }
}

bool WindowedRenderer::stop()
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
        if (buffer && monitor_data && monitor_data_size == buffer_size)
            memcpy(monitor_data, buffer, monitor_data_size);
        m_monitor.unlock_data();
    }
    else  // windows has probaly been closed
    {
        spdlog::warn("Window has been closed");
        m_should_stop = true;
    }
}