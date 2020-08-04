/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   MSP telemetry library backend base class
*/
#pragma once

#include <AP_RCTelemetry/AP_RCTelemetry.h>

#include "msp.h"

#include <time.h>

#if HAL_MSP_ENABLED

#define MSP_TIME_SLOT_MAX 12
#define CELLFULL 4.35
#define MSP_TXT_BUFFER_SIZE     15U // 11 + 3 utf8 chars + terminator
#define MSP_TXT_VISIBLE_CHARS   11U

using namespace MSP;

class AP_MSP;

class AP_MSP_Telem_Backend : AP_RCTelemetry
{
public:
    AP_MSP_Telem_Backend(AP_HAL::UARTDriver *uart);

    typedef struct battery_state_s {
        float batt_current_a;
        float batt_consumed_mah;
        float batt_voltage_v;
        int32_t batt_capacity_mah;
        uint8_t batt_cellcount;
        battery_state_e batt_state;
    } battery_state_t;

    typedef struct gps_state_s {
        int32_t gps_latitude;
        int32_t gps_longitude;
        uint8_t gps_num_sats;
        int32_t gps_altitude_cm;
        float gps_speed_ms;
        uint16_t gps_ground_course_cd;
        uint8_t gps_fix_type;
    } gps_state_t;

    typedef struct airspeed_state_s {
        float airspeed_estimate_ms;
        bool  airspeed_have_estimate;
    } airspeed_state_t;

    typedef struct home_state_s {
        bool home_is_set;
        float home_bearing_cd;
        uint32_t home_distance_m;
        int32_t rel_altitude_cm;
    } home_state_t;

    // init - perform required initialisation
    virtual bool init() override;
    virtual bool init_uart();
    virtual void enable_warnings();
    virtual void hide_osd_items(void);

    // MSP tx/rx processors
    void process_incoming_data();     // incoming data
    void process_outgoing_data();     // push outgoing data

protected:
    enum msp_packet_type : uint8_t {
        EMPTY_SLOT = 0,
        NAME,
        STATUS,
        CONFIG,
        RAW_GPS,
        COMP_GPS,
        ATTITUDE,
        ALTITUDE,
        ANALOG,
        BATTERY_STATE,
        ESC_SENSOR_DATA,
        RTC_DATETIME,
    };

    const uint16_t msp_packet_type_map[MSP_TIME_SLOT_MAX] = {
        0,
        MSP_NAME,
        MSP_STATUS,
        MSP_OSD_CONFIG,
        MSP_RAW_GPS,
        MSP_COMP_GPS,
        MSP_ATTITUDE,
        MSP_ALTITUDE,
        MSP_ANALOG,
        MSP_BATTERY_STATE,
        MSP_ESC_SENSOR_DATA,
        MSP_RTC
    };

    /* UTF-8 encodings
        U+2191 ↑       e2 86 91        UPWARDS ARROW
        U+2197 ↗       e2 86 97        NORTH EAST ARROW
        U+2192 →       e2 86 92        RIGHTWARDS ARROW
        U+2198 ↘       e2 86 98        SOUTH EAST ARROW
        U+2193 ↓       e2 86 93        DOWNWARDS ARROW
        U+2199 ↙       e2 86 99        SOUTH WEST ARROW
        U+2190 ←       e2 86 90        LEFTWARDS ARROW
        U+2196 ↖       e2 86 96        NORTH WEST ARROW
    */
    static constexpr uint8_t arrows[8] = {0x91, 0x97, 0x92, 0x98, 0x93, 0x99, 0x90, 0x96};

    static const uint8_t message_scroll_time_ms = 200;
    static const uint8_t message_scroll_delay = 5;

    // each backend can hide/unhide items dynamically
    uint64_t osd_hidden_items_bitmask;

    // MSP decoder status
    msp_port_t _msp_port;

    // passthrough WFQ scheduler
    bool is_packet_ready(uint8_t idx, bool queue_empty) override;
    void process_packet(uint8_t idx) override;
    void adjust_packet_weight(bool queue_empty) override {};
    void setup_wfq_scheduler(void) override;
    bool get_next_msg_chunk(void) override
    {
        return true;
    };

    // telemetry helpers
    uint8_t calc_cell_count(float battery_voltage);
    float get_vspeed_ms(void);
    void update_home_pos(home_state_t &home_state);
    void update_battery_state(battery_state_t &_battery_state);
    void update_gps_state(gps_state_t &gps_state);
    void update_airspeed(airspeed_state_t &airspeed_state);
    void update_flight_mode_str(char *flight_mode_str, bool wind_enabled);

    // MSP parsing
    void msp_process_received_command();
    MSPCommandResult msp_process_command(msp_packet_t *cmd, msp_packet_t *reply);
    MSPCommandResult msp_process_sensor_command(uint16_t cmd_msp, sbuf_t *src);
    MSPCommandResult msp_process_out_command(uint16_t cmd_msp, sbuf_t *dst);

    // MSP sensor command processing
    void msp_handle_opflow(const msp_opflow_sensor_t &pkt);
    void msp_handle_rangefinder(const msp_rangefinder_sensor_t &pkt);

    // implementation specific helpers
    virtual uint32_t get_osd_flight_mode_bitmask(void)
    {
        return 0;
    };    // custom masks are needed for vendor specific settings
    virtual bool is_scheduler_enabled() = 0;                            // only osd backends should allow a push type telemetry

    // implementation specific MSP out command processing
    virtual MSPCommandResult msp_process_out_api_version(sbuf_t *dst) = 0;
    virtual MSPCommandResult msp_process_out_fc_version(sbuf_t *dst) = 0;
    virtual MSPCommandResult msp_process_out_fc_variant(sbuf_t *dst) = 0;
    virtual MSPCommandResult msp_process_out_uid(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_board_info(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_build_info(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_name(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_status(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_osd_config(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_raw_gps(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_comp_gps(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_attitude(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_altitude(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_analog(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_battery_state(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_esc_sensor_data(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_rtc(sbuf_t *dst);
    virtual MSPCommandResult msp_process_out_rc(sbuf_t *dst);
};

#endif  //HAL_MSP_ENABLED