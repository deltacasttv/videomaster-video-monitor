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
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#else
#include <VideoMasterHD_Core.h>
#endif
#include <cstdint>
#include <filesystem>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>

namespace Deltacast::VideoMonitor::Session
{
    auto InputSessionFactory::create_input_session(
        uint32_t device_id, uint32_t stream_id, std::optional<std::filesystem::path> sdp_file_path,
        std::optional<Deltacast::VideoMonitor::Session::IpNetworkConfiguration>
            ip_network_configuration,
        std::optional<Deltacast::VideoMonitor::Session::IpInputMediaConfiguration>
                                                  ip_media_configuration,
        Deltacast::VideoMonitor::SharedResources& shared_resources)
        -> std::unique_ptr<InputSessionBase>
    {
        spdlog::trace("Creating input session for device {} stream {}", device_id, stream_id);
        auto board = Deltacast::Wrapper::Board::open(device_id);
        auto channel_type = board.rx(stream_id).type();
        spdlog::debug("Detected RX{} channel type: {}", stream_id,
                      Deltacast::Wrapper::to_pretty_string(channel_type));

        if (ip_network_configuration.has_value() && channel_type != VHD_CHNTYPE_IP_2110)
        {
            spdlog::warn("IP network options were provided for non-IP channel type {}",
                         Deltacast::Wrapper::to_pretty_string(channel_type));
            throw Exceptions::ConfigurationException(
                fmt::format("IP network configuration options (--ip-dhcp, --ip-address, "
                            "--ip-subnet, --ip-gateway) are only valid for IP 2110 channels, "
                            "but detected channel type: {}",
                            Deltacast::Wrapper::to_pretty_string(channel_type)));
        }

        if (ip_media_configuration.has_value() && channel_type != VHD_CHNTYPE_IP_2110)
        {
            spdlog::warn("IP media options were provided for non-IP channel type {}",
                         Deltacast::Wrapper::to_pretty_string(channel_type));
            throw Exceptions::ConfigurationException(
                fmt::format("IP media configuration options are only valid for IP 2110 channels, "
                            "but detected channel type: {}",
                            Deltacast::Wrapper::to_pretty_string(channel_type)));
        }

        switch (channel_type)
        {
        case VHD_CHNTYPE_HDSDI:
        case VHD_CHNTYPE_3GSDI:
        case VHD_CHNTYPE_12GSDI:
            spdlog::debug("Creating SDI input session for RX{}", stream_id);
            return std::make_unique<SdiInputSession>(SdiInputSessionConfiguration{ device_id,
                                                                                   stream_id },
                                                     shared_resources);
        case VHD_CHNTYPE_DISPLAYPORT:
        case VHD_CHNTYPE_HDMI_TMDS:
        case VHD_CHNTYPE_HDMI_FRL3:
        case VHD_CHNTYPE_HDMI_FRL4:
        case VHD_CHNTYPE_HDMI_FRL5:
        case VHD_CHNTYPE_HDMI_FRL6:
            spdlog::debug("Creating DV input session for RX{}", stream_id);
            return std::make_unique<DvInputSession>(DvInputSessionConfiguration{ device_id,
                                                                                 stream_id },
                                                    shared_resources);
        case VHD_CHNTYPE_IP_2110:
            spdlog::debug("Creating IP 2110 input session for RX{}", stream_id);

            if (sdp_file_path.has_value() && ip_media_configuration.has_value())
            {
                throw Exceptions::ConfigurationException(
                    "Explicit IP media options cannot be combined with --sdp-file");
            }

            if (!sdp_file_path.has_value())
            {
                if (!ip_media_configuration.has_value())
                {
                    spdlog::warn("Missing SDP file path or IP media configuration for IP 2110 "
                                 "input session on RX{}",
                                 stream_id);
                    throw Exceptions::ConfigurationException(
                        "SDP file path must be provided for IP input sessions");
                }

                spdlog::info("Using explicit IP media configuration without SDP on RX{}",
                             stream_id);
            }
            else
            {
                spdlog::trace("Using SDP file '{}' for RX{}", sdp_file_path->string(), stream_id);
            }

            if (!ip_network_configuration.has_value())
            {
                spdlog::warn("Missing IP network configuration for IP 2110 input session on RX{}. "
                             "Pre-configured network settings will be used.",
                             stream_id);
            }

            return std::make_unique<IpInputSession>(IpInputSessionConfig{ device_id, stream_id,
                                                                          sdp_file_path,
                                                                          ip_network_configuration,
                                                                          ip_media_configuration },
                                                    shared_resources);
        default:
            spdlog::warn("Unsupported RX{} channel type: {}", stream_id,
                         Deltacast::Wrapper::to_pretty_string(channel_type));
            throw Exceptions::ConfigurationException(
                fmt::format("Unsupported channel type: {}",
                            Deltacast::Wrapper::to_pretty_string(channel_type)));
        }
    }
}  // namespace Deltacast::VideoMonitor::Session