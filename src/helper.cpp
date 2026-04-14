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
#include "exceptions.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/board/rx/rx.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <VideoMasterCppApi/helper/sdi.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterHD_Core.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ios>
#include <ostream>
#include <thread>

auto operator<<(std::ostream& output_stream, Deltacast::Wrapper::Board& board) -> std::ostream&
{
    output_stream << "\t" << "Board " << board.index() << ":  [ " << board.name() << " ]"
                  << std::endl;
    output_stream << "\t" << "\t" << "- " << board.number_of_rx() << " RX / "
                  << board.number_of_tx() << " TX" << std::endl;
    output_stream << "\t" << "\t" << "- Driver: " << board.driver_version() << std::endl;
    output_stream << "\t" << "\t" << "- PCIe ID: " << board.pcie_identifier() << std::endl;
    output_stream << "\t" << "\t" << "- SN: " << board.serial_number() << std::endl;
    auto [pcie_bus, number_of_lanes] = board.pcie();
    output_stream << "\t" << "\t" << "- " << Deltacast::Wrapper::to_pretty_string(pcie_bus) << ", "
                  << number_of_lanes << " lanes" << std::endl;

    output_stream << std::hex;
    output_stream << "\t" << "\t" << "- Firmware: 0x" << board.fpga().version() << std::endl;
    if (board.has_scp())
    {
        output_stream << "\t" << "\t" << "- SCP: 0x" << board.scp().version() << std::endl;
    }
    output_stream << std::dec;

    return output_stream;
}

namespace Deltacast::VideoMonitor::Helper
{
    namespace
    {
        constexpr std::array<VHD_STREAMTYPE, 12> rx_stream_types = {
            VHD_ST_RX0, VHD_ST_RX1, VHD_ST_RX2, VHD_ST_RX3, VHD_ST_RX4,  VHD_ST_RX5,
            VHD_ST_RX6, VHD_ST_RX7, VHD_ST_RX8, VHD_ST_RX9, VHD_ST_RX10, VHD_ST_RX11
        };

        constexpr uint32_t wait_for_input_timeout_ms = 100;
    }  // namespace

    auto rx_index_to_streamtype(unsigned int rx_index) -> VHD_STREAMTYPE
    {
        if (rx_index >= rx_stream_types.size())
        {
            throw Deltacast::VideoMonitor::Exceptions::SignalDetectionException("Invalid RX index");
        }
        return rx_stream_types[rx_index];
    }

    auto wait_for_input(Deltacast::Wrapper::BoardComponents::RxConnector& rx_connector,
                        const std::atomic_bool&                           stop_is_requested) -> bool
    {
        while (!stop_is_requested && !rx_connector.signal_present())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_for_input_timeout_ms));
        }

        return rx_connector.signal_present();
    }
}  // namespace Deltacast::VideoMonitor::Helper