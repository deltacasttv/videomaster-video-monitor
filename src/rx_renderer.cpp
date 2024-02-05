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

#include "rx_renderer.hpp"

#include <iostream>
#include <cstring>

RxRenderer::RxRenderer(std::string window_title, int window_width, int window_height, int framerate_ms, std::atomic_bool& stop_is_requested)
    : _window_title(window_title)
    , _window_width(window_width)
    , _window_height(window_height)
    , _framerate_ms(framerate_ms)
    , _should_stop(stop_is_requested)
{
}

RxRenderer::~RxRenderer()
{
    stop();
}

bool RxRenderer::init(int image_width, int image_height, Deltacast::VideoViewer::InputFormat input_format)
{
    _monitor_thread = std::thread(&RxRenderer::monitor, this, image_width, image_height, input_format);

    return true;
}

bool RxRenderer::monitor(int image_width, int image_height, Deltacast::VideoViewer::InputFormat input_format)
{
    if (!_monitor.init(_window_width, _window_height, _window_title.c_str(), image_width, image_height, input_format))
    {
        std::cout << "ERROR: VideoViewer initialization failed" << std::endl;
        return false;
    }

    _monitor.render_loop(_framerate_ms);
    _monitor.release();

    return true;
}

bool RxRenderer::stop()
{
    _monitor.stop();
    if (_monitor_thread.joinable())
        _monitor_thread.join();

    return true;
}

void RxRenderer::render_buffer(BYTE* buffer, ULONG buffer_size)
{
    uint8_t* monitor_data = nullptr;
    uint64_t monitor_data_size = 0;
    if (_monitor.lock_data(&monitor_data, &monitor_data_size)) 
    {
        if (buffer && monitor_data && monitor_data_size == buffer_size)
            memcpy(monitor_data, buffer, monitor_data_size);
        _monitor.unlock_data();
    }
    else // windows has probaly been closed
    {
        _should_stop = true; 
    }
}