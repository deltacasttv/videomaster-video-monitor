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
#include <optional>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <VideoMasterCppApi/helper/sdi.hpp>

using namespace Deltacast::Wrapper;
using namespace Deltacast::Wrapper::Helper;

std::ostream& operator<<(std::ostream& os, Board& board)
{
    os << "\t" << "Board " << board.index() << ":  [ " << board.name() << " ]" << std::endl;
    os << "\t" << "\t" << "- " << board.number_of_rx() << " RX / " << board.number_of_tx() << " TX" << std::endl;
    os << "\t" << "\t" << "- Driver: " << board.driver_version() << std::endl;
    os << "\t" << "\t" << "- PCIe ID: " << board.pcie_identifier() << std::endl;
    os << "\t" << "\t" << "- SN: " << board.serial_number() << std::endl;
    auto [ pcie_bus, number_of_lanes ] = board.pcie();
    os << "\t" << "\t" << "- " << to_pretty_string(pcie_bus) << ", " << number_of_lanes << " lanes" << std::endl;

    os << std::hex;
    os << "\t" << "\t" << "- Firmware: 0x" << board.fpga().version() << std::endl;
    if (board.has_scp())
        os << "\t" << "\t" << "- SCP: 0x" << board.scp().version() << std::endl;
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

    SdiSignalInformation detect_information(SdiStream& sdi_stream)
    {
        return { sdi_stream.video_standard(), sdi_stream.clock_divisor(), sdi_stream.interface() };
    }

    VideoCharacteristics get_video_characteristics(const SdiSignalInformation& sdi_signal_information)
    {
        return Sdi::video_standard_to_characteristics(sdi_signal_information.video_standard);
    }
}