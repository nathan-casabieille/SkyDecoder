#include "skydecoder/utils.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <regex>

namespace skydecoder {
namespace utils {

std::string to_hex_string(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        ss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return ss.str();
}

std::string to_hex_string(uint32_t value, size_t width) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setfill('0');
    if (width > 0) {
        ss << std::setw(width);
    }
    ss << value;
    return ss.str();
}

std::vector<uint8_t> from_hex_string(const std::string& hex) {
    std::vector<uint8_t> result;
    std::string clean_hex = hex;
    
    // Remove spaces and 0x prefix
    clean_hex.erase(std::remove_if(clean_hex.begin(), clean_hex.end(), ::isspace), clean_hex.end());
    if (clean_hex.substr(0, 2) == "0x" || clean_hex.substr(0, 2) == "0X") {
        clean_hex = clean_hex.substr(2);
    }
    
    // Ensure even length
    if (clean_hex.length() % 2 != 0) {
        clean_hex = "0" + clean_hex;
    }
    
    for (size_t i = 0; i < clean_hex.length(); i += 2) {
        std::string byte_str = clean_hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        result.push_back(byte);
    }
    
    return result;
}

std::string format_value(const FieldValue& value, Unit unit, double lsb) {
    return std::visit([&](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        
        if constexpr (std::is_same_v<T, bool>) {
            return val ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return val;
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return to_hex_string(val);
        } else if constexpr (std::is_arithmetic_v<T>) {
            double numeric_val = static_cast<double>(val) * lsb;
            
            switch (unit) {
                case Unit::SECONDS:
                    return format_time_of_day(val, lsb);
                case Unit::NAUTICAL_MILES:
                    return std::to_string(numeric_val) + " NM";
                case Unit::DEGREES:
                    return std::to_string(numeric_val) + "°";
                case Unit::FLIGHT_LEVEL:
                    return format_flight_level(val, lsb);
                case Unit::FEET:
                    return std::to_string(numeric_val) + " ft";
                case Unit::KNOTS:
                    return std::to_string(numeric_val) + " kts";
                case Unit::METERS_PER_SECOND:
                    return std::to_string(numeric_val) + " m/s";
                default:
                    return std::to_string(numeric_val);
            }
        } else {
            return "unknown";
        }
    }, value);
}

std::string format_time_of_day(uint32_t tod_value, double lsb) {
    double seconds = tod_value * lsb;
    int hours = static_cast<int>(seconds / 3600) % 24;
    int minutes = static_cast<int>((seconds - hours * 3600) / 60);
    double sec = seconds - hours * 3600 - minutes * 60;
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setw(2) << minutes << ":"
       << std::setw(6) << std::fixed << std::setprecision(3) << sec;
    return ss.str();
}

std::string format_coordinates(double latitude, double longitude) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << latitude << "°N, " << longitude << "°E";
    return ss.str();
}

std::string format_flight_level(uint16_t fl_value, double lsb) {
    double fl = fl_value * lsb;
    std::stringstream ss;
    ss << "FL" << std::setfill('0') << std::setw(3) << static_cast<int>(fl);
    return ss.str();
}

bool validate_checksum(const std::vector<uint8_t>& data) {
    // Simple XOR checksum implementation
    uint8_t checksum = 0;
    for (size_t i = 0; i < data.size() - 1; ++i) {
        checksum ^= data[i];
    }
    return checksum == data.back();
}

bool is_valid_mode_a_code(uint16_t code) {
    // Check that Mode A code is valid (digits 0-7 only)
    for (int i = 0; i < 4; ++i) {
        uint8_t digit = (code >> (3 * i)) & 0x07;
        if (digit > 7) {
            return false;
        }
    }
    return true;
}

bool is_valid_callsign(const std::string& callsign) {
    // Check callsign format (letters, numbers, spaces)
    std::regex callsign_regex("^[A-Z0-9 ]{1,8}$");
    return std::regex_match(callsign, callsign_regex);
}

double nautical_miles_to_meters(double nm) {
    return nm * 1852.0;
}

double meters_to_nautical_miles(double meters) {
    return meters / 1852.0;
}

double degrees_to_radians(double degrees) {
    return degrees * M_PI / 180.0;
}

double radians_to_degrees(double radians) {
    return radians * 180.0 / M_PI;
}

double flight_level_to_feet(double fl) {
    return fl * 100.0;
}

double feet_to_flight_level(double feet) {
    return feet / 100.0;
}

uint32_t extract_bits_from_bytes(const std::vector<uint8_t>& data, size_t start_bit, size_t num_bits) {
    uint32_t result = 0;
    
    for (size_t i = 0; i < num_bits; ++i) {
        size_t byte_idx = (start_bit + i) / 8;
        size_t bit_idx = 7 - ((start_bit + i) % 8);
        
        if (byte_idx < data.size() && (data[byte_idx] & (1 << bit_idx))) {
            result |= (1 << (num_bits - 1 - i));
        }
    }
    
    return result;
}

void set_bits_in_bytes(std::vector<uint8_t>& data, size_t start_bit, size_t num_bits, uint32_t value) {
    for (size_t i = 0; i < num_bits; ++i) {
        size_t byte_idx = (start_bit + i) / 8;
        size_t bit_idx = 7 - ((start_bit + i) % 8);
        
        if (byte_idx >= data.size()) {
            data.resize(byte_idx + 1, 0);
        }
        
        if (value & (1 << (num_bits - 1 - i))) {
            data[byte_idx] |= (1 << bit_idx);
        } else {
            data[byte_idx] &= ~(1 << bit_idx);
        }
    }
}

std::string bits_to_string(const std::vector<uint8_t>& data) {
    std::string result;
    for (uint8_t byte : data) {
        for (int i = 7; i >= 0; --i) {
            result += (byte & (1 << i)) ? '1' : '0';
        }
        result += ' ';
    }
    if (!result.empty()) {
        result.pop_back(); // Remove the last space
    }
    return result;
}

MessageStatistics analyze_messages(const std::vector<AsterixMessage>& messages) {
    MessageStatistics stats;
    
    stats.total_messages = messages.size();
    
    for (const auto& message : messages) {
        if (message.valid) {
            stats.valid_messages++;
        } else {
            stats.invalid_messages++;
            stats.errors.push_back(message.error_message);
        }
        
        stats.category_counts[message.category]++;
        
        for (const auto& item : message.data_items) {
            stats.data_item_counts[item.id]++;
        }
    }
    
    return stats;
}

void print_statistics(const MessageStatistics& stats) {
    std::cout << "=== MESSAGE STATISTICS ===" << std::endl;
    std::cout << "Total messages: " << stats.total_messages << std::endl;
    std::cout << "Valid messages: " << stats.valid_messages << std::endl;
    std::cout << "Invalid messages: " << stats.invalid_messages << std::endl;
    
    if (stats.total_messages > 0) {
        double success_rate = (static_cast<double>(stats.valid_messages) / stats.total_messages) * 100.0;
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
    }
    
    std::cout << "\nCategory distribution:" << std::endl;
    for (const auto& pair : stats.category_counts) {
        std::cout << "  CAT " << std::setw(3) << static_cast<int>(pair.first) 
                  << ": " << std::setw(6) << pair.second << " messages" << std::endl;
    }
    
    if (!stats.data_item_counts.empty()) {
        std::cout << "\nTop data items:" << std::endl;
        std::vector<std::pair<std::string, size_t>> sorted_items(
            stats.data_item_counts.begin(), stats.data_item_counts.end());
        
        std::sort(sorted_items.begin(), sorted_items.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(size_t(10), sorted_items.size()); ++i) {
            std::cout << "  " << std::setw(12) << sorted_items[i].first 
                      << ": " << std::setw(6) << sorted_items[i].second << std::endl;
        }
    }
    
    if (!stats.errors.empty()) {
        std::cout << "\nErrors encountered:" << std::endl;
        std::unordered_map<std::string, size_t> error_counts;
        for (const auto& error : stats.errors) {
            error_counts[error]++;
        }
        
        for (const auto& pair : error_counts) {
            std::cout << "  " << pair.first << " (" << pair.second << " times)" << std::endl;
        }
    }
}

std::string to_json(const ParsedField& field) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << field.name << "\",\n";
    ss << "  \"description\": \"" << field.description << "\",\n";
    ss << "  \"valid\": " << (field.valid ? "true" : "false") << ",\n";
    
    if (!field.valid) {
        ss << "  \"error\": \"" << field.error_message << "\",\n";
    }
    
    ss << "  \"value\": ";
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
            ss << (value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
            ss << "\"" << value << "\"";
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            ss << "\"" << to_hex_string(value) << "\"";
        } else {
            ss << value;
        }
    }, field.value);
    
    ss << ",\n";
    ss << "  \"unit\": \"";
    switch (field.unit) {
        case Unit::SECONDS: ss << "seconds"; break;
        case Unit::NAUTICAL_MILES: ss << "NM"; break;
        case Unit::DEGREES: ss << "degrees"; break;
        case Unit::FLIGHT_LEVEL: ss << "FL"; break;
        case Unit::FEET: ss << "feet"; break;
        case Unit::KNOTS: ss << "knots"; break;
        case Unit::METERS_PER_SECOND: ss << "m/s"; break;
        default: ss << "none"; break;
    }
    ss << "\"\n}";
    
    return ss.str();
}

std::string to_json(const ParsedDataItem& item) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << item.id << "\",\n";
    ss << "  \"name\": \"" << item.name << "\",\n";
    ss << "  \"valid\": " << (item.valid ? "true" : "false") << ",\n";
    
    if (!item.valid) {
        ss << "  \"error\": \"" << item.error_message << "\",\n";
    }
    
    ss << "  \"fields\": [\n";
    for (size_t i = 0; i < item.fields.size(); ++i) {
        ss << "    " << to_json(item.fields[i]);
        if (i < item.fields.size() - 1) {
            ss << ",";
        }
        ss << "\n";
    }
    ss << "  ]\n}";
    
    return ss.str();
}

std::string to_json(const AsterixMessage& message) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"category\": " << static_cast<int>(message.category) << ",\n";
    ss << "  \"length\": " << message.length << ",\n";
    ss << "  \"valid\": " << (message.valid ? "true" : "false") << ",\n";
    
    if (!message.valid) {
        ss << "  \"error\": \"" << message.error_message << "\",\n";
    }
    
    ss << "  \"data_items\": [\n";
    for (size_t i = 0; i < message.data_items.size(); ++i) {
        ss << "    " << to_json(message.data_items[i]);
        if (i < message.data_items.size() - 1) {
            ss << ",";
        }
        ss << "\n";
    }
    ss << "  ]\n}";
    
    return ss.str();
}

std::string to_json(const AsterixBlock& block) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"category\": " << static_cast<int>(block.category) << ",\n";
    ss << "  \"length\": " << block.length << ",\n";
    ss << "  \"messages\": [\n";
    
    for (size_t i = 0; i < block.messages.size(); ++i) {
        ss << "    " << to_json(block.messages[i]);
        if (i < block.messages.size() - 1) {
            ss << ",";
        }
        ss << "\n";
    }
    
    ss << "  ]\n}";
    return ss.str();
}

// Performance Profiler Implementation
void PerformanceProfiler::start_timer(const std::string& name) {
    timers_[name].start_time = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::stop_timer(const std::string& name) {
    auto now = std::chrono::high_resolution_clock::now();
    auto& timer = timers_[name];
    
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(
        now - timer.start_time);
    
    timer.total_time += duration;
    timer.call_count++;
}

void PerformanceProfiler::print_results() const {
    std::cout << "\n=== PERFORMANCE PROFILE ===" << std::endl;
    std::cout << std::left << std::setw(20) << "Timer Name" 
              << std::setw(15) << "Total Time (s)" 
              << std::setw(10) << "Calls" 
              << std::setw(15) << "Avg Time (ms)" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    for (const auto& pair : timers_) {
        const auto& name = pair.first;
        const auto& timer = pair.second;
        
        double avg_time_ms = (timer.total_time.count() / timer.call_count) * 1000.0;
        
        std::cout << std::left << std::setw(20) << name
                  << std::setw(15) << std::fixed << std::setprecision(6) << timer.total_time.count()
                  << std::setw(10) << timer.call_count
                  << std::setw(15) << std::fixed << std::setprecision(3) << avg_time_ms << std::endl;
    }
}

void PerformanceProfiler::reset() {
    timers_.clear();
}

// Category Cache Implementation
void CategoryCache::add_category(uint8_t category, std::unique_ptr<AsterixCategory> definition) {
    cache_[category] = std::move(definition);
}

const AsterixCategory* CategoryCache::get_category(uint8_t category) const {
    auto it = cache_.find(category);
    return (it != cache_.end()) ? it->second.get() : nullptr;
}

void CategoryCache::clear() {
    cache_.clear();
}

size_t CategoryCache::size() const {
    return cache_.size();
}

std::vector<uint8_t> CategoryCache::get_cached_categories() const {
    std::vector<uint8_t> categories;
    for (const auto& pair : cache_) {
        categories.push_back(pair.first);
    }
    std::sort(categories.begin(), categories.end());
    return categories;
}

} // namespace utils
} // namespace skydecoder