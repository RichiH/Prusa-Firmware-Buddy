#include <render.hpp>

#include <catch2/catch.hpp>

#include <cstring>
#include <string_view>

using std::string_view;
using namespace con;
using namespace json;

namespace {

constexpr const char *const print_file = "/usb/box.gco";
constexpr const char *const rejected_event = "{\"command_id\":11,\"event\":\"REJECTED\"}";

constexpr device_params_t params_printing() {
    device_params_t params {};

    params.job_id = 42;
    params.progress_percent = 12;
    params.job_path = print_file;
    params.temp_bed = 65;
    params.temp_nozzle = 200;
    params.target_bed = 70;
    params.target_nozzle = 195;
    params.state = DeviceState::Printing;

    return params;
};

constexpr device_params_t params_idle() {
    device_params_t params {};

    params.job_id = 13;
    params.state = DeviceState::Idle;

    return params;
}

constexpr printer_info_t printer_info() {
    printer_info_t info {};

    info.appendix = false;
    strcpy(info.fingerprint, "DEADBEEF");
    strcpy(info.firmware_version, "TST-1234");
    strcpy(info.serial_number, "FAKE-1234");

    return info;
}

}

TEST_CASE("Render") {
    string_view expected;
    printer_info_t info = printer_info();
    device_params_t params {};
    Action action;

    SECTION("Telemetry - empty") {
        params = params_printing();
        action = SendTelemetry { true };
        expected = "{}";
    }

    SECTION("Telemetry - printing") {
        params = params_printing();
        action = SendTelemetry { false };
        // clang-format off
        expected = "{"
            "\"temp_nozzle\":200.0,"
            "\"temp_bed\":65.0,"
            "\"target_nozzle\":195.0,"
            "\"target_bed\":70.0,"
            "\"axis_z\":0.00,"
            "\"job_id\":42,"
            "\"speed\":0,"
            "\"flow\":0,"
            "\"time_printing\":0,"
            "\"time_remaining\":0,"
            "\"progress\":12,"
            "\"fan_extruder\":0,"
            "\"fan_print\":0,"
            "\"filament\":0.0,"
            "\"state\":\"PRINTING\""
        "}";
        // clang-format on
    }

    SECTION("Telemetry - idle") {
        params = params_idle();
        action = SendTelemetry { false };
        // clang-format off
        expected = "{"
            "\"temp_nozzle\":0.0,"
            "\"temp_bed\":0.0,"
            "\"target_nozzle\":0.0,"
            "\"target_bed\":0.0,"
            "\"axis_x\":0.00,"
            "\"axis_y\":0.00,"
            "\"axis_z\":0.00,"
            "\"state\":\"IDLE\""
        "}";
        // clang-format on
    }

    SECTION("Event - rejected") {
        action = Event {
            EventType::Rejected,
            11,
        };
        params = params_idle();
        expected = rejected_event;
    }

    SECTION("Event - job info") {
        action = Event {
            EventType::JobInfo,
            11,
            42,
        };
        params = params_printing();
        // clang-format off
        expected = "{"
            "\"job_id\":42,"
            "\"data\":{\"path\":\"/usb/box.gco\"},"
            "\"command_id\":11,"
            "\"event\":\"JOB_INFO\""
        "}";
        // clang-format on
    }

    SECTION("Even - job info not printing") {
        action = Event {
            EventType::JobInfo,
            11,
            42,
        };
        params = params_idle();
        expected = rejected_event;
    }

    SECTION("Even - job info - invalid job ID") {
        action = Event {
            EventType::JobInfo,
            11,
            13,
        };
        params = params_printing();
        expected = rejected_event;
    }

    SECTION("Event - info") {
        action = Event {
            EventType::Info,
            11,
        };
        params = params_idle();

        // clang-format off
        expected = "{"
            "\"data\":{"
                "\"firmware\":\"TST-1234\","
                "\"sn\":\"FAKE-1234\","
                "\"appendix\":false,"
                "\"fingerprint\":\"DEADBEEF\""
            "},"
            "\"command_id\":11,"
            "\"event\":\"INFO\""
        "}";
        // clang-format on
    }

    RenderState state {
        info,
        params,
        action,
    };
    Renderer renderer(std::move(state));
    uint8_t buffer[1024];
    const auto [result, amount] = renderer.render(buffer, sizeof buffer);

    REQUIRE(result == JsonResult::Complete);

    const uint8_t *b = buffer;
    string_view output(reinterpret_cast<const char *>(b), amount);
    REQUIRE(output == expected);
}
