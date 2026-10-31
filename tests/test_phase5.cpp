#include "test_runner.hpp"
#include "core/embedded_mock.hpp"
#include <string>

using namespace thermobench;

namespace {

TEST("Phase 5 - embedded: discover returns 3 simulated target devices") {
    auto& mgr = embedded::EmbeddedManager::instance();
    mgr.reset();

    auto devs = mgr.discover();
    ASSERT(devs.size() == 3);

    bool found_mcu = false;
    bool found_fpga = false;
    bool found_jtag = false;

    for (const auto& d : devs) {
        ASSERT(d.simulated);
        ASSERT(!d.firmware_version.empty());
        if (d.id == "mcu_stm32") {
            found_mcu = true;
            ASSERT_NEAR(d.voltage_rail_V, 3.3, 0.05);
        } else if (d.id == "fpga_artix7") {
            found_fpga = true;
            ASSERT_NEAR(d.voltage_rail_V, 1.8, 0.05);
        } else if (d.id == "jtag_ftdi") {
            found_jtag = true;
            ASSERT_NEAR(d.voltage_rail_V, 5.0, 0.05);
        }
    }

    ASSERT(found_mcu);
    ASSERT(found_fpga);
    ASSERT(found_jtag);
}

TEST("Phase 5 - embedded: read_telemetry streams realistic metrics and verifies power") {
    auto& mgr = embedded::EmbeddedManager::instance();
    mgr.reset();

    auto mcu_t = mgr.get_telemetry("mcu_stm32");
    ASSERT(mcu_t.device_id == "mcu_stm32");
    ASSERT(mcu_t.packet_seq > 0);
    ASSERT_NEAR(mcu_t.voltage_V, 3.30, 0.1);
    ASSERT(mcu_t.current_mA > 50.0);
    ASSERT_NEAR(mcu_t.power_mW, mcu_t.voltage_V * mcu_t.current_mA, 1e-4);
    ASSERT(!mcu_t.fault_detected);

    auto fpga_t = mgr.get_telemetry("fpga_artix7");
    ASSERT(fpga_t.device_id == "fpga_artix7");
    ASSERT_NEAR(fpga_t.voltage_V, 1.80, 0.1);
    ASSERT(fpga_t.temperature_C >= 40.0);
    ASSERT(!fpga_t.fault_detected);
}

TEST("Phase 5 - embedded: connection lifecycle connect and disconnect") {
    auto& mgr = embedded::EmbeddedManager::instance();
    mgr.reset();

    ASSERT(mgr.disconnect_device("mcu_stm32"));
    auto t1 = mgr.get_telemetry("mcu_stm32");
    ASSERT(t1.state_str == "DISCONNECTED");

    ASSERT(mgr.connect_device("mcu_stm32"));
    auto t2 = mgr.get_telemetry("mcu_stm32");
    ASSERT(t2.state_str == "CONNECTED");
}

TEST("Phase 5 - embedded: timeout fault injection and automated recovery with bus re-enumeration") {
    auto& mgr = embedded::EmbeddedManager::instance();
    mgr.reset();

    // Trigger timeout fault
    ASSERT(mgr.trigger_timeout("fpga_artix7", 500));
    auto t_fault = mgr.get_telemetry("fpga_artix7");
    ASSERT(t_fault.fault_detected);
    ASSERT(t_fault.state_str == "TIMEOUT_ERROR");
    ASSERT(t_fault.status_msg.find("TIMEOUT") != std::string::npos);

    // Automated recovery
    ASSERT(mgr.recover_device("fpga_artix7"));
    auto t_rec = mgr.get_telemetry("fpga_artix7");
    ASSERT(!t_rec.fault_detected);
    ASSERT(t_rec.state_str == "CONNECTED");
}

TEST("Phase 5 - embedded: serializers and error handling on unknown ID") {
    auto& mgr = embedded::EmbeddedManager::instance();
    mgr.reset();

    std::string devs_json = mgr.devices_to_json();
    ASSERT(!devs_json.empty());
    ASSERT(devs_json.find("\"device_count\":3") != std::string::npos);
    ASSERT(devs_json.find("\"simulated\":true") != std::string::npos);

    std::string telem_json = mgr.all_telemetry_to_json();
    ASSERT(!telem_json.empty());
    ASSERT(telem_json.find("\"packets\":[") != std::string::npos);

    ASSERT_THROWS(mgr.get_telemetry("non_existent_device"));
}

} // namespace
