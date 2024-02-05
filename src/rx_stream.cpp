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

#include "rx_stream.hpp"

#include "VideoMasterAPIHelper/handle_manager.hpp"

#include <thread>
#include <functional>
#include <unordered_map>

const std::unordered_map<uint32_t, VHD_STREAMTYPE> id_to_stream_type = {
    {0, VHD_ST_RX0},
    {1, VHD_ST_RX1},
    {2, VHD_ST_RX2},
    {3, VHD_ST_RX3},
    {4, VHD_ST_RX4},
    {5, VHD_ST_RX5},
    {6, VHD_ST_RX6},
    {7, VHD_ST_RX7},
    {8, VHD_ST_RX8},
    {9, VHD_ST_RX9},
    {10, VHD_ST_RX10},
    {11, VHD_ST_RX11},
};

Deltacast::RxStream::RxStream(Device& device, std::string name, int channel_index)
    : _device(device)
    , _name(name)
    , _channel_index(channel_index)
{
    if (id_to_stream_type.find(_channel_index) == id_to_stream_type.end())
        throw std::runtime_error("Cannot find stream type for channel index " + std::to_string(channel_index)); 
    
    auto stream_handle = get_stream_handle(device.handle(), id_to_stream_type.at(channel_index), VHD_SDI_STPROC_DISJOINED_VIDEO);

    if (!stream_handle)
        throw std::runtime_error("Couldn't create a stream handle for channel index " + std::to_string(channel_index)); 
    _stream_handle = std::move(stream_handle);
}

bool Deltacast::RxStream::configure(SignalInformation signal_info)
{
    Deltacast::Helper::ApiSuccess api_success;
    if (!(api_success = VHD_SetStreamProperty(*handle(), VHD_CORE_SP_TRANSFER_SCHEME, VHD_TRANSFER_SLAVED))
        || !(api_success = VHD_SetStreamProperty(*handle(), VHD_SDI_SP_VIDEO_STANDARD, signal_info.video_standard))
        || !(api_success = VHD_SetStreamProperty(*handle(), VHD_SDI_SP_INTERFACE, signal_info.interface)))
    {
        std::cout << "ERROR for " << _name << ": Cannot configure stream (" << api_success << ")" << std::endl;
        return false;
    }
    
    return true;
}

bool Deltacast::RxStream::start()
{
    if (stream_started)
    {
        std::cout << "ERROR for " << _name << ": Stream already started" << std::endl;
        return false;
    }

    Deltacast::Helper::ApiSuccess api_success{VHD_StartStream(*handle())};
    if (!api_success)
    {
        std::cout << "ERROR for " << _name << ": Cannot start stream (" << api_success << ")" << std::endl;
        return false;
    }

    stream_started = true;
    return true;
}

bool Deltacast::RxStream::stop() {
    if (!stream_started)
    {
        std::cout << "ERROR for " << _name << ": Stream already stopped" << std::endl;
        return false;
    }

    Deltacast::Helper::ApiSuccess api_success{VHD_StopStream(*handle())};
    if (!api_success)
    {
        std::cout << "ERROR for " << _name << ": Cannot stop stream (" << api_success << ")" << std::endl;
        return false;
    } 
    stream_started = false;
    return true;
}

bool Deltacast::RxStream::lock_slot()
{
    if(_slot != nullptr)
    {
        std::cout << "ERROR for " << _name << ": Slot already locked" << std::endl;
        return false;
    }

    Deltacast::Helper::ApiSuccess api_success{VHD_LockSlotHandle(*handle(), &_slot)};
    if (!api_success)
    {
        std::cout << "ERROR for " << _name << ": Cannot lock slot (" << api_success << ")" << std::endl;
        return false;
    }
    return true;
}

bool Deltacast::RxStream::unlock_slot()
{
    if(_slot == nullptr)
    {
        std::cout << "ERROR for " << _name << ": Slot already unlocked" << std::endl;
        return false;
    }

    Deltacast::Helper::ApiSuccess api_success{VHD_UnlockSlotHandle(_slot)};
    if (!api_success)
    {
        std::cout << "ERROR for " << _name << ": Cannot unlock slot (" << api_success << ")" << std::endl;
        return false;
    }
    _slot = nullptr;
    return true;
}

bool Deltacast::RxStream::get_buffer(UBYTE*& buffer, ULONG& buffer_size)
{
    if(_slot == nullptr)
    {
        std::cout << "ERROR for " << _name << ": Slot not locked" << std::endl;
        return false;
    }

    Deltacast::Helper::ApiSuccess api_success{VHD_GetSlotBuffer(_slot, VHD_SDI_BT_VIDEO, &buffer, &buffer_size)};
    if (!api_success)
    {
        std::cout << "ERROR for " << _name << ": Cannot get slot buffer (" << api_success << ")" << std::endl;
        return false;
    } 
    return true;
}

Deltacast::RxStream::~RxStream()
{
    if (_slot != nullptr)
        VHD_UnlockSlotHandle(_slot);
    if (stream_started)
        VHD_StopStream(*handle());
}
