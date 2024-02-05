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

#include "device.hpp"
#include "shared_resources.hpp"
#include "signal_information.hpp"

#include "api_helper/api_success.hpp"

namespace Deltacast
{
    class RxStream
    {
    public:

        RxStream(Device& device, std::string name, int channel_index);
        ~RxStream();

        bool configure(SignalInformation signal_info);
        bool start();
        bool stop();

        bool lock_slot();
        bool unlock_slot();
        bool get_buffer(BYTE*& buffer, ULONG& buffer_size);
        Helper::StreamHandle& handle() { return *_stream_handle; }

    private:
        Device& _device;
        int _channel_index;
        std::string _name;
        std::unique_ptr<Helper::StreamHandle> _stream_handle;
        bool stream_started = false;

        HANDLE _slot = nullptr;
        BYTE *buffer = nullptr;
        ULONG buffer_size = 0;
    };
}