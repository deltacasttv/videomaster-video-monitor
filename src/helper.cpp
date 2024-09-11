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

#include "helper.hpp"

#include <thread>
#include <utility>
#include <optional>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterCppApi/helper/sdi.hpp>

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using namespace Deltacast::Wrapper;
using namespace Deltacast::Wrapper::Helper;

std::ostream& operator<<(std::ostream& os, Board& board)
{
    os << "Board " << board.index() << ":  [ " << board.name() << " ]" << std::endl;
    os << "\t" << "- " << board.number_of_rx() << " RX / " << board.number_of_tx() << " TX" << std::endl;
    os << "\t" << "- Driver: " << board.driver_version() << std::endl;
    os << "\t" << "- PCIe ID: " << board.pcie_identifier() << std::endl;
    os << "\t" << "\t" << "- SN: " << board.serial_number() << std::endl;
    auto [ pcie_bus, number_of_lanes ] = board.pcie();
    os << "\t" << "\t" << "- " << to_pretty_string(pcie_bus) << ", " << number_of_lanes << " lanes" << std::endl;

    os << std::hex;
    os << "\t" << "- Firmware: 0x" << board.fpga().version() << std::endl;
    os << "\t" << "- SCP: 0x" << board.scp().version() << std::endl;
    os << "\t" << "- ARM: 0x" << board.arm().version() << std::endl;
    os << std::dec;

    return os;
}

namespace Application::Helper
{
    std::optional<std::reference_wrapper<BoardComponents::Loopback>> get_loopback(Board& board, unsigned int channel_index)
    {
        try { return board.firmware_loopback(channel_index); } catch (const UnavailableResource& e) { }
        try { return board.active_loopback(channel_index); } catch (const UnavailableResource& e) { }
        try { return board.passive_loopback(channel_index); } catch (const UnavailableResource& e) { }
        return std::nullopt;
    }

    void enable_loopback(Board& board, unsigned int channel_index)
    {
        auto optional_loopback = get_loopback(board, channel_index);
        if (optional_loopback)
            optional_loopback.value().get().enable();
    }

    void disable_loopback(Board& board, unsigned int channel_index)
    {
        auto optional_loopback = get_loopback(board, channel_index);
        if (optional_loopback)
            optional_loopback.value().get().disable();
    }

    VHD_STREAMTYPE rx_index_to_streamtype(unsigned int rx_index)
    {
        switch (rx_index)
        {
        case 0: return VHD_ST_RX0;
        case 1: return VHD_ST_RX1;
        case 2: return VHD_ST_RX2;
        case 3: return VHD_ST_RX3;
        case 4: return VHD_ST_RX4;
        case 5: return VHD_ST_RX5;
        case 6: return VHD_ST_RX6;
        case 7: return VHD_ST_RX7;
        case 8: return VHD_ST_RX8;
        case 9: return VHD_ST_RX9;
        case 10: return VHD_ST_RX10;
        case 11: return VHD_ST_RX11;
        default:
            throw std::invalid_argument("Invalid RX index");
        }
    }

    bool wait_for_input(BoardComponents::RxConnector& rx_connector, const std::atomic_bool& stop_is_requested)
    {
        while (!stop_is_requested && !rx_connector.signal_present())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return rx_connector.signal_present();
    }

    Streams open_stream(Board& board, VHD_STREAMTYPE stream_type)
    {
        return std::move(board.sdi().open_stream(stream_type, VHD_SDI_STPROC_DISJOINED_VIDEO));
    }

    Stream& to_base_stream(Streams& stream)
    {
        return std::visit(overloaded{
            [](SdiStream& sdi_stream) -> Stream& { return sdi_stream; },
            [](DvStream& dv_stream) -> Stream& { return dv_stream; }
        }, stream);
    }

    void configure_stream(Streams& stream, const SignalInformations& signal_information)
    {
        std::visit(overloaded{
            [&signal_information](SdiStream& sdi_stream)
            {
                const SdiSignalInformation& sdi_signal_information = std::get<SdiSignalInformation>(signal_information);
                sdi_stream.set_video_standard(sdi_signal_information.video_standard);
                sdi_stream.set_interface(sdi_signal_information.video_interface);
            },
            [&signal_information](DvStream& dv_stream)
            {
                const DvSignalInformation& dv_signal_information = std::get<DvSignalInformation>(signal_information);
                dv_stream.set_active_width(dv_signal_information.width);
                dv_stream.set_active_height(dv_signal_information.height);
                dv_signal_information.progressive ? dv_stream.set_progressive() : dv_stream.set_interlaced();
                dv_stream.set_frame_rate(dv_signal_information.framerate);
            }
        }, stream);
    }

    void print_information(const SignalInformations& signal_information, const std::string& prefix /*= ""*/)
    {
        std::visit(overloaded{
            [&prefix](const SdiSignalInformation& sdi_signal_info)
            {
                std::cout << prefix << "Video standard: " << to_pretty_string(sdi_signal_info.video_standard) << std::endl;
                std::cout << prefix << "Clock divisor: " << to_pretty_string(sdi_signal_info.clock_divisor) << std::endl;
                std::cout << prefix << "Interface: " << to_pretty_string(sdi_signal_info.video_interface) << std::endl;
            },
            [&prefix](const DvSignalInformation& dv_signal_info)
            {
                std::cout << prefix << dv_signal_info.width << "x" << dv_signal_info.height 
                                    << (dv_signal_info.progressive ? "p" : "i") 
                                    << dv_signal_info.framerate << std::endl;
            }
        }, signal_information);
    }

    SignalInformations detect_information(Streams& stream)
    {
        return std::visit(overloaded{
            [](SdiStream& sdi_stream) -> SignalInformations
            {
                return SdiSignalInformation{sdi_stream.video_standard(), sdi_stream.clock_divisor(), sdi_stream.interface()};
            },
            [](DvStream& dv_stream) -> SignalInformations
            {
                return DvSignalInformation{dv_stream.active_width(), dv_stream.active_height(), !dv_stream.interlaced(), dv_stream.frame_rate()};
            }
        }, stream);
    }

    VideoCharacteristics get_video_characteristics(const SignalInformations& signal_information)
    {
        return std::visit(overloaded{
            [](const SdiSignalInformation& sdi_signal_info) -> VideoCharacteristics
            {
                return Sdi::video_standard_to_characteristics(sdi_signal_info.video_standard);
            },
            [](const DvSignalInformation& dv_signal_info) -> VideoCharacteristics
            {
                return { dv_signal_info.width, dv_signal_info.height, !dv_signal_info.progressive, dv_signal_info.framerate };
            }
        }, signal_information);
    }
}