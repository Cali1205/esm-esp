/*
 * EMS-ESP - https://github.com/proddy/EMS-ESP
 * Copyright 2019  Paul Derbyshire
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// code originally written by nomis - https://github.com/nomis

#include "sensor.h"
#include "emsesp.h"

#ifdef ESP32
#define YIELD
#else
#define YIELD yield()
#endif

namespace emsesp {

uuid::log::Logger Sensor::logger_{F_(sensor), uuid::log::Facility::DAEMON};

// start the 1-wire
void Sensor::start() {
    reload();

#ifndef EMSESP_STANDALONE
    if (dallas_gpio_) {
        bus_.begin(dallas_gpio_);
    }
#endif

    // API call
    Command::add_with_json(EMSdevice::DeviceType::SENSOR, F("info"), [&](const char * value, const int8_t id, JsonObject & object) {
        return command_info(value, id, object);
    });
}

// load the MQTT settings
void Sensor::reload() {
    EMSESP::emsespSettingsService.read([&](EMSESPSettings & settings) {
        dallas_gpio_ = settings.dallas_gpio;
        parasite_    = settings.dallas_parasite;
    });

    if (Mqtt::mqtt_format() == Mqtt::Format::HA) {
        for (uint8_t i = 0; i < MAX_SENSORS; registered_ha_[i++] = false)
            ;
    }
}

void Sensor::loop() {
#ifndef EMSESP_STANDALONE
    uint32_t time_now = uuid::get_uptime();

    if (state_ == State::IDLE) {
        if (time_now - last_activity_ >= READ_INTERVAL_MS) {
            // LOG_DEBUG(F("Read sensor temperature")); // uncomment for debug
            if (bus_.reset() || parasite_) {
                YIELD;
                bus_.skip();
                bus_.write(CMD_CONVERT_TEMP, parasite_ ? 1 : 0);
                state_ = State::READING;
            } else {
                // no sensors found
                // LOG_ERROR(F("Bus reset failed")); // uncomment for debug
                devices_.clear(); // remove all know devices in case we have a disconnect
            }
            last_activity_ = time_now;
        }
    } else if (state_ == State::READING) {
        if (temperature_convert_complete() && (time_now - last_activity_ > CONVERSION_MS)) {
            // LOG_DEBUG(F("Scanning for sensors")); // uncomment for debug
            bus_.reset_search();
            found_.clear();
            state_ = State::SCANNING;
        } else if (time_now - last_activity_ > READ_TIMEOUT_MS) {
            LOG_ERROR(F("Sensor read timeout"));
            state_ = State::IDLE;
        }
    } else if (state_ == State::SCANNING) {
        if (time_now - last_activity_ > SCAN_TIMEOUT_MS) {
            LOG_ERROR(F("Sensor scan timeout"));
            state_ = State::IDLE;
        } else {
            uint8_t addr[ADDR_LEN] = {0};

            if (bus_.search(addr)) {
                if (!parasite_) {
                    bus_.depower();
                }
                if (bus_.crc8(addr, ADDR_LEN - 1) == addr[ADDR_LEN - 1]) {
                    switch (addr[0]) {
                    case TYPE_DS18B20:
                    case TYPE_DS18S20:
                    case TYPE_DS1822:
                    case TYPE_DS1825:
                        float f;
                        f = get_temperature_c(addr);
                        if ((f != NAN) && (f >= -55) && (f <= 125)) {
                            found_.emplace_back(addr);
                            found_.back().temperature_c = f;
                        }

                        /*
                        // comment out for debugging
                        char result[10];
                        LOG_DEBUG(F("Temp of %s = %s"),
                                  found_.back().to_string().c_str(),
                                  Helpers::render_value(result, found_.back().temperature_c_, 2)); 
                        */
                        break;

                    default:
                        LOG_ERROR(F("Unknown sensor %s"), Device(addr).to_string().c_str());
                        break;
                    }
                } else {
                    LOG_ERROR(F("Invalid sensor %s"), Device(addr).to_string().c_str());
                }
            } else {
                if (!parasite_) {
                    bus_.depower();
                }
                if ((found_.size() >= devices_.size()) || (retrycnt_ > 5)) {
                    if (found_.size() == devices_.size()) {
                        for (uint8_t i = 0; i < devices_.size(); i++) {
                            if (found_[i].temperature_c != devices_[i].temperature_c) {
                                changed_ = true;
                            }
                        }
                    } else {
                        changed_ = true;
                    }
                    devices_  = std::move(found_);
                    retrycnt_ = 0;
                } else {
                    retrycnt_++;
                }
                found_.clear();
                // LOG_DEBUG(F("Found %zu sensor(s). Adding them."), devices_.size()); // uncomment for debug
                state_ = State::IDLE;
            }
        }
    }
#endif
}

bool Sensor::temperature_convert_complete() {
#ifndef EMSESP_STANDALONE
    if (parasite_) {
        return true; // don't care, use the minimum time in loop
    }
    return bus_.read_bit() == 1;
#else
    return true;
#endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

float Sensor::get_temperature_c(const uint8_t addr[]) {
#ifndef EMSESP_STANDALONE
    if (!bus_.reset()) {
        LOG_ERROR(F("Bus reset failed before reading scratchpad from %s"), Device(addr).to_string().c_str());
        return NAN;
    }
    YIELD;
    uint8_t scratchpad[SCRATCHPAD_LEN] = {0};
    bus_.select(addr);
    bus_.write(CMD_READ_SCRATCHPAD);
    bus_.read_bytes(scratchpad, SCRATCHPAD_LEN);
    YIELD;
    if (!bus_.reset()) {
        LOG_ERROR(F("Bus reset failed after reading scratchpad from %s"), Device(addr).to_string().c_str());
        return NAN;
    }
    YIELD;
    if (bus_.crc8(scratchpad, SCRATCHPAD_LEN - 1) != scratchpad[SCRATCHPAD_LEN - 1]) {
        LOG_WARNING(F("Invalid scratchpad CRC: %02X%02X%02X%02X%02X%02X%02X%02X%02X from device %s"),
                    scratchpad[0],
                    scratchpad[1],
                    scratchpad[2],
                    scratchpad[3],
                    scratchpad[4],
                    scratchpad[5],
                    scratchpad[6],
                    scratchpad[7],
                    scratchpad[8],
                    Device(addr).to_string().c_str());
        return NAN;
    }

    int16_t raw_value = ((int16_t)scratchpad[SCRATCHPAD_TEMP_MSB] << 8) | scratchpad[SCRATCHPAD_TEMP_LSB];

    if (addr[0] == TYPE_DS18S20) {
        raw_value = (raw_value << 3) + 12 - scratchpad[SCRATCHPAD_CNT_REM];
    } else {
        // Adjust based on device resolution
        int resolution = 9 + ((scratchpad[SCRATCHPAD_CONFIG] >> 5) & 0x3);
        switch (resolution) {
        case 9:
            raw_value &= ~0x7;
            break;
        case 10:
            raw_value &= ~0x3;
            break;
        case 11:
            raw_value &= ~0x1;
            break;
        case 12:
            break;
        }
    }
    uint32_t raw = ((uint32_t)raw_value * 625 + 500) / 1000; // round to 0.1
    return (float)raw / 10;
#else
    return NAN;
#endif
}

#pragma GCC diagnostic pop

const std::vector<Sensor::Device> Sensor::devices() const {
    return devices_;
}

Sensor::Device::Device(const uint8_t addr[])
    : id_(((uint64_t)addr[0] << 56) | ((uint64_t)addr[1] << 48) | ((uint64_t)addr[2] << 40) | ((uint64_t)addr[3] << 32) | ((uint64_t)addr[4] << 24)
          | ((uint64_t)addr[5] << 16) | ((uint64_t)addr[6] << 8) | (uint64_t)addr[7]) {
}

uint64_t Sensor::Device::id() const {
    return id_;
}

std::string Sensor::Device::to_string() const {
    std::string str(20, '\0');
    snprintf_P(&str[0],
               str.capacity() + 1,
               PSTR("%02X-%04X-%04X-%04X-%02X"),
               (unsigned int)(id_ >> 56) & 0xFF,
               (unsigned int)(id_ >> 40) & 0xFFFF,
               (unsigned int)(id_ >> 24) & 0xFFFF,
               (unsigned int)(id_ >> 8) & 0xFFFF,
               (unsigned int)(id_)&0xFF);
    return str;
}

// check to see if values have been updated
bool Sensor::updated_values() {
    if (changed_) {
        changed_ = false;
        return true;
    }
    return false;
}

bool Sensor::command_info(const char * value, const int8_t id, JsonObject & output) {
    return (export_values(output));
}

// creates JSON doc from values
// returns false if empty
// e.g. sensor_data = {"sensor1":{"id":"28-EA41-9497-0E03-5F","temp":"23.30"},"sensor2":{"id":"28-233D-9497-0C03-8B","temp":"24.0"}}
bool Sensor::export_values(JsonObject & output) {
    if (devices_.size() == 0) {
        return false;
    }
    uint8_t i = 1; // sensor count
    for (const auto & device : devices_) {
        char s[7];
        char sensorID[20]; // sensor{1-n}
        strlcpy(sensorID, "sensor", 20);
        strlcat(sensorID, Helpers::itoa(s, i), 20);
        JsonObject dataSensor = output.createNestedObject(sensorID);
        dataSensor["id"]      = device.to_string();
        dataSensor["temp"]    = Helpers::render_value(s, device.temperature_c, 1);
        i++;
    }

    return true;
}

// send all dallas sensor values as a JSON package to MQTT
// assumes there are devices
void Sensor::publish_values() {
    uint8_t num_devices = devices_.size();

    if (num_devices == 0) {
        return;
    }

    uint8_t mqtt_format_ = Mqtt::mqtt_format();

    // single mode as e.g. ems-esp/sensor_28-EA41-9497-0E03-5F = {"temp":20.2}
    if (mqtt_format_ == Mqtt::Format::SINGLE) {
        StaticJsonDocument<100> doc;
        for (const auto & device : devices_) {
            char topic[60];
            strlcpy(topic, "sensor_", 50);
            strlcat(topic, device.to_string().c_str(), 60);
            char s[7]; // to support -55.00 to 125.00
            doc["temp"] = Helpers::render_value(s, device.temperature_c, 1);
            Mqtt::publish(topic, doc.as<JsonObject>());
            doc.clear(); // clear json doc so we can reuse the buffer again
        }
        return;
    }

    DynamicJsonDocument doc(100 * num_devices);
    uint8_t             i = 1; // sensor count
    for (const auto & device : devices_) {
        char s[7];

        if ((mqtt_format_ == Mqtt::Format::NESTED) || (mqtt_format_ == Mqtt::Format::HA)) {
            // e.g. sensor_data = {"sensor1":{"id":"28-EA41-9497-0E03-5F","temp":"23.30"},"sensor2":{"id":"28-233D-9497-0C03-8B","temp":"24.0"}}
            char sensorID[20]; // sensor{1-n}
            strlcpy(sensorID, "sensor", 20);
            strlcat(sensorID, Helpers::itoa(s, i), 20);
            JsonObject dataSensor = doc.createNestedObject(sensorID);
            dataSensor["id"]      = device.to_string();
            dataSensor["temp"]    = Helpers::render_value(s, device.temperature_c, 1);
        }

        // special for HA
        if (mqtt_format_ == Mqtt::Format::HA) {
            // create the config if this hasn't already been done
            // to e.g. homeassistant/sensor/ems-esp/dallas_sensor1/config
            if (!(registered_ha_[i])) {
                StaticJsonDocument<EMSESP_MAX_JSON_SIZE_MEDIUM> config;
                config["dev_cla"]      = F("temperature");
                config["stat_t"]       = F("ems-esp/sensor_data");
                config["unit_of_meas"] = F("°C");

                char str[50];
                snprintf_P(str, sizeof(str), PSTR("{{value_json.sensor%d.temp}}"), i);
                config["val_tpl"] = str;

                snprintf_P(str, sizeof(str), PSTR("Dallas sensor%d"), i);
                config["name"] = str;

                snprintf_P(str, sizeof(str), PSTR("dalas_sensor%d"), i);
                config["uniq_id"] = str;

                JsonObject dev = config.createNestedObject("dev");
                JsonArray  ids = dev.createNestedArray("ids");
                ids.add("ems-esp");

                std::string topic(100, '\0');
                snprintf_P(&topic[0], 100, PSTR("homeassistant/sensor/ems-esp/dallas_sensor%d/config"), i);
                Mqtt::publish_retain(topic, config.as<JsonObject>(), true); // publish the config payload with retain flag

                registered_ha_[i] = true;
            }
        }
        i++; // increment sensor count
    }

    Mqtt::publish(F("sensor_data"), doc.as<JsonObject>());
}

} // namespace emsesp