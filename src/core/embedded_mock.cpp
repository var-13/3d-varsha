#include "core/embedded_mock.hpp"
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace thermobench {
namespace embedded {

namespace {

uint64_t current_time_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string state_to_string(DeviceState state) {
    switch (state) {
        case DeviceState::DISCONNECTED: return "DISCONNECTED";
        case DeviceState::CONNECTED: return "CONNECTED";
        case DeviceState::TIMEOUT_ERROR: return "TIMEOUT_ERROR";
        case DeviceState::RECOVERED: return "CONNECTED"; // Recovered state transitions to active connected
    }
    return "UNKNOWN";
}

} // namespace

MockDevice::MockDevice(const DeviceInfo& d) : info(d) {
    info.state_str = state_to_string(info.state);
    last_telemetry.device_id = d.id;
    last_telemetry.voltage_V = d.voltage_rail_V;
    last_telemetry.state_str = info.state_str;
}

bool MockDevice::connect() {
    info.state = DeviceState::CONNECTED;
    info.state_str = "CONNECTED";
    in_timeout = false;
    last_telemetry.fault_detected = false;
    last_telemetry.state_str = "CONNECTED";
    last_telemetry.status_msg = "Device connected and streaming telemetry";
    return true;
}

bool MockDevice::disconnect() {
    info.state = DeviceState::DISCONNECTED;
    info.state_str = "DISCONNECTED";
    last_telemetry.state_str = "DISCONNECTED";
    last_telemetry.status_msg = "Device disconnected";
    return true;
}

void MockDevice::trigger_timeout(int timeout_ms) {
    in_timeout = true;
    info.state = DeviceState::TIMEOUT_ERROR;
    info.state_str = "TIMEOUT_ERROR";
    fault_counter++;

    last_telemetry.fault_detected = true;
    last_telemetry.state_str = "TIMEOUT_ERROR";
    std::ostringstream oss;
    oss << "ERR_TIMEOUT: Response dropped after " << timeout_ms << " ms. No ACK received.";
    last_telemetry.status_msg = oss.str();
}

bool MockDevice::recover() {
    in_timeout = false;
    info.state = DeviceState::CONNECTED;
    info.state_str = "CONNECTED";

    last_telemetry.fault_detected = false;
    last_telemetry.state_str = "CONNECTED";
    last_telemetry.status_msg = "Recovery successful: bus re-enumerated and frame sync restored.";
    return true;
}

TelemetryPacket MockDevice::read_telemetry() {
    last_telemetry.timestamp_ms = current_time_ms();
    last_telemetry.packet_seq++;

    if (info.state == DeviceState::DISCONNECTED) {
        last_telemetry.state_str = "DISCONNECTED";
        last_telemetry.status_msg = "Device offline";
        return last_telemetry;
    }

    if (in_timeout) {
        last_telemetry.fault_detected = true;
        last_telemetry.state_str = "TIMEOUT_ERROR";
        last_telemetry.status_msg = "COMMUNICATION TIMEOUT: Keep-alive frame lost!";
        return last_telemetry;
    }

    // Realistic small fluctuation per device type
    double seq_w = last_telemetry.packet_seq * 0.1;
    if (info.type == DeviceType::MCU) {
        last_telemetry.temperature_C = 42.5 + 1.2 * std::sin(seq_w);
        last_telemetry.voltage_V = 3.30 + 0.02 * std::cos(seq_w);
        last_telemetry.current_mA = 115.0 + 8.0 * std::sin(seq_w * 0.5);
    } else if (info.type == DeviceType::FPGA) {
        last_telemetry.temperature_C = 58.0 + 2.5 * std::sin(seq_w);
        last_telemetry.voltage_V = 1.80 + 0.015 * std::cos(seq_w);
        last_telemetry.current_mA = 480.0 + 35.0 * std::sin(seq_w * 0.7);
    } else { // JTAG
        last_telemetry.temperature_C = 31.0 + 0.5 * std::sin(seq_w);
        last_telemetry.voltage_V = 5.02 + 0.01 * std::cos(seq_w);
        last_telemetry.current_mA = 45.0 + 2.0 * std::sin(seq_w * 0.3);
    }

    last_telemetry.power_mW = last_telemetry.voltage_V * last_telemetry.current_mA;
    last_telemetry.state_str = "CONNECTED";
    last_telemetry.status_msg = "Telemetry healthy (CRC16 verified)";
    last_telemetry.fault_detected = false;

    return last_telemetry;
}

EmbeddedManager& EmbeddedManager::instance() {
    static EmbeddedManager mgr;
    return mgr;
}

EmbeddedManager::EmbeddedManager() {
    init_default_devices();
}

void EmbeddedManager::init_default_devices() {
    m_devices.clear();

    DeviceInfo mcu;
    mcu.id = "mcu_stm32";
    mcu.name = "STM32H743ZI Thermal Controller";
    mcu.type = DeviceType::MCU;
    mcu.type_str = "MCU";
    mcu.firmware_version = "v2.4.1-rc3";
    mcu.voltage_rail_V = 3.3;
    mcu.state = DeviceState::CONNECTED;
    mcu.state_str = "CONNECTED";
    mcu.simulated = true;
    m_devices.emplace(mcu.id, MockDevice(mcu));

    DeviceInfo fpga;
    fpga.id = "fpga_artix7";
    fpga.name = "Xilinx Artix-7 XC7A35T Coprocessor";
    fpga.type = DeviceType::FPGA;
    fpga.type_str = "FPGA";
    fpga.firmware_version = "Bitstream v1.1.0";
    fpga.voltage_rail_V = 1.8;
    fpga.state = DeviceState::CONNECTED;
    fpga.state_str = "CONNECTED";
    fpga.simulated = true;
    m_devices.emplace(fpga.id, MockDevice(fpga));

    DeviceInfo jtag;
    jtag.id = "jtag_ftdi";
    jtag.name = "FTDI FT2232H JTAG/UART Bridge";
    jtag.type = DeviceType::JTAG;
    jtag.type_str = "JTAG";
    jtag.firmware_version = "Driver v2.12.36";
    jtag.voltage_rail_V = 5.0;
    jtag.state = DeviceState::CONNECTED;
    jtag.state_str = "CONNECTED";
    jtag.simulated = true;
    m_devices.emplace(jtag.id, MockDevice(jtag));
}

void EmbeddedManager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    init_default_devices();
}

std::vector<DeviceInfo> EmbeddedManager::discover() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DeviceInfo> result;
    result.reserve(m_devices.size());
    for (const auto& kv : m_devices) {
        result.push_back(kv.second.info);
    }
    return result;
}

bool EmbeddedManager::connect_device(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_devices.find(id);
    if (it == m_devices.end()) return false;
    return it->second.connect();
}

bool EmbeddedManager::disconnect_device(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_devices.find(id);
    if (it == m_devices.end()) return false;
    return it->second.disconnect();
}

TelemetryPacket EmbeddedManager::get_telemetry(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_devices.find(id);
    if (it == m_devices.end()) {
        throw std::invalid_argument("Device not found: " + id);
    }
    return it->second.read_telemetry();
}

bool EmbeddedManager::trigger_timeout(const std::string& id, int timeout_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_devices.find(id);
    if (it == m_devices.end()) return false;
    it->second.trigger_timeout(timeout_ms);
    return true;
}

bool EmbeddedManager::recover_device(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_devices.find(id);
    if (it == m_devices.end()) return false;
    return it->second.recover();
}

std::string EmbeddedManager::devices_to_json() {
    auto devs = discover();
    std::ostringstream oss;
    oss << "{\"device_count\":" << devs.size() << ",\"devices\":[";
    for (size_t i = 0; i < devs.size(); ++i) {
        const auto& d = devs[i];
        if (i > 0) oss << ",";
        oss << "{\"id\":\"" << d.id << "\""
            << ",\"name\":\"" << d.name << "\""
            << ",\"type\":\"" << d.type_str << "\""
            << ",\"state\":\"" << d.state_str << "\""
            << ",\"firmware\":\"" << d.firmware_version << "\""
            << ",\"voltage_rail_V\":" << d.voltage_rail_V
            << ",\"simulated\":true}";
    }
    oss << "]}";
    return oss.str();
}

std::string EmbeddedManager::telemetry_to_json(const std::string& id) {
    auto t = get_telemetry(id);
    std::ostringstream oss;
    oss << "{\"device_id\":\"" << t.device_id << "\""
        << ",\"timestamp_ms\":" << t.timestamp_ms
        << ",\"packet_seq\":" << t.packet_seq
        << ",\"temperature_C\":" << t.temperature_C
        << ",\"voltage_V\":" << t.voltage_V
        << ",\"current_mA\":" << t.current_mA
        << ",\"power_mW\":" << t.power_mW
        << ",\"state\":\"" << t.state_str << "\""
        << ",\"status_msg\":\"" << t.status_msg << "\""
        << ",\"fault_detected\":" << (t.fault_detected ? "true" : "false") << "}";
    return oss.str();
}

std::string EmbeddedManager::all_telemetry_to_json() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\"timestamp_ms\":" << current_time_ms() << ",\"packets\":[";
    bool first = true;
    for (auto& kv : m_devices) {
        if (!first) oss << ",";
        first = false;
        auto t = kv.second.read_telemetry();
        oss << "{\"device_id\":\"" << t.device_id << "\""
            << ",\"name\":\"" << kv.second.info.name << "\""
            << ",\"type\":\"" << kv.second.info.type_str << "\""
            << ",\"packet_seq\":" << t.packet_seq
            << ",\"temperature_C\":" << t.temperature_C
            << ",\"voltage_V\":" << t.voltage_V
            << ",\"current_mA\":" << t.current_mA
            << ",\"power_mW\":" << t.power_mW
            << ",\"state\":\"" << t.state_str << "\""
            << ",\"status_msg\":\"" << t.status_msg << "\""
            << ",\"fault_detected\":" << (t.fault_detected ? "true" : "false") << "}";
    }
    oss << "]}";
    return oss.str();
}

} // namespace embedded
} // namespace thermobench
