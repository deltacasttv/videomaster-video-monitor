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

#include "signal_information.hpp"

#include "VideoMasterAPIHelper/handle_manager.hpp"

#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Sdi.h"
#include "VideoMasterHD_Sdi_Keyer.h"

#include <iostream>
#include <atomic>

namespace Deltacast
{
    class Device
    {
    private:
        Device() = delete;
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

        Device(int device_index, std::unique_ptr<Helper::BoardHandle> device_handle)
            : _device_index(device_index)
            , _device_handle(std::move(device_handle))
        {
        }

    public:
        ~Device();

        static std::unique_ptr<Device> create(int device_index);

        void enable_loopback(int index);
        void disable_loopback(int index);
        bool wait_for_incoming_signal(int rx_index, const std::atomic_bool& stop_is_requested);
        SignalInformation get_incoming_signal_information(int rx_index);

        int& index() { return _device_index; }
        Helper::BoardHandle& handle() { return *_device_handle; }

        friend std::ostream& operator<<(std::ostream& os, const Device& device);

    private:
        int _device_index;
        std::unique_ptr<Helper::BoardHandle> _device_handle;

        bool set_loopback_state(int index, bool enabled);
    };
}