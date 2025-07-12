#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <stdexcept>

namespace skydecoder {

// Basic types for fields
enum class FieldType {
    // Unsigned integers
    UINT8,
    UINT16,
    UINT24,
    UINT32,
    UINT1,
    UINT2,
    UINT3,
    UINT4,
    UINT5,
    UINT6,
    UINT7,
    UINT12,
    UINT14,
    
    // Signed integers
    INT8,
    INT16,
    INT24,
    INT32,
    
    // Other types
    BOOL,
    STRING,
    BYTES
};

// Data item format
enum class DataFormat {
    FIXED,
    VARIABLE,
    EXPLICIT,
    REPETITIVE
};

// Units of measurement
enum class Unit {
    NONE,
    SECONDS,
    NAUTICAL_MILES,
    DEGREES,
    FLIGHT_LEVEL,
    FEET,
    KNOTS,
    METERS_PER_SECOND
};

// Field value
using FieldValue = std::variant<
    uint8_t,
    uint16_t,
    uint32_t,
    int8_t,
    int16_t,
    int32_t,
    bool,
    std::string,
    std::vector<uint8_t>
>;


// Structure for enumerations
struct EnumValue {
    uint32_t value;
    std::string description;
};

// Field structure
struct Field {
    std::string name;
    FieldType type;
    uint8_t bits;
    std::string description;
    double lsb = 1.0;  // Least Significant Bit
    Unit unit = Unit::NONE;
    std::vector<EnumValue> enums;
    std::optional<std::string> encoding;
    
    // For conditional extensions
    std::optional<std::string> condition;
    std::vector<Field> extension_fields;
};

// Data item structure
struct DataItem {
    std::string id;
    std::string name;
    std::string definition;
    DataFormat format;
    std::optional<uint16_t> length;  // For fixed formats
    std::vector<Field> fields;
};

// User Application Profile (UAP)
struct UserApplicationProfile {
    std::vector<std::string> items;
};

// Category header
struct CategoryHeader {
    uint8_t category;
    std::string name;
    std::string description;
    std::string version;
    std::string date;
};

// Parsing rule
struct ParsingRule {
    std::string name;
    std::string description;
    std::string condition;
    std::string action;
};

// Validation rule
struct ValidationRule {
    std::string field;
    std::string type;  // mandatory, conditional, optional
    std::optional<std::string> condition;
};

// Complete ASTERIX category
struct AsterixCategory {
    CategoryHeader header;
    UserApplicationProfile uap;
    std::unordered_map<std::string, DataItem> data_items;
    std::vector<ParsingRule> parsing_rules;
    std::vector<ValidationRule> validation_rules;
};

// Field parsing result
struct ParsedField {
    std::string name;
    FieldValue value;
    std::string description;
    Unit unit;
    bool valid = true;
    std::string error_message;
};

// Data item parsing result
struct ParsedDataItem {
    std::string id;
    std::string name;
    std::vector<ParsedField> fields;
    bool valid = true;
    std::string error_message;
};

// Parsed ASTERIX message
struct AsterixMessage {
    uint8_t category;
    uint16_t length;
    std::vector<ParsedDataItem> data_items;
    bool valid = true;
    std::string error_message;
};

// ASTERIX data block
struct AsterixBlock {
    uint8_t category;
    uint16_t length;
    bool valid;
    std::vector<AsterixMessage> messages;
};

// Structure for parsing context
struct ParseContext {
    const uint8_t* data;
    size_t size;
    size_t position;
    const AsterixCategory* category;
    
    ParseContext(const uint8_t* d, size_t s, const AsterixCategory* c) 
        : data(d), size(s), position(0), category(c) {}
    
    bool has_data(size_t bytes) const {
        return position + bytes <= size;
    }
    
    uint8_t read_uint8() {
        if (!has_data(1)) throw std::runtime_error("Insufficient data");
        return data[position++];
    }
    
    uint16_t read_uint16() {
        if (!has_data(2)) throw std::runtime_error("Insufficient data");
        uint16_t value = (data[position] << 8) | data[position + 1];
        position += 2;
        return value;
    }
    
    uint32_t read_uint24() {
        if (!has_data(3)) throw std::runtime_error("Insufficient data");
        uint32_t value = (data[position] << 16) | (data[position + 1] << 8) | data[position + 2];
        position += 3;
        return value;
    }
    
    std::vector<uint8_t> read_bytes(size_t count) {
        if (!has_data(count)) throw std::runtime_error("Insufficient data");
        std::vector<uint8_t> result(data + position, data + position + count);
        position += count;
        return result;
    }
    
    void skip(size_t bytes) {
        if (!has_data(bytes)) throw std::runtime_error("Insufficient data");
        position += bytes;
    }
};

} // namespace skydecoder