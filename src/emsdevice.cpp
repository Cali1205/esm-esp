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

#include "emsdevice.h"
#include "emsesp.h"

namespace emsesp {

uuid::log::Logger EMSdevice::logger_{F_(emsesp), uuid::log::Facility::CONSOLE};

std::string EMSdevice::brand_to_string() const {
    switch (brand_) {
    case EMSdevice::Brand::BOSCH:
        return read_flash_string(F("Bosch"));
        break;
    case EMSdevice::Brand::JUNKERS:
        return read_flash_string(F("Junkers"));
        break;
    case EMSdevice::Brand::BUDERUS:
        return read_flash_string(F("Buderus"));
        break;
    case EMSdevice::Brand::NEFIT:
        return read_flash_string(F("Nefit"));
        break;
    case EMSdevice::Brand::SIEGER:
        return read_flash_string(F("Sieger"));
        break;
    case EMSdevice::Brand::WORCESTER:
        return read_flash_string(F("Worcester"));
        break;
    case EMSdevice::Brand::NO_BRAND:
    default:
        return read_flash_string(F("---"));
        break;
    }

    return std::string{};
}

// returns the name of the MQTT topic to use for a specific device
std::string EMSdevice::device_type_2_device_name(const uint8_t device_type) {
    switch (device_type) {
    case DeviceType::SYSTEM:
        return read_flash_string(F("system"));
        break;

    case DeviceType::BOILER:
        return read_flash_string(F("boiler"));
        break;

    case DeviceType::THERMOSTAT:
        return read_flash_string(F("thermostat"));
        break;

    case DeviceType::HEATPUMP:
        return read_flash_string(F("heatpump"));
        break;

    case DeviceType::SOLAR:
        return read_flash_string(F("solar"));
        break;

    case DeviceType::MIXING:
        return read_flash_string(F("mixing"));
        break;

    case DeviceType::SENSOR:
        return read_flash_string(F("sensor"));
        break;

    case DeviceType::CONTROLLER:
        return read_flash_string(F("controller"));
        break;

    case DeviceType::SWITCH:
        return read_flash_string(F("switch"));
        break;

    default:
        return std::string{};
        break;
    }
}

// returns device_type from a string
uint8_t EMSdevice::device_name_2_device_type(const char * topic) {
    if (strcmp(topic, "boiler") == 0) {
        return DeviceType::BOILER;
    }

    if (strcmp(topic, "thermostat") == 0) {
        return DeviceType::THERMOSTAT;
    }

    if (strcmp(topic, "system") == 0) {
        return DeviceType::SYSTEM;
    }

    if (strcmp(topic, "heatpump") == 0) {
        return DeviceType::HEATPUMP;
    }

    if (strcmp(topic, "solar") == 0) {
        return DeviceType::SOLAR;
    }

    if (strcmp(topic, "mixing") == 0) {
        return DeviceType::MIXING;
    }

    if (strcmp(topic, "sensor") == 0) {
        return DeviceType::SENSOR;
    }

    return DeviceType::UNKNOWN;
}

std::string EMSdevice::device_type_name() const {
    switch (device_type_) {
    case DeviceType::BOILER:
        return read_flash_string(F("Boiler"));
        break;

    case DeviceType::THERMOSTAT:
        return read_flash_string(F("Thermostat"));
        break;

    case DeviceType::HEATPUMP:
        return read_flash_string(F("Heat Pump"));
        break;

    case DeviceType::SOLAR:
        return read_flash_string(F("Solar Module"));
        break;

    case DeviceType::CONNECT:
        return read_flash_string(F("Connect Module"));
        break;

    case DeviceType::CONTROLLER:
        return read_flash_string(F("Controller"));
        break;

    case DeviceType::MIXING:
        return read_flash_string(F("Mixing Module"));
        break;

    case DeviceType::SWITCH:
        return read_flash_string(F("Switching Module"));
        break;

    case DeviceType::GATEWAY:
        return read_flash_string(F("Gateway Module"));
        break;

    default:
        return read_flash_string(F("Unknown"));
        break;
    }
}

// 0=unknown, 1=bosch, 2=junkers, 3=buderus, 4=nefit, 5=sieger, 11=worcester
uint8_t EMSdevice::decode_brand(uint8_t value) {
    switch (value) {
    case 1:
        return EMSdevice::Brand::BOSCH;
        break;
    case 2:
        return EMSdevice::Brand::JUNKERS;
        break;
    case 3:
        return EMSdevice::Brand::BUDERUS;
        break;
    case 4:
        return EMSdevice::Brand::NEFIT;
        break;
    case 5:
        return EMSdevice::Brand::SIEGER;
        break;
    case 11:
        return EMSdevice::Brand::WORCESTER;
        break;
    case 0:
    default:
        return EMSdevice::Brand::NO_BRAND;
        break;
    }
}

// returns string of a human friendly description of the EMS device
std::string EMSdevice::to_string() const {
    std::string str(160, '\0');

    // for devices that haven't been lookup yet, don't show all details
    if (product_id_ == 0) {
        snprintf_P(&str[0], str.capacity() + 1, PSTR("%s (DeviceID:0x%02X)"), name_.c_str(), device_id_);
        return str;
    }

    if (brand_ == Brand::NO_BRAND) {
        snprintf_P(&str[0], str.capacity() + 1, PSTR("%s (DeviceID:0x%02X, ProductID:%d, Version:%s)"), name_.c_str(), device_id_, product_id_, version_.c_str());
    } else {
        snprintf_P(&str[0],
                   str.capacity() + 1,
                   PSTR("%s %s (DeviceID:0x%02X ProductID:%d, Version:%s)"),
                   brand_to_string().c_str(),
                   name_.c_str(),
                   device_id_,
                   product_id_,
                   version_.c_str());
    }

    return str;
}

// returns out brand + device name
std::string EMSdevice::to_string_short() const {
    std::string str(160, '\0');
    if (brand_ == Brand::NO_BRAND) {
        snprintf_P(&str[0], str.capacity() + 1, PSTR("%s: %s"), device_type_name().c_str(), name_.c_str());
    } else {
        snprintf_P(&str[0], str.capacity() + 1, PSTR("%s: %s %s"), device_type_name().c_str(), brand_to_string().c_str(), name_.c_str());
    }
    return str;
}

// prints the header for the section
void EMSdevice::show_values(uuid::console::Shell & shell) {
    shell.printfln(F("%s: %s"), device_type_name().c_str(), to_string().c_str());
}

// for each telegram that has the fetch value set (true) do a read request
void EMSdevice::fetch_values() {
    LOG_DEBUG(F("Fetching values for device ID 0x%02X"), device_id());

    for (const auto & tf : telegram_functions_) {
        if (tf.fetch_) {
            read_command(tf.telegram_type_id_);
        }
    }
}

// toggle on/off automatic fetch for a telegram id
void EMSdevice::toggle_fetch(uint16_t telegram_id, bool toggle) {
    LOG_DEBUG(F("Toggling fetch for device ID 0x%02X, telegram ID 0x%02X to %d"), device_id(), telegram_id, toggle);

    for (auto & tf : telegram_functions_) {
        if (tf.telegram_type_id_ == telegram_id) {
            tf.fetch_ = toggle;
        }
    }
}

// get status of automatic fetch for a telegram id
bool EMSdevice::get_toggle_fetch(uint16_t telegram_id) {
    for (auto & tf : telegram_functions_) {
        if (tf.telegram_type_id_ == telegram_id) {
            return tf.fetch_;
        }
    }
    return false;
}


// list all the telegram type IDs for this device
void EMSdevice::show_telegram_handlers(uuid::console::Shell & shell) {
    if (telegram_functions_.size() == 0) {
        return;
    }

    shell.printf(F(" This %s will respond to telegram type IDs: "), device_type_name().c_str());
    for (const auto & tf : telegram_functions_) {
        shell.printf(F("0x%02X "), tf.telegram_type_id_);
    }
    shell.println();
}

// list all the mqtt handlers for this device
void EMSdevice::show_mqtt_handlers(uuid::console::Shell & shell) {
    Mqtt::show_topic_handlers(shell, this->device_type_);
}

void EMSdevice::register_mqtt_topic(const std::string & topic, mqtt_subfunction_p f) {
    Mqtt::subscribe(this->device_type_, topic, f);
}

// add command to library
void EMSdevice::register_mqtt_cmd(const __FlashStringHelper * cmd, cmdfunction_p f) {
    Command::add(this->device_type_, this->device_id_, cmd, f);
}

// register a call back function for a specific telegram type
void EMSdevice::register_telegram_type(const uint16_t telegram_type_id, const __FlashStringHelper * telegram_type_name, bool fetch, process_function_p f) {
    telegram_functions_.emplace_back(telegram_type_id, telegram_type_name, fetch, f);
}

// return the name of the telegram type
std::string EMSdevice::telegram_type_name(std::shared_ptr<const Telegram> telegram) {
    // see if it's one of the common ones, like Version
    if (telegram->type_id == EMS_TYPE_VERSION) {
        return read_flash_string(F("Version"));
    } else if (telegram->type_id == EMS_TYPE_UBADevices) {
        return read_flash_string(F("UBADevices"));
    }

    for (const auto & tf : telegram_functions_) {
        if ((tf.telegram_type_id_ == telegram->type_id) && ((telegram->type_id & 0xF0) != 0xF0)) {
            return uuid::read_flash_string(tf.telegram_type_name_);
        }
    }

    return std::string{};
}

// take a telegram_type_id and call the matching handler
// return true if match found
bool EMSdevice::handle_telegram(std::shared_ptr<const Telegram> telegram) {
    for (const auto & tf : telegram_functions_) {
        if (tf.telegram_type_id_ == telegram->type_id) {
            // if the data block is empty, assume that this telegram is not recognized by the bus master
            // so remove it from the automatic fetch list
            if (telegram->message_length == 0) {
                LOG_DEBUG(F("This telegram (%s) is not recognized by the EMS bus"), uuid::read_flash_string(tf.telegram_type_name_).c_str());
                toggle_fetch(tf.telegram_type_id_, false);
                return false;
            }

            LOG_DEBUG(F("Received %s"), uuid::read_flash_string(tf.telegram_type_name_).c_str());
            tf.process_function_(telegram);
            return true;
        }
    }
    return false; // type not found
}

// send Tx write with a data block
void EMSdevice::write_command(const uint16_t type_id, const uint8_t offset, uint8_t * message_data, const uint8_t message_length, const uint16_t validate_typeid) {
    EMSESP::send_write_request(type_id, this->device_id(), offset, message_data, message_length, validate_typeid);
}

// send Tx write with a single value
void EMSdevice::write_command(const uint16_t type_id, const uint8_t offset, const uint8_t value, const uint16_t validate_typeid) {
    EMSESP::send_write_request(type_id, this->device_id(), offset, value, validate_typeid);
}

// send Tx write with a single value, with no post validation
void EMSdevice::write_command(const uint16_t type_id, const uint8_t offset, const uint8_t value) {
    EMSESP::send_write_request(type_id, this->device_id(), offset, value, 0);
}

// send Tx read command to the device
void EMSdevice::read_command(const uint16_t type_id) {
    EMSESP::send_read_request(type_id, device_id());
}

// prints a string value to the console
void EMSdevice::print_value(uuid::console::Shell & shell, uint8_t padding, const __FlashStringHelper * name, const __FlashStringHelper * value) {
    print_value(shell, padding, name, uuid::read_flash_string(value).c_str());
}

// print string value, value is not in flash
void EMSdevice::print_value(uuid::console::Shell & shell, uint8_t padding, const __FlashStringHelper * name, const char * value) {
    uint8_t i = padding;
    while (i-- > 0) {
        shell.print(F(" "));
    }

    shell.printfln(PSTR("%s: %s"), uuid::read_flash_string(name).c_str(), value);
}

// print value to shell from the json doc
void EMSdevice::print_value_json(uuid::console::Shell &      shell,
                                 const __FlashStringHelper * key,
                                 const __FlashStringHelper * prefix,
                                 const __FlashStringHelper * name,
                                 const __FlashStringHelper * suffix,
                                 JsonObject &                json) {
    JsonVariant data = json[uuid::read_flash_string(key)];
    if (data == nullptr) {
        return; // doesn't exist
    }

    if (prefix != nullptr) {
        shell.printf(PSTR("  %s%s: "), uuid::read_flash_string(prefix).c_str(), uuid::read_flash_string(name).c_str());
    } else {
        shell.printf(PSTR("  %s: "), uuid::read_flash_string(name).c_str());
    }

    if (data.is<char *>()) {
        shell.printf(PSTR("%s"), data.as<char *>());
    } else if (data.is<int>()) {
        shell.printf(PSTR("%d"), data.as<int>());
    } else if (data.is<float>()) {
        char data_str[10];
        shell.printf(PSTR("%s"), Helpers::render_value(data_str, (float)data.as<float>(), 1));
    } else if (data.is<bool>()) {
        char data_str[10];
        shell.printf(PSTR("%s"), Helpers::render_boolean(data_str, data.as<bool>()));
    }

    if (suffix != nullptr) {
        shell.print(' ');
        shell.print(uuid::read_flash_string(suffix).c_str());
    }

    shell.println();
}

// print value to shell from the json doc into a name/value pair
void EMSdevice::print_value_json(JsonArray &                 root,
                                 const __FlashStringHelper * key,
                                 const __FlashStringHelper * prefix,
                                 const __FlashStringHelper * name,
                                 const __FlashStringHelper * suffix,
                                 JsonObject &                json) {
    JsonVariant data = json[uuid::read_flash_string(key)];
    if (data == nullptr) {
        return; // doesn't exist
    }

    JsonObject dataElement = root.createNestedObject();
    // add prefix to name
    if (prefix != nullptr) {
        char name_text[100];
        snprintf_P(name_text, sizeof(name_text), PSTR("%s%s"), uuid::read_flash_string(prefix).c_str(), uuid::read_flash_string(name).c_str());
        dataElement["n"] = name_text;
    } else {
        dataElement["n"] = name;
    }

    // convert to string and add the suffix, this is to save space when sending to the web as json
    // which is why we use n and v instead of name and value
    std::string suffix_string(10, '\0');
    if (suffix == nullptr) {
        suffix_string = "";
    } else {
        suffix_string = " " + uuid::read_flash_string(suffix);
    }

    char data_string[20];
    if (data.is<char *>()) {
        snprintf_P(data_string, sizeof(data_string), PSTR("%s%s"), data.as<char *>(), suffix_string.c_str());
    } else if (data.is<int>()) {
        snprintf_P(data_string, sizeof(data_string), PSTR("%d%s"), data.as<int>(), suffix_string.c_str());
    } else if (data.is<float>()) {
        char s[10];
        snprintf_P(data_string, sizeof(data_string), PSTR("%s%s"), Helpers::render_value(s, (float)data.as<float>(), 1), suffix_string.c_str());
    } else if (data.is<bool>()) {
        char s[10];
        snprintf_P(data_string, sizeof(data_string), PSTR("%s%s"), Helpers::render_boolean(s, data.as<bool>()), suffix_string.c_str());
    }

    dataElement["v"] = data_string;
}

} // namespace emsesp
