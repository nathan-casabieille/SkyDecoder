#include <skydecoder/asterix_decoder.h>
#include <skydecoder/utils.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <bitset>

using namespace skydecoder;

/**
 * @brief Test program to verify ASTERIX CAT002 message decoding
 * 
 * Test message: 02 00 16 F0 00 10 01 00 12 34 56 78 9A BC
 * This program decodes the message and compares results with expected values
 * from a proven software decoder.
 */
class AsterixCAT002TestValidator {
private:
    AsterixDecoder decoder_;
    
    // Reference values from proven software decoder
    struct ReferenceRecord {
        struct DataItem {
            std::string id;
            std::vector<std::pair<std::string, std::string>> fields;
        };
        std::vector<DataItem> items;
        size_t expected_length;
    };
    
    std::vector<ReferenceRecord> reference_records_;
    
public:
    AsterixCAT002TestValidator() {
        decoder_.set_debug_mode(true);
        setup_reference_data();
    }
    
    /**
     * @brief Setup reference data from proven decoder output
     */
    void setup_reference_data() {
        // Record 1: Length 8 bytes
        ReferenceRecord record1;
        record1.expected_length = 8;
        
        // I002/010 - Data Source Identifier
        ReferenceRecord::DataItem item010;
        item010.id = "I002/010";
        item010.fields.push_back({"SAC", "0x00"});
        item010.fields.push_back({"SIC", "0x10"});
        record1.items.push_back(item010);
        
        // I002/000 - Message Type
        ReferenceRecord::DataItem item000;
        item000.id = "I002/000";
        item000.fields.push_back({"Message Type", "0x01"});
        record1.items.push_back(item000);
        
        // I002/020 - Sector Number
        ReferenceRecord::DataItem item020;
        item020.id = "I002/020";
        item020.fields.push_back({"SECTOR", "0x00"}); // raw=0, deg=0
        record1.items.push_back(item020);
        
        // I002/030 - Time of Day
        ReferenceRecord::DataItem item030;
        item030.id = "I002/030";
        item030.fields.push_back({"ToD", "0x123456"}); // raw=1193046, seconds=9320.67
        record1.items.push_back(item030);
        
        reference_records_.push_back(record1);
        
        // Record 2: Length 8 bytes
        ReferenceRecord record2;
        record2.expected_length = 8;
        
        // I002/000 - Message Type
        ReferenceRecord::DataItem item000_2;
        item000_2.id = "I002/000";
        item000_2.fields.push_back({"Message Type", "0x9A"});
        record2.items.push_back(item000_2);
        
        // I002/020 - Sector Number
        ReferenceRecord::DataItem item020_2;
        item020_2.id = "I002/020";
        item020_2.fields.push_back({"SECTOR", "0xBC"}); // raw=188, deg=264.375
        record2.items.push_back(item020_2);
        
        // I002/030 - Time of Day
        ReferenceRecord::DataItem item030_2;
        item030_2.id = "I002/030";
        item030_2.fields.push_back({"ToD", "0x000000"}); // raw=0, seconds=0
        record2.items.push_back(item030_2);
        
        // I002/041 - Antenna Rotation Period
        ReferenceRecord::DataItem item041_2;
        item041_2.id = "I002/041";
        item041_2.fields.push_back({"ARP", "0x0000"}); // raw=0, seconds=0
        record2.items.push_back(item041_2);
        
        reference_records_.push_back(record2);
        
        // Records 3, 4, 5: Empty records (1 byte each)
        for (int i = 0; i < 3; i++) {
            ReferenceRecord empty_record;
            empty_record.expected_length = 1;
            reference_records_.push_back(empty_record);
        }
    }
    
    /**
     * @brief Initialize decoder and load CAT002 definition
     */
    bool initialize() {
        std::cout << "=== ASTERIX CAT002 TEST VALIDATOR ===" << std::endl;
        std::cout << "Loading CAT002 definition..." << std::endl;
        
        if (!decoder_.load_category_definition("../data/asterix_categories/cat02.xml")) {
            std::cout << "ERROR: Cannot load CAT002 definition" << std::endl;
            std::cout << "   Trying alternative paths..." << std::endl;
            
            // Try alternative locations
            if (!decoder_.load_category_definition("data/asterix_categories/cat02.xml") &&
                !decoder_.load_category_definition("cat02.xml")) {
                std::cout << "CAT002 definition not found" << std::endl;
                return false;
            }
        }
        
        std::cout << "CAT002 definition loaded successfully" << std::endl;
        return true;
    }
    
    /**
     * @brief Run the complete test
     */
    void run_test() {
        std::cout << "\n=== TEST MESSAGE DECODING ===" << std::endl;
        
        // Test message: 02 00 16 F0 00 10 01 00 12 34 56 78 9A BC
        std::vector<uint8_t> test_message = {
            0x02,              // Category 002
            0x00, 0x16,        // Length = 22 bytes
            0xF0,              // FSPEC Record 1 = 11110000b (I002/010, I002/000, I002/020, I002/030)
            0x00, 0x10,        // I002/010 - Data Source: SAC=0x00, SIC=0x10
            0x01,              // I002/000 - Message Type = 0x01
            0x00,              // I002/020 - Sector Number = 0x00
            0x12, 0x34, 0x56,  // I002/030 - Time of Day = 0x123456
            0x78,              // FSPEC Record 2 = 01111000b (I002/000, I002/020, I002/030, I002/041)
            0x9A,              // I002/000 - Message Type = 0x9A
            0xBC,              // I002/020 - Sector Number = 0xBC
            0x00, 0x00, 0x00,  // I002/030 - Time of Day = 0x000000
            0x00, 0x00,        // I002/041 - Antenna Rotation Period = 0x0000
            0x00,              // FSPEC Record 3 = 00000000b (empty)
            0x00,              // FSPEC Record 4 = 00000000b (empty)
            0x00               // FSPEC Record 5 = 00000000b (empty)
        };
        
        display_raw_message(test_message);
        
        // Decode the block
        auto block = decoder_.decode_block(test_message);
        
        if (!block.valid || block.messages.empty()) {
            std::cout << "Block decoding failed" << std::endl;
            return;
        }
        
        std::cout << "\nBlock decoded successfully" << std::endl;
        std::cout << "Number of records: " << block.messages.size() << std::endl;
        
        // Validate results against reference
        validate_against_reference(block);
        
        // Display detailed results
        display_detailed_results(block);
    }
    
private:
    /**
     * @brief Display raw message in hexadecimal
     */
    void display_raw_message(const std::vector<uint8_t>& message) {
        std::cout << "\nTest message: ";
        for (size_t i = 0; i < message.size(); ++i) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) 
                      << static_cast<int>(message[i]);
            if (i < message.size() - 1) std::cout << " ";
        }
        std::cout << std::dec << std::endl;
        std::cout << "Total size: " << message.size() << " bytes" << std::endl;
        
        // Show expected structure
        std::cout << "\nExpected structure:" << std::endl;
        std::cout << "  Block header: 02 00 16 (CAT=2, LEN=22)" << std::endl;
        std::cout << "  Record 1 (8 bytes): F0 00 10 01 00 12 34 56" << std::endl;
        std::cout << "  Record 2 (8 bytes): 78 9A BC 00 00 00 00 00" << std::endl;
        std::cout << "  Records 3-5 (3 bytes): 00 00 00" << std::endl;
    }
    
    /**
     * @brief Validate decoded results against reference data
     */
    void validate_against_reference(const AsterixBlock& block) {
        std::cout << "\n=== VALIDATION AGAINST REFERENCE ===" << std::endl;
        
        bool all_valid = true;
        
        // Check block-level properties
        if (block.category != 2) {
            std::cout << "Block category mismatch: got " << static_cast<int>(block.category) 
                      << ", expected 2" << std::endl;
            all_valid = false;
        } else {
            std::cout << "Block category: " << static_cast<int>(block.category) << std::endl;
        }
        
        if (block.length != 22) {
            std::cout << "Block length mismatch: got " << block.length 
                      << ", expected 22" << std::endl;
            all_valid = false;
        } else {
            std::cout << "Block length: " << block.length << std::endl;
        }
        
        size_t expected_records = reference_records_.size();
        if (block.messages.size() != expected_records) {
            std::cout << "Record count mismatch: got " << block.messages.size() 
                      << ", expected " << expected_records << std::endl;
            all_valid = false;
        } else {
            std::cout << "Record count: " << block.messages.size() << std::endl;
        }
        
        // Validate each record
        size_t min_records = std::min(block.messages.size(), reference_records_.size());
        for (size_t i = 0; i < min_records; ++i) {
            std::cout << "\n--- Record #" << (i + 1) << " Validation ---" << std::endl;
            if (!validate_record(block.messages[i], reference_records_[i], i + 1)) {
                all_valid = false;
            }
        }
        
        // Final result
        std::cout << "\n" << std::string(50, '=') << std::endl;
        if (all_valid) {
            std::cout << "ALL VALIDATIONS PASSED!" << std::endl;
            std::cout << "Decoder output matches reference data" << std::endl;
        } else {
            std::cout << "SOME VALIDATIONS FAILED" << std::endl;
            std::cout << "Check decoder implementation" << std::endl;
        }
        std::cout << std::string(50, '=') << std::endl;
    }
    
    /**
     * @brief Validate individual record against reference
     */
    bool validate_record(const AsterixMessage& record, 
                        const ReferenceRecord& reference, 
                        size_t  /* record_num */) {
        bool record_valid = true;
        
        if (!record.valid) {
            std::cout << "Record invalid: " << record.error_message << std::endl;
            return false;
        }
        
        // Check record length
        if (record.length != reference.expected_length) {
            std::cout << "Record length mismatch: got " << record.length 
                      << ", expected " << reference.expected_length << std::endl;
            record_valid = false;
        } else if (reference.expected_length > 1) {
            std::cout << "Record length: " << record.length << " bytes" << std::endl;
        }
        
        // For empty records, just check length
        if (reference.items.empty()) {
            if (record.data_items.empty()) {
                std::cout << "Empty record (as expected)" << std::endl;
            } else {
                std::cout << "Expected empty record but found " 
                          << record.data_items.size() << " items" << std::endl;
                record_valid = false;
            }
            return record_valid;
        }
        
        // Check data items count
        if (record.data_items.size() != reference.items.size()) {
            std::cout << "Data items count mismatch: got " << record.data_items.size() 
                      << ", expected " << reference.items.size() << std::endl;
            record_valid = false;
        }
        
        // Validate each data item
        size_t min_items = std::min(record.data_items.size(), reference.items.size());
        for (size_t i = 0; i < min_items; ++i) {
            if (!validate_data_item(record.data_items[i], reference.items[i])) {
                record_valid = false;
            }
        }
        
        return record_valid;
    }
    
    /**
     * @brief Validate data item against reference
     */
    bool validate_data_item(const ParsedDataItem& item, 
                           const ReferenceRecord::DataItem& reference) {
        std::cout << "Validating " << item.id << ": ";
        
        if (!item.valid) {
            std::cout << "INVALID - " << item.error_message << std::endl;
            return false;
        }
        
        if (item.id != reference.id) {
            std::cout << "ID mismatch: got " << item.id 
                      << ", expected " << reference.id << std::endl;
            return false;
        }
        
        bool item_valid = true;
        
        // Validate specific data items
        if (item.id == "I002/010") {
            item_valid = validate_data_source(item, reference);
        } else if (item.id == "I002/000") {
            item_valid = validate_message_type(item, reference);
        } else if (item.id == "I002/020") {
            item_valid = validate_sector_number(item, reference);
        } else if (item.id == "I002/030") {
            item_valid = validate_time_of_day(item, reference);
        } else if (item.id == "I002/041") {
            item_valid = validate_antenna_rotation_period(item, reference);
        } else {
            std::cout << "ℹ️  Item type not specifically validated" << std::endl;
        }
        
        return item_valid;
    }
    
    /**
     * @brief Validate I002/010 - Data Source Identifier
     */
    bool validate_data_source(const ParsedDataItem& item, 
                             const ReferenceRecord::DataItem& reference) {
        if (item.fields.size() < 2) {
            std::cout << "Insufficient fields (expected: 2, got: " 
                      << item.fields.size() << ")" << std::endl;
            return false;
        }
        
        bool valid = true;
        
        // Check SAC
        auto sac_value = std::get<uint8_t>(item.fields[0].value);
        std::string expected_sac = reference.fields[0].second; // "0x00"
        uint8_t expected_sac_val = std::stoi(expected_sac, nullptr, 16);
        
        if (sac_value != expected_sac_val) {
            std::cout << "SAC mismatch: got 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(sac_value) << ", expected " << expected_sac << std::dec << std::endl;
            valid = false;
        }
        
        // Check SIC
        auto sic_value = std::get<uint8_t>(item.fields[1].value);
        std::string expected_sic = reference.fields[1].second; // "0x10"
        uint8_t expected_sic_val = std::stoi(expected_sic, nullptr, 16);
        
        if (sic_value != expected_sic_val) {
            std::cout << "SIC mismatch: got 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(sic_value) << ", expected " << expected_sic << std::dec << std::endl;
            valid = false;
        }
        
        if (valid) {
            std::cout << "SAC=0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(sac_value) << ", SIC=0x" 
                      << static_cast<int>(sic_value) << std::dec << std::endl;
        }
        
        return valid;
    }
    
    /**
     * @brief Validate I002/000 - Message Type
     */
    bool validate_message_type(const ParsedDataItem& item, 
                              const ReferenceRecord::DataItem& reference) {
        if (item.fields.empty()) {
            std::cout << "No fields found" << std::endl;
            return false;
        }
        
        auto msg_type = std::get<uint8_t>(item.fields[0].value);
        std::string expected_type = reference.fields[0].second;
        uint8_t expected_type_val = std::stoi(expected_type, nullptr, 16);
        
        if (msg_type != expected_type_val) {
            std::cout << "Type mismatch: got 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(msg_type) << ", expected " << expected_type << std::dec << std::endl;
            return false;
        }
        
        std::cout << "Message Type=0x" << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(msg_type) << std::dec;
        
        // Interpret the type
        switch (msg_type) {
            case 1: std::cout << " (North Marker)"; break;
            case 2: std::cout << " (Sector Crossing)"; break;
            case 3: std::cout << " (South Marker)"; break;
            case 0x9A: std::cout << " (Application Dependent)"; break;
            default: std::cout << " (Unknown Type)"; break;
        }
        std::cout << std::endl;
        
        return true;
    }
    
    /**
     * @brief Validate I002/020 - Sector Number
     */
    bool validate_sector_number(const ParsedDataItem& item, 
                               const ReferenceRecord::DataItem& reference) {
        if (item.fields.empty()) {
            std::cout << "No fields found" << std::endl;
            return false;
        }
        
        auto sector = std::get<uint8_t>(item.fields[0].value);
        std::string expected_sector = reference.fields[0].second;
        uint8_t expected_sector_val = std::stoi(expected_sector, nullptr, 16);
        
        if (sector != expected_sector_val) {
            std::cout << "Sector mismatch: got 0x" << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(sector) << ", expected " << expected_sector << std::dec << std::endl;
            return false;
        }
        
        double azimuth = sector * (360.0/256.0);
        
        std::cout << "Sector=0x" << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(sector) << std::dec 
                  << " (azimuth=" << std::fixed << std::setprecision(3) << azimuth << "°)" << std::endl;
        
        return true;
    }
    
    /**
     * @brief Validate I002/030 - Time of Day
     */
    bool validate_time_of_day(const ParsedDataItem& item, 
                             const ReferenceRecord::DataItem& reference) {
        if (item.fields.empty()) {
            std::cout << "No fields found" << std::endl;
            return false;
        }
        
        // Handle different possible value types
        uint32_t tod_raw = 0;
        if (std::holds_alternative<uint32_t>(item.fields[0].value)) {
            tod_raw = std::get<uint32_t>(item.fields[0].value);
        } else if (std::holds_alternative<uint16_t>(item.fields[0].value)) {
            tod_raw = std::get<uint16_t>(item.fields[0].value);
        } else if (std::holds_alternative<uint8_t>(item.fields[0].value)) {
            tod_raw = std::get<uint8_t>(item.fields[0].value);
        } else {
            std::cout << "Unsupported data type for Time of Day" << std::endl;
            return false;
        }
        
        std::string expected_tod = reference.fields[0].second;
        uint32_t expected_tod_val = std::stoi(expected_tod, nullptr, 16);
        
        if (tod_raw != expected_tod_val) {
            std::cout << "ToD mismatch: got 0x" << std::hex << std::setfill('0') << std::setw(6)
                      << tod_raw << ", expected " << expected_tod << std::dec << std::endl;
            return false;
        }
        
        // Calculate seconds (LSB = 1/128)
        double tod_seconds = tod_raw * (1.0/128.0);
        
        std::cout << "ToD=0x" << std::hex << std::setfill('0') << std::setw(6)
                  << tod_raw << std::dec << " (" << tod_raw << "), "
                  << std::fixed << std::setprecision(2) << tod_seconds << "s";
        
        // Convert to time format
        if (tod_seconds > 0) {
            int hours = static_cast<int>(tod_seconds / 3600) % 24;
            int minutes = static_cast<int>((tod_seconds - hours * 3600) / 60);
            double sec = tod_seconds - hours * 3600 - minutes * 60;
            std::cout << " (" << std::setfill('0') << std::setw(2) << hours 
                      << ":" << std::setw(2) << minutes 
                      << ":" << std::setw(6) << std::fixed << std::setprecision(3) << sec << ")";
        }
        std::cout << std::endl;
        
        return true;
    }
    
    /**
     * @brief Validate I002/041 - Antenna Rotation Period
     */
    bool validate_antenna_rotation_period(const ParsedDataItem& item, 
                                         const ReferenceRecord::DataItem& reference) {
        if (item.fields.empty()) {
            std::cout << "No fields found" << std::endl;
            return false;
        }
        
        auto arp_raw = std::get<uint16_t>(item.fields[0].value);
        std::string expected_arp = reference.fields[0].second;
        uint16_t expected_arp_val = std::stoi(expected_arp, nullptr, 16);
        
        if (arp_raw != expected_arp_val) {
            std::cout << "ARP mismatch: got 0x" << std::hex << std::setfill('0') << std::setw(4)
                      << arp_raw << ", expected " << expected_arp << std::dec << std::endl;
            return false;
        }
        
        double arp_seconds = arp_raw * (1.0/128.0);
        
        std::cout << "ARP=0x" << std::hex << std::setfill('0') << std::setw(4)
                  << arp_raw << std::dec << " (" << arp_raw << "), "
                  << std::fixed << std::setprecision(2) << arp_seconds << "s" << std::endl;
        
        return true;
    }
    
    /**
     * @brief Display detailed results
     */
    void display_detailed_results(const AsterixBlock& block) {
        std::cout << "\n=== DETAILED DECODING RESULTS ===" << std::endl;
        
        for (size_t i = 0; i < block.messages.size(); ++i) {
            const auto& record = block.messages[i];
            
            std::cout << "\n--- Record #" << (i + 1) << " (Length: " 
                      << record.length << " bytes) ---" << std::endl;
            
            if (record.data_items.empty()) {
                std::cout << "Empty record" << std::endl;
                continue;
            }
            
            for (const auto& item : record.data_items) {
                std::cout << "[" << item.id << "] " << item.name << std::endl;
                
                for (const auto& field : item.fields) {
                    std::cout << "  • " << field.name << ": ";
                    
                    // Display value with proper formatting
                    std::visit([&](const auto& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, uint8_t>) {
                            std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2)
                                      << static_cast<int>(value) << std::dec 
                                      << " (" << static_cast<int>(value) << ")";
                        } else if constexpr (std::is_same_v<T, uint16_t>) {
                            std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4)
                                      << static_cast<int>(value) << std::dec 
                                      << " (" << static_cast<int>(value) << ")";
                        } else if constexpr (std::is_same_v<T, uint32_t>) {
                            std::cout << "0x" << std::hex << std::setfill('0') << std::setw(8)
                                      << static_cast<int>(value) << std::dec 
                                      << " (" << static_cast<int>(value) << ")";
                        } else if constexpr (std::is_same_v<T, bool>) {
                            std::cout << (value ? "true" : "false");
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            std::cout << "\"" << value << "\"";
                        } else {
                            std::cout << "unknown_type";
                        }
                    }, field.value);
                    
                    if (!field.description.empty()) {
                        std::cout << " - " << field.description;
                    }
                    std::cout << std::endl;
                }
            }
        }
        
        // Display block statistics
        auto stats = decoder_.analyze_block_records(block);
        decoder_.print_record_statistics(stats);
    }
};

int main() {
    AsterixCAT002TestValidator validator;
    
    if (!validator.initialize()) {
        std::cerr << "Initialization failed" << std::endl;
        return 1;
    }
    
    validator.run_test();
    
    std::cout << "\n=== TEST COMPLETED ===" << std::endl;
    return 0;
}