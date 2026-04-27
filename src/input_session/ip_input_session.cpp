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

#include "ip_input_session.hpp"
#include "exceptions.hpp"
#include "input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/board/ip/port/port.hpp>
#include <VideoMasterCppApi/helper/ip.hpp>
#include <VideoMasterCppApi/helper/sdp.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/slot/ip/st2110_slot.hpp>
#include <VideoMasterCppApi/stream/ip/st2110_stream.hpp>
#include <VideoMasterCppApi/stream/ip/video.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Ip_Board.h>
#include <VideoMasterHD_Ip_ST2110_Board.h>
#include <VideoMasterHD_SDP.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <ipaddress/ip-address-base.hpp>
#include <ipaddress/ip-any-address.hpp>
#include <ipaddress/ipaddress.hpp>
#include <ipaddress/ipv4-address.hpp>
#include <iterator>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Deltacast::VideoMonitor::Session
{
    namespace
    {
        constexpr uint32_t dhcp_timeout_seconds = 10;
        constexpr uint32_t dhcp_poll_interval_ms = 100;
        constexpr uint32_t main_port_index = 0;
        constexpr uint32_t sps_port_index = 1;
        constexpr uint32_t ipv6_byte_count = 16;

        auto parse_sdp_ip_address(const VHD_SDP_IP_ADDRESS& ip_address_struct)
            -> ipaddress::ip_address
        {
            if (ip_address_struct.Version == VHD_SDP_IP_VERSION_4)
            {
                return ipaddress::ipv4_address::from_uint(ip_address_struct.AddressV4);
            }

            if (ip_address_struct.Version == VHD_SDP_IP_VERSION_6)
            {
                const auto* bytes = reinterpret_cast<const uint8_t*>(
                    &ip_address_struct.AddressV6[0]);
                return ipaddress::ip_address::from_bytes(bytes, sizeof(ip_address_struct.AddressV6),
                                                         ipaddress::ip_version::V6);
            }

            throw Exceptions::ConfigurationException("Unsupported IP version in SDP");
        }

        auto classify_st2110_20_media(std::vector<VHD_SDP_MEDIA> media)
            -> std::vector<std::pair<VHD_SDP_MEDIA, IpInputSession::MediaRole>>
        {
            using MediaRole = IpInputSession::MediaRole;
            std::vector<std::pair<VHD_SDP_MEDIA, MediaRole>> result;
            std::map<BYTE, uint32_t>                         group_occurrence;

            for (auto& m : media)
            {
                if (m.MediaType != VHD_SDP_MEDIA_TYPE_ST2110_20)
                    continue;

                if (m.GroupId == 0)
                {
                    result.emplace_back(m, MediaRole::Main);
                }
                else
                {
                    auto& count = group_occurrence[m.GroupId];
                    ++count;
                    if (count == 1)
                        result.emplace_back(m, MediaRole::Main);
                    else if (count == 2)
                        result.emplace_back(m, MediaRole::Sps);
                    else
                        result.emplace_back(m, MediaRole::Ignored);
                }
            }
            return result;
        }

        auto to_ipv6_bytes(const ipaddress::ip_address& addr)
            -> std::array<uint8_t, ipv6_byte_count>
        {
            const auto&                          v6_bytes = addr.v6().value().bytes();
            std::array<uint8_t, ipv6_byte_count> result{};
            std::memcpy(result.data(), v6_bytes.data(), result.size());
            return result;
        }

        auto network_mode_to_string(IpNetworkMode mode) -> const char*
        {
            return mode == IpNetworkMode::Dhcp ? "DHCP" : "static";
        }

        auto wait_for_dhcp_ip(Deltacast::Wrapper::BoardComponents::IpComponents::Port& port) -> void
        {
            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(dhcp_timeout_seconds);
            spdlog::trace("Waiting for DHCP lease assignment...");
            while (!static_cast<bool>(port.dhcp().status().IPValid))
            {
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    spdlog::warn("DHCP lease assignment timed out after {} seconds",
                                 dhcp_timeout_seconds);
                    throw Exceptions::NetworkException(
                        "Timed out waiting for DHCP to assign an IP address");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(dhcp_poll_interval_ms));
            }

            spdlog::info("DHCP lease acquired");
            spdlog::debug("DHCP assigned IP address: {}",
                          ipaddress::ipv4_address::from_uint(port.ip_address()).to_string());
            spdlog::debug("DHCP assigned subnet mask: {}",
                          ipaddress::ipv4_address::from_uint(port.subnet_mask()).to_string());
            spdlog::debug("DHCP assigned gateway: {}",
                          ipaddress::ipv4_address::from_uint(port.gateway()).to_string());
        };

        auto configure_ip_port(Deltacast::Wrapper::Board& board, uint32_t port_index,
                               IpNetworkMode mode, const ipaddress::ipv4_address& ip_address_v4,
                               const ipaddress::ipv4_address& subnet_mask_v4,
                               const ipaddress::ipv4_address& gateway_v4,
                               const char*                    unsupported_dhcp_message) -> void
        {
            auto& port = board.ip().port(port_index);
            spdlog::trace("Configuring IP port {} in {} mode", port_index,
                          network_mode_to_string(mode));

            if (mode == IpNetworkMode::Dhcp)
            {
                if (!port.has_dhcp())
                {
                    spdlog::warn("DHCP requested on IP port {} but not supported", port_index);
                    throw Exceptions::NetworkException(unsupported_dhcp_message);
                }

                port.dhcp().enable();
                wait_for_dhcp_ip(port);
                spdlog::trace("IP port {} DHCP configuration applied", port_index);
                return;
            }

            if (port.has_dhcp())
            {
                port.dhcp().disable();
            }

            port.set_ip_address(ip_address_v4.to_uint());
            port.set_subnet_mask(subnet_mask_v4.to_uint());
            port.set_gateway(gateway_v4.to_uint());
            spdlog::trace("IP port {} configured with IP address {}, subnet mask {}, gateway {}",
                          port_index,
                          ipaddress::ipv4_address::from_uint(port.ip_address()).to_string(),
                          ipaddress::ipv4_address::from_uint(port.subnet_mask()).to_string(),
                          ipaddress::ipv4_address::from_uint(port.gateway()).to_string());

            spdlog::debug("Configured IP port {} with address {}, subnet {}, gateway {}",
                          port_index, ip_address_v4.to_string(), subnet_mask_v4.to_string(),
                          gateway_v4.to_string());
        };

    }  // namespace

    IpInputSession::IpInputSession(const IpInputSessionConfig&               config,
                                   Deltacast::VideoMonitor::SharedResources& shared_resources)
        : InputSession<Deltacast::Wrapper::Ip2110Stream>(config, shared_resources),
          m_network_configuration(config.network_configuration)
    {
        spdlog::trace("Loading SDP file '{}' for IP RX{}", config.sdp_file_path.string(),
                      config.stream_id);
        std::string sdp_content;
        {
            std::ifstream sdp_file(config.sdp_file_path);
            if (!sdp_file.is_open())
            {
                throw Exceptions::ConfigurationException(
                    fmt::format("Failed to open SDP file: {}", config.sdp_file_path.string()));
            }
            sdp_content.assign((std::istreambuf_iterator<char>(sdp_file)),
                               std::istreambuf_iterator<char>());
        }

        auto [session, media] = Deltacast::Wrapper::Helper::Ip::read_sdp(sdp_content);
        this->m_session = session;
        spdlog::debug("Parsed SDP session with {} media description(s)", media.size());
        this->m_classified_media = classify_st2110_20_media(std::move(media));
    }

    void IpInputSession::join_multicast_group(const VHD_SDP_IP_ADDRESS& ip_address_struct,
                                              uint32_t                  port_index)
    {
        auto&      port = this->m_board->ip().port(port_index);
        const auto ip_address = parse_sdp_ip_address(ip_address_struct);

        if (!ip_address.is_multicast())
        {
            spdlog::trace("Skipping multicast join for unicast destination {} on port {}",
                          ip_address.to_string(), port_index);
            return;
        }

        if (!port.has_multicast())
        {
            throw Exceptions::NetworkException(fmt::format(
                "Multicast IP address {} in SDP but the port does not support multicast",
                ip_address.to_string()));
        }

        port.multicast().set_version(VHD_IP_BRD_IGMP_VERSION_V3);

        if (ip_address.is_v4())
        {

            port.multicast().join(ip_address.to_uint32());
        }
        else if (ip_address.is_v6())
        {
            port.multicast().join(to_ipv6_bytes(ip_address));
        }

        m_multicast_groups.emplace_back(port_index, ip_address);
        spdlog::trace("Joined multicast group {} on port {}", ip_address.to_string(), port_index);
    }

    void IpInputSession::open_board()
    {
        spdlog::trace("Opening IP board {} for RX{}", this->device_id(), this->stream_id());
        m_multicast_groups.clear();

        this->m_board = std::make_unique<Deltacast::Wrapper::Board>(Deltacast::Wrapper::Board::open(
            this->device_id(),
            [&](auto& board)
            {
                for (const auto& [port_index, group] : m_multicast_groups)
                {
                    auto& port = board.ip().port(port_index);
                    if (group.is_v6())
                    {
                        port.multicast().leave(to_ipv6_bytes(group));
                    }
                    else if (group.is_v4())
                    {
                        port.multicast().leave(group.to_uint32());
                    }
                }
            }));

        spdlog::debug("Configuring Main port {}", main_port_index);
        configure_ip_port(this->board(), main_port_index, m_network_configuration.mode,
                          m_network_configuration.ip_address_v4,
                          m_network_configuration.subnet_mask_v4,
                          m_network_configuration.gateway_v4,
                          "DHCP mode requested but DHCP is not supported on this IP port");

        if (m_network_configuration.has_sps)
        {
            spdlog::debug("Configuring SPS port {}", sps_port_index);
            configure_ip_port(
                this->board(), sps_port_index, m_network_configuration.sps_mode,
                m_network_configuration.sps_ip_address_v4,
                m_network_configuration.sps_subnet_mask_v4, m_network_configuration.sps_gateway_v4,
                "DHCP mode requested for SPS but DHCP is not supported on SPS IP port");
        }

        for (const auto& [media_description, role] : m_classified_media)
        {
            spdlog::trace("Preparing multicast subscription for SDP media group {}, MID '{}'",
                          media_description.GroupId, media_description.MID);

            if (role == MediaRole::Ignored)
            {
                spdlog::warn("SDP contains more than 2 media descriptions in group {}. "
                             "Ignoring extra media.",
                             media_description.GroupId);
            }
            else if (role == MediaRole::Sps)
            {
                if (m_network_configuration.has_sps)
                {
                    join_multicast_group(media_description.DestinationIP, sps_port_index);
                }
                else
                {
                    spdlog::warn("SDP contains a redundant (SPS) media stream in group {} but "
                                 "SPS is not enabled in network configuration. Ignoring.",
                                 media_description.GroupId);
                }
            }
            else if (role == MediaRole::Main)
            {
                join_multicast_group(media_description.DestinationIP, main_port_index);
            }
        }
    }

    void IpInputSession::prepare_video_stream()
    {
        auto& board = this->board();
        auto  stream_id = this->stream_id();

        spdlog::trace("Opening ST2110-20 essence stream for RX{}", stream_id);

        m_stream = std::make_unique<Deltacast::Wrapper::Ip2110Stream>(
            board.ip().ip2110().open_essence_stream(VHD_ET_ST2110_20, VHD_RX_CHANNEL, stream_id));

        const auto set_destination_address =
            [](auto& stream, const ipaddress::ip_address& ip_address)
        {
            if (ip_address.is_v4())
            {
                stream.set_destination_ip_address(ip_address.to_uint32());
            }
            else if (ip_address.is_v6())
            {
                stream.set_destination_ipv6_address(to_ipv6_bytes(ip_address));
            }
        };

        const auto configure_multicast_filtering =
            [](auto& stream, auto& port, const VHD_SDP_MEDIA& media_description)
        {
            stream.set_filtering_mask(VHD_IP_FILTER_RTP_PAYLOAD_TYPE | VHD_IP_FILTER_UDP_PORT_DEST |
                                      VHD_IP_FILTER_IP_ADDR_DEST);

            if (!static_cast<bool>(media_description.SourceFilter.UseSourceFilter))
            {
                spdlog::trace(
                    "Configuring multicast filtering for destination {} with no source filtering",
                    ipaddress::ipv4_address::from_uint(
                        media_description.SourceFilter.DestinationIP.AddressV4)
                        .to_string());
                return;
            }

            if (media_description.SourceFilter.DestinationIP.Version != VHD_SDP_IP_VERSION_4)
            {
                throw Exceptions::ConfigurationException(
                    "Source filtering is only supported for IPv4 addresses");
            }

            spdlog::trace(
                "Configuring multicast filtering for destination {}: filter mode {}, source "
                "IPs {}",
                ipaddress::ipv4_address::from_uint(
                    media_description.SourceFilter.DestinationIP.AddressV4)
                    .to_string(),
                media_description.SourceFilter.FilterMode == VHD_SDP_FILTER_MODE_INCL ? "INCLUDE"
                                                                                      : "EXCLUDE",
                [&]
                {
                    std::string result;
                    for (ULONG i = 0; i < media_description.SourceFilter.SourceIPCount; ++i)
                    {
                        if (i > 0)
                            result += ", ";
                        result += parse_sdp_ip_address(
                                      media_description.SourceFilter.SourceIPArray[i])
                                      .to_string();
                    }
                    return result;
                }());

            port.multicast().set_source_mode(media_description.SourceFilter.DestinationIP.AddressV4,
                                             media_description.SourceFilter.FilterMode ==
                                                     VHD_SDP_FILTER_MODE_INCL
                                                 ? VHD_IP_BRD_FILTERMODEMULTICAST_INCLUDE
                                                 : VHD_IP_BRD_FILTERMODEMULTICAST_EXCLUDE);

            for (ULONG i = 0; i < media_description.SourceFilter.SourceIPCount; ++i)
            {
                const auto source_ip_address = ipaddress::ipv4_address::from_uint(
                    media_description.SourceFilter.SourceIPArray[i].AddressV4);
                port.multicast().add_source(media_description.SourceFilter.DestinationIP.AddressV4,
                                            source_ip_address.to_uint());
            }
        };

        // Apply common destination configuration shared by main and SPS streams.
        const auto configure_destination = [&](auto& stream, auto& port, const auto& session,
                                               const auto& media_description,
                                               const auto& ip_address)
        {
            set_destination_address(stream, ip_address);

            if (ip_address.is_multicast() && port.has_multicast())
            {
                configure_multicast_filtering(stream, port, media_description);
            }
            else if (ip_address.is_multicast())
            {
                throw Exceptions::ConfigurationException(
                    fmt::format("IP address {} in SDP is multicast but port does not support "
                                "multicast reception",
                                ip_address.to_string()));
            }
            else
            {
                auto source_ip_address = parse_sdp_ip_address(session.SourceIP);
                if (source_ip_address.is_v4() && ip_address.is_v4())
                {
                    stream.set_source_ip_address(source_ip_address.to_uint32());
                }
                else if (source_ip_address.is_v6() && ip_address.is_v6())
                {
                    stream.set_source_ipv6_address(to_ipv6_bytes(source_ip_address));
                }
                else
                {
                    throw Exceptions::ConfigurationException(
                        fmt::format("Source IP address version does not match destination IP "
                                    "address version in SDP"));
                }
            }

            stream.set_destination_port(media_description.UdpPort);
            stream.set_rtp_payload_type(media_description.PayloadType);
        };

        for (const auto& [media_description, role] : m_classified_media)
        {
            const auto ip_address = parse_sdp_ip_address(media_description.DestinationIP);

            if (role == MediaRole::Ignored)
            {
                spdlog::warn("SDP contains more than 2 media descriptions in group {}. "
                             "Ignoring extra media.",
                             media_description.GroupId);
            }
            else if (role == MediaRole::Sps)
            {
                if (m_network_configuration.has_sps)
                {
                    spdlog::info("Configuring SPS ST2110 media (group {}): destination {}, "
                                 "UDP {}, payload {}",
                                 media_description.GroupId, ip_address.to_string(),
                                 media_description.UdpPort, media_description.PayloadType);
                    configure_destination(m_stream->sps_stream(), board.ip().port(sps_port_index),
                                          m_session, media_description, ip_address);
                }
                else
                {
                    spdlog::warn("SDP contains a redundant (SPS) media stream in group {} but "
                                 "SPS is not enabled in network configuration. Ignoring.",
                                 media_description.GroupId);
                }
            }
            else if (role == MediaRole::Main)
            {
                spdlog::info(
                    "Configuring main ST2110 media (group {}, MID '{}'): destination {}, UDP {}, "
                    "payload {}",
                    media_description.GroupId, media_description.MID, ip_address.to_string(),
                    media_description.UdpPort, media_description.PayloadType);
                configure_destination(m_stream->main_stream(), board.ip().port(main_port_index),
                                      m_session, media_description, ip_address);

                m_stream->video().set_video_standard(media_description.ST2110_20.VideoStandard);
                m_stream->video().set_sampling_rate(media_description.ST2110_20.Sampling);
                m_stream->video().set_bit_depth(media_description.ST2110_20.Depth);

                m_video_characteristics =
                    Deltacast::Wrapper::Helper::Ip::video_standard_to_characteristics(
                        media_description.ST2110_20.VideoStandard);

                spdlog::info("Detected ST2110 video standard {} ({}x{}, interlaced={})",
                             Deltacast::Wrapper::to_pretty_string(
                                 media_description.ST2110_20.VideoStandard),
                             m_video_characteristics.width, m_video_characteristics.height,
                             static_cast<bool>(m_video_characteristics.interlaced));
            }
        }
    }

    void IpInputSession::configure_video_stream()
    {
        auto& stream = this->stream();

        stream.buffer_queue().set_depth(buffer_queue_size);
        stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);
        spdlog::trace("Configured ST2110 buffer queue depth={} packing=YUV422_8",
                      buffer_queue_size);
    }

    auto IpInputSession::has_video_input_changed() -> bool
    {
        return false;
    }

    auto IpInputSession::video_input_has_changed() -> bool
    {
        return false;
    }

    auto IpInputSession::get_video_buffer() -> std::pair<UBYTE*, ULONG>
    {

        this->ensure_board_is_opened();
        auto  current_slot = this->stream().pop_slot();
        auto& slot = static_cast<Deltacast::Wrapper::Ip2110Slot&>(*current_slot);
        return slot.video_essence().buffer();
    }

    auto IpInputSession::get_video_characteristics()
        -> Deltacast::Wrapper::Helper::VideoCharacteristics
    {
        return { m_video_characteristics.width, m_video_characteristics.height,
                 m_video_characteristics.interlaced, m_video_characteristics.framerate };
    }

}  // namespace Deltacast::VideoMonitor::Session