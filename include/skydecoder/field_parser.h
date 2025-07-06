#pragma once

#include "skydecoder/asterix_types.h"
#include <vector>
#include <bitset>

namespace skydecoder {

class FieldParser {
public:
    // Parse a field from binary data
    static ParsedField parse_field(const Field& field_def, ParseContext& context);
    
    // Parse a complete data item
    static ParsedDataItem parse_data_item(const DataItem& item_def, ParseContext& context);
    
    // Parse conditional extension fields
    static std::vector<ParsedField> parse_extension_fields(
        const std::vector<Field>& extension_fields,
        ParseContext& context,
        const std::vector<ParsedField>& parsed_fields
    );
    
private:
    // Utility methods for extracting bits
    static uint32_t extract_bits(const std::vector<uint8_t>& data, size_t start_bit, size_t num_bits);
    static std::vector<uint8_t> read_field_bytes(const Field& field, ParseContext& context);
    
    // Convert raw values to typed values
    static FieldValue convert_raw_value(uint32_t raw_value, const Field& field);
    static std::string decode_6bit_ascii(const std::vector<uint8_t>& data);
    
    // Condition validation
    static bool evaluate_condition(const std::string& condition, const std::vector<ParsedField>& fields);
    
    // Apply scaling factors (LSB)
    static double apply_lsb(uint32_t raw_value, double lsb);
};

} // namespace skydecoder