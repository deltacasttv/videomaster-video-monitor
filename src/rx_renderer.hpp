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

#include "VideoMasterHD_Core.h"

#include <iostream>
#include <thread>
#include <atomic>

class RxRenderer
{
public:
    RxRenderer(std::string window_title, int window_width, int window_height, int framerate_ms, std::atomic_bool& stop_is_requested);
    ~RxRenderer();
    
    RxRenderer(const RxRenderer&) = delete;
    RxRenderer& operator=(const RxRenderer&) = delete;
    RxRenderer(RxRenderer&&) = delete;
    RxRenderer& operator=(RxRenderer&&) = delete;

    bool init(int image_width, int image_height, Deltacast::VideoViewer::InputFormat input_format);
    void render_buffer(BYTE* buffer, ULONG buffer_size);
    bool stop();

private:
    std::string _window_title;
    int _window_width;
    int _window_height;
    int _framerate_ms;

    Deltacast::VideoViewer _monitor;
    std::thread _monitor_thread;

    std::atomic_bool& _should_stop;

    bool monitor(int image_width, int image_height, Deltacast::VideoViewer::InputFormat input_format);
};