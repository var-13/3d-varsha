#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace thermobench {
namespace embedded {

enum class DeviceType {
    MCU,
    FPGA,
    JTAG
};

enum class DeviceState {
    DISCONNECTED,
    CONNECTED,
    TIMEOUT_ERROR,
    RECOVERED
};

struct DeviceInfo {
    std::string id;
    std::string name;
    std::string type_str;
    std::string state_str;
    DeviceType type{DeviceType::MCU};
    DeviceState state{DeviceState::DISCONNECTED};
    std::string firmware_version;
    double voltage_rail_V{3.3};
    bool simulated{true};
};

struct TelemetryPacket {
    std::string device_id;
    uint64_t timestamp_ms{0};
    uint32_t packet_seq{0};
    double temperature_C{35.0};
    double voltage_V{3.3};
    double current_mA{120.0};
    double power_mW{396.0};
    std::string state_str{"CONNECTED"};
    std::string status_msg{"Telemetry OK"};
    bool fault_detected{false};
};

class MockDevice {
public:
    DeviceInfo info;
    TelemetryPacket last_telemetry;
    int fault_counter{0};
    bool in_timeout{false};

    explicit MockDevice(const DeviceInfo& d);

    bool connect();
    bool disconnect();
    void trigger_timeout(int timeout_ms = 1000);
    bool recover();
    TelemetryPacket read_telemetry();
};

class EmbeddedManager {
public:
    static EmbeddedManager& instance();

    std::vector<DeviceInfo> discover();
    bool connect_device(const std::string& id);
    bool disconnect_device(const std::string& id);
    TelemetryPacket get_telemetry(const std::string& id);
    bool trigger_timeout(const std::string& id, int timeout_ms = 1000);
    bool recover_device(const std::string& id);

    std::string devices_to_json();
    std::string telemetry_to_json(const std::string& id);
    std::string all_telemetry_to_json();

    void reset();

private:
    EmbeddedManager();
    std::mutex m_mutex;
    std::map<std::string, MockDevice> m_devices;

    void init_default_devices();
};

} // namespace embedded
} // namespace thermobench
