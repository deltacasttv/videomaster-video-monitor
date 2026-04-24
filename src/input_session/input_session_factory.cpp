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
#include "ip_input_session.hpp"
#include "sdi_input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterHD_Core.h>
#include <cstdint>
#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <optional>

namespace Deltacast::VideoMonitor::Session
{
    auto InputSessionFactory::create_input_session(
        uint32_t device_id, uint32_t stream_id, std::optional<std::filesystem::path> sdp_file_path,
        std::optional<Deltacast::VideoMonitor::Session::IpNetworkConfiguration>
                                                  ip_network_configuration,
        Deltacast::VideoMonitor::SharedResources& shared_resources)
        -> std::unique_ptr<InputSessionBase>
    {
        auto board = Deltacast::Wrapper::Board::open(device_id);
        auto channel_type = board.rx(stream_id).type();

        if (ip_network_configuration.has_value() && channel_type != VHD_CHNTYPE_IP_2110)
        {
            throw Exceptions::ConfigurationException(
                fmt::format("IP network configuration options (--ip-dhcp, --ip-address, "
                            "--ip-subnet, --ip-gateway) are only valid for IP 2110 channels, "
                            "but detected channel type: {}",
                            Deltacast::Wrapper::to_pretty_string(channel_type)));
        }

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
                throw Exceptions::ConfigurationException(
                    "SDP file path must be provided for IP input sessions");
            }
            if (!ip_network_configuration.has_value())
            {
                throw Exceptions::ConfigurationException(
                    "IP network configuration must be provided for IP input sessions");
            }
            return std::make_unique<IpInputSession>(
                IpInputSessionConfig{ device_id, stream_id, sdp_file_path.value(),
                                      ip_network_configuration.value() },
                shared_resources);
        default:
            throw Exceptions::ConfigurationException(
                fmt::format("Unsupported channel type: {}",
                            Deltacast::Wrapper::to_pretty_string(channel_type)));
        }
    }
}  // namespace Deltacast::VideoMonitor::Session