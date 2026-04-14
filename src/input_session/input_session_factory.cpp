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

#include "input_session_factory.hpp"
#include "dv_input_session.hpp"
#include "exceptions.hpp"
#include "input_session_base.hpp"
#include "sdi_input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterHD_Core.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace Deltacast::VideoMonitor::Session
{
    auto InputSessionFactory::create_input_session(
        uint32_t device_id, uint32_t stream_id, std::optional<std::filesystem::path> sdp_file_path,
        Deltacast::VideoMonitor::SharedResources& shared_resources)
        -> std::unique_ptr<InputSessionBase>
    {
        auto board = Deltacast::Wrapper::Board::open(device_id);
        auto channel_type = board.rx(stream_id).type();
        switch (channel_type)
        {
        case VHD_CHNTYPE_HDSDI:
        case VHD_CHNTYPE_3GSDI:
        case VHD_CHNTYPE_12GSDI:
            return std::make_unique<SdiInputSession>(SdiInputSessionConfiguration{ device_id,
                                                                                   stream_id },
                                                     shared_resources);
        case VHD_CHNTYPE_DISPLAYPORT:
        case VHD_CHNTYPE_HDMI_TMDS:
        case VHD_CHNTYPE_HDMI_FRL3:
        case VHD_CHNTYPE_HDMI_FRL4:
        case VHD_CHNTYPE_HDMI_FRL5:
        case VHD_CHNTYPE_HDMI_FRL6:
            return std::make_unique<DvInputSession>(DvInputSessionConfiguration{ device_id,
                                                                                 stream_id },
                                                    shared_resources);
        case VHD_CHNTYPE_IP_2110:
            if (!sdp_file_path.has_value())
            {
                throw Exceptions::VideoMonitorException(
                    "SDP file path must be provided for IP input sessions");
            }
            return nullptr;  // TODO: Implement IP input session
        default:
            throw Exceptions::UnsupportedChannelTypeException(
                Deltacast::Wrapper::to_pretty_string(channel_type));
        }
    }
}  // namespace Deltacast::VideoMonitor::Session