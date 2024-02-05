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

#include "device.hpp"

#include "api_helper/handle_manager.hpp"

#include <string>
#include <thread>
#include <unordered_map>

using Deltacast::Helper::ApiSuccess;

const std::unordered_map<uint32_t, VHD_CORE_BOARDPROPERTY> id_to_passive_loopback_prop = {
    {0, VHD_CORE_BP_BYPASS_RELAY_0},
    {1, VHD_CORE_BP_BYPASS_RELAY_1},
    {2, VHD_CORE_BP_BYPASS_RELAY_2},
    {3, VHD_CORE_BP_BYPASS_RELAY_3}
};
const std::unordered_map<uint32_t, VHD_CORE_BOARDPROPERTY> id_to_active_loopback_prop =
                                                            {{0, VHD_CORE_BP_ACTIVE_LOOPBACK_0}};
const std::unordered_map<uint32_t, VHD_CORE_BOARDPROPERTY> id_to_firmware_loopback_prop =
                                                            {{0, VHD_CORE_BP_FIRMWARE_LOOPBACK_0}};

const std::unordered_map<uint32_t, VHD_CORE_BOARDPROPERTY> id_to_rx_status_prop = {
    {0, VHD_CORE_BP_RX0_STATUS},
    {1, VHD_CORE_BP_RX1_STATUS},
    {2, VHD_CORE_BP_RX2_STATUS},
    {3, VHD_CORE_BP_RX3_STATUS},
    {4, VHD_CORE_BP_RX4_STATUS},
    {5, VHD_CORE_BP_RX5_STATUS},
    {6, VHD_CORE_BP_RX6_STATUS},
    {7, VHD_CORE_BP_RX7_STATUS},
    {8, VHD_CORE_BP_RX8_STATUS},
    {9, VHD_CORE_BP_RX9_STATUS},
    {10, VHD_CORE_BP_RX10_STATUS},
    {11, VHD_CORE_BP_RX11_STATUS},
};
const std::unordered_map<uint32_t, VHD_SDI_BOARDPROPERTY> id_to_rx_video_standard_prop = {
    {0, VHD_SDI_BP_RX0_STANDARD},
    {1, VHD_SDI_BP_RX1_STANDARD},
    {2, VHD_SDI_BP_RX2_STANDARD},
    {3, VHD_SDI_BP_RX3_STANDARD},
    {4, VHD_SDI_BP_RX4_STANDARD},
    {5, VHD_SDI_BP_RX5_STANDARD},
    {6, VHD_SDI_BP_RX6_STANDARD},
    {7, VHD_SDI_BP_RX7_STANDARD},
    {8, VHD_SDI_BP_RX8_STANDARD},
    {9, VHD_SDI_BP_RX9_STANDARD},
    {10, VHD_SDI_BP_RX10_STANDARD},
    {11, VHD_SDI_BP_RX11_STANDARD},
};
const std::unordered_map<uint32_t, VHD_SDI_BOARDPROPERTY> id_to_rx_clock_divisor_prop = {
    {0, VHD_SDI_BP_RX0_CLOCK_DIV},
    {1, VHD_SDI_BP_RX1_CLOCK_DIV},
    {2, VHD_SDI_BP_RX2_CLOCK_DIV},
    {3, VHD_SDI_BP_RX3_CLOCK_DIV},
    {4, VHD_SDI_BP_RX4_CLOCK_DIV},
    {5, VHD_SDI_BP_RX5_CLOCK_DIV},
    {6, VHD_SDI_BP_RX6_CLOCK_DIV},
    {7, VHD_SDI_BP_RX7_CLOCK_DIV},
    {8, VHD_SDI_BP_RX8_CLOCK_DIV},
    {9, VHD_SDI_BP_RX9_CLOCK_DIV},
    {10, VHD_SDI_BP_RX10_CLOCK_DIV},
    {11, VHD_SDI_BP_RX11_CLOCK_DIV},
};
const std::unordered_map<uint32_t, VHD_SDI_BOARDPROPERTY> id_to_rx_interface_prop = {
    {0, VHD_SDI_BP_RX0_INTERFACE},
    {1, VHD_SDI_BP_RX1_INTERFACE},
    {2, VHD_SDI_BP_RX2_INTERFACE},
    {3, VHD_SDI_BP_RX3_INTERFACE},
    {4, VHD_SDI_BP_RX4_INTERFACE},
    {5, VHD_SDI_BP_RX5_INTERFACE},
    {6, VHD_SDI_BP_RX6_INTERFACE},
    {7, VHD_SDI_BP_RX7_INTERFACE},
    {8, VHD_SDI_BP_RX8_INTERFACE},
    {9, VHD_SDI_BP_RX9_INTERFACE},
    {10, VHD_SDI_BP_RX10_INTERFACE},
    {11, VHD_SDI_BP_RX11_INTERFACE},
};

Deltacast::Device::~Device()
{
    ULONG number_of_rx_channels = 0;
    VHD_GetBoardProperty(*handle(), VHD_CORE_BP_NB_RXCHANNELS, &number_of_rx_channels);

    for (auto i = 0; i < number_of_rx_channels; i++)
        enable_loopback(i);
}

std::unique_ptr<Deltacast::Device> Deltacast::Device::create(int device_index)
{
    auto device_handle = Helper::get_board_handle(device_index);
    if (!device_handle)
        return nullptr;
    
    return std::unique_ptr<Device>(new Device(device_index, std::move(device_handle)));
}

bool Deltacast::Device::set_loopback_state(int index, bool enabled)
{
    ULONG has_passive_loopback = FALSE;
    ULONG has_active_loopback = FALSE;
    ULONG has_firmware_loopback = FALSE;

    VHD_GetBoardCapability(*handle(), VHD_CORE_BOARD_CAP_PASSIVE_LOOPBACK, &has_passive_loopback);
    VHD_GetBoardCapability(*handle(), VHD_CORE_BOARD_CAP_ACTIVE_LOOPBACK, &has_active_loopback);
    VHD_GetBoardCapability(*handle(), VHD_CORE_BOARD_CAP_FIRMWARE_LOOPBACK, &has_firmware_loopback);

    if (has_firmware_loopback && id_to_firmware_loopback_prop.find(index) != id_to_firmware_loopback_prop.end())
        return ApiSuccess{VHD_SetBoardProperty(*handle(), id_to_firmware_loopback_prop.at(index), enabled)};
    else if (has_active_loopback && id_to_active_loopback_prop.find(index) != id_to_active_loopback_prop.end())
        return ApiSuccess{VHD_SetBoardProperty(*handle(), id_to_active_loopback_prop.at(index), enabled)};
    else if (has_passive_loopback && id_to_passive_loopback_prop.find(index) != id_to_passive_loopback_prop.end())
        return ApiSuccess{VHD_SetBoardProperty(*handle(), id_to_passive_loopback_prop.at(index), enabled)};
    return true;
}

void Deltacast::Device::enable_loopback(int index)
{
    set_loopback_state(index, true);
}

void Deltacast::Device::disable_loopback(int index)
{
    set_loopback_state(index, false);
}

bool Deltacast::Device::wait_for_incoming_signal(int rx_index, const std::atomic_bool& stop_is_requested)
{
    if (id_to_rx_status_prop.find(rx_index) == id_to_rx_status_prop.end())
        return false;

    while (!stop_is_requested.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ULONG status = VHD_CORE_RXSTS_UNLOCKED;
        auto api_success = ApiSuccess{VHD_GetBoardProperty(*handle(), id_to_rx_status_prop.at(rx_index), &status)};
        if (api_success && !(status & VHD_CORE_RXSTS_UNLOCKED))
            return true;
    }

    return false;
}

Deltacast::SignalInformation Deltacast::Device::get_incoming_signal_information(int rx_index)
{
    if ((id_to_rx_video_standard_prop.find(rx_index) == id_to_rx_video_standard_prop.end())
        || (id_to_rx_clock_divisor_prop.find(rx_index) == id_to_rx_clock_divisor_prop.end())
        || (id_to_rx_interface_prop.find(rx_index) == id_to_rx_interface_prop.end()))
        return {};

    SignalInformation signal_information;

    ApiSuccess api_success;
    if (!(api_success = ApiSuccess{VHD_GetBoardProperty(*handle(), id_to_rx_video_standard_prop.at(rx_index), (ULONG*)&signal_information.video_standard)})
        || !(api_success = ApiSuccess{VHD_GetBoardProperty(*handle(), id_to_rx_clock_divisor_prop.at(rx_index), (ULONG*)&signal_information.clock_divisor)})
        || !(api_success = ApiSuccess{VHD_GetBoardProperty(*handle(), id_to_rx_interface_prop.at(rx_index), (ULONG*)&signal_information.interface)}))
    {
        std::cout << "ERROR: Cannot get incoming signal information (" << api_success << ")" << std::endl;
        throw std::runtime_error("Cannot get incoming signal information");
    }

    return signal_information;
}

namespace Deltacast
{
    std::ostream& operator<<(std::ostream& os, const Device& device)
    {
        ULONG driver_version = 0, firmware_version, number_of_rx_channels, number_of_tx_channels;

        VHD_GetBoardProperty(**(device._device_handle), VHD_CORE_BP_DRIVER_VERSION, &driver_version);
        VHD_GetBoardProperty(**(device._device_handle), VHD_CORE_BP_FIRMWARE_VERSION, &firmware_version);
        VHD_GetBoardProperty(**(device._device_handle), VHD_CORE_BP_NB_RXCHANNELS, &number_of_rx_channels);
        VHD_GetBoardProperty(**(device._device_handle), VHD_CORE_BP_NB_TXCHANNELS, &number_of_tx_channels);

        char pcie_id_string[64];
        VHD_GetPCIeIdentificationString(device._device_index, pcie_id_string);

        os << "  Board " << device._device_index << ":  [ " << VHD_GetBoardModel(device._device_index) << " ]" << "\n";
        os << "    - PCIe Id string: " << pcie_id_string << "\n";
        os << "    - Driver v" << ((driver_version & 0xFF000000) >> 24) << "." << ((driver_version & 0x00FF0000) >> 16) << "." << ((driver_version & 0x0000FF00) >> 8) << "." << ((driver_version & 0x000000FF) >> 0) << "\n";

        os << std::hex;

        os << "    - Board fpga firmware v" << ((firmware_version & 0xFF000000) >> 24) << "." << ((firmware_version & 0x00FF0000) >> 16) << "." << ((firmware_version & 0x0000FF00) >> 8) << "." << ((firmware_version & 0x000000FF) >> 0) << "\n";
        
        os << std::dec;

        os << "    - " << number_of_rx_channels << " In / " << number_of_tx_channels << " Out" << "\n";

        return os;
    }
}