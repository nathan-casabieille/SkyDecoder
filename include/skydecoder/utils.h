#pragma once

#include "asterix_types.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace skydecoder {
namespace utils {

// Conversion and formatting
std::string to_hex_string(const std::vector<uint8_t>& data);
std::string to_hex_string(uint32_t value, size_t width = 0);
std::vector<uint8_t> from_hex_string(const std::string& hex);

// Value formatting according to unit
std::string format_value(const FieldValue& value, Unit unit, double lsb = 1.0);
std::string format_time_of_day(uint32_t tod_value, double lsb);
std::string format_coordinates(double latitude, double longitude);
std::string format_flight_level(uint16_t fl_value, double lsb);

// Data validation
bool validate_checksum(const std::vector<uint8_t>& data);
bool is_valid_mode_a_code(uint16_t code);
bool is_valid_callsign(const std::string& callsign);

// Unit conversion
double nautical_miles_to_meters(double nm);
double meters_to_nautical_miles(double meters);
double degrees_to_radians(double degrees);
double radians_to_degrees(double radians);
double flight_level_to_feet(double fl);
double feet_to_flight_level(double feet);

// Bit utilities
uint32_t extract_bits_from_bytes(const std::vector<uint8_t>& data, size_t start_bit, size_t num_bits);
void set_bits_in_bytes(std::vector<uint8_t>& data, size_t start_bit, size_t num_bits, uint32_t value);
std::string bits_to_string(const std::vector<uint8_t>& data);

// Statistical analysis
struct MessageStatistics {
    size_t total_messages = 0;
    size_t valid_messages = 0;
    size_t invalid_messages = 0;
    std::unordered_map<uint8_t, size_t> category_counts;
    std::unordered_map<std::string, size_t> data_item_counts;
    std::vector<std::string> errors;
};

MessageStatistics analyze_messages(const std::vector<AsterixMessage>& messages);
void print_statistics(const MessageStatistics& stats);

// JSON serialization
std::string to_json(const AsterixMessage& message);
std::string to_json(const AsterixBlock& block);
std::string to_json(const ParsedField& field);
std::string to_json(const ParsedDataItem& item);

// Performance profiling
class PerformanceProfiler {
public:
    void start_timer(const std::string& name);
    void stop_timer(const std::string& name);
    void print_results() const;
    void reset();

private:
    struct TimerData {
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::duration<double> total_time{0};
        size_t call_count = 0;
    };
    
    std::unordered_map<std::string, TimerData> timers_;
};

// Cache for category definitions
class CategoryCache {
public:
    void add_category(uint8_t category, std::unique_ptr<AsterixCategory> definition);
    const AsterixCategory* get_category(uint8_t category) const;
    void clear();
    size_t size() const;
    std::vector<uint8_t> get_cached_categories() const;

private:
    std::unordered_map<uint8_t, std::unique_ptr<AsterixCategory>> cache_;
};

} // namespace utils
} // namespace skydecoder