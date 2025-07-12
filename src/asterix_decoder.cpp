#include "skydecoder/asterix_decoder.h"
#include "skydecoder/field_parser.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace skydecoder {

AsterixDecoder::AsterixDecoder() 
    : xml_parser_(std::make_unique<XmlParser>()) {
}

AsterixDecoder::~AsterixDecoder() = default;

bool AsterixDecoder::load_category_definition(const std::string& xml_file) {
    try {
        auto category = xml_parser_->parse_category(xml_file);
        uint8_t cat_num = category->header.category;
        categories_[cat_num] = std::move(category);
        
        log_debug("Loaded category " + std::to_string(cat_num) + " from " + xml_file);
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to load category from " + xml_file + ": " + e.what());
        return false;
    }
}

bool AsterixDecoder::load_category_definition_from_string(const std::string& xml_content) {
    try {
        auto category = xml_parser_->parse_category_from_string(xml_content);
        uint8_t cat_num = category->header.category;
        categories_[cat_num] = std::move(category);
        
        log_debug("Loaded category " + std::to_string(cat_num) + " from string");
        return true;
    } catch (const std::exception& e) {
        log_error(std::string("Failed to load category from string: ") + e.what());
        return false;
    }
}

bool AsterixDecoder::load_categories_from_directory(const std::string& directory) {
    try {
        int loaded_count = 0;
        
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".xml") {
                if (load_category_definition(entry.path().string())) {
                    loaded_count++;
                }
            }
        }
        
        log_debug("Loaded " + std::to_string(loaded_count) + " categories from " + directory);
        return loaded_count > 0;
    } catch (const std::exception& e) {
        log_error("Failed to load categories from directory " + directory + ": " + e.what());
        return false;
    }
}

AsterixBlock AsterixDecoder::decode_block(const std::vector<uint8_t>& data) {
    AsterixBlock block;
    
    if (data.size() < 3) {
        block.valid = false;
        log_error("Block too small: " + std::to_string(data.size()) + " bytes");
        return block;
    }
    
    ParseContext context(data.data(), data.size(), nullptr);
    
    try {
        // Read the block header
        block.category = context.read_uint8();
        block.length = context.read_uint16();
        
        log_debug("Decoding block: category=" + std::to_string(block.category) + 
                 ", length=" + std::to_string(block.length));
        
        // Check that the category is supported
        auto cat_it = categories_.find(block.category);
        if (cat_it == categories_.end()) {
            throw std::runtime_error("Unsupported category: " + std::to_string(block.category));
        }
        
        context.category = cat_it->second.get();
        
        // Handle according to category type
        if (block.category == 2) {
            // CAT002: multi-record structure
            decode_multirecord_block(context, block);
        } else {
            // Other categories: traditional structure
            decode_traditional_block(context, block);
        }
        
        block.valid = true;
        
    } catch (const std::exception& e) {
        log_error("Failed to decode block: " + std::string(e.what()));
        block.valid = false;
    }
    
    return block;
}

void AsterixDecoder::decode_multirecord_block(ParseContext& context, AsterixBlock& block) {
    log_debug("Decoding multi-record block for CAT002");
    
    size_t block_end = block.length;
    size_t record_count = 0;
    
    // Decode each record in the block
    while (context.position < block_end) {
        record_count++;
        log_debug("Decoding record #" + std::to_string(record_count) + 
                 " at position " + std::to_string(context.position));
        
        try {
            // Each record has its own FSPEC + data structure
            auto record = decode_single_record(context);
            block.messages.push_back(std::move(record));
            
        } catch (const std::exception& e) {
            log_error("Failed to decode record #" + std::to_string(record_count) + 
                     ": " + std::string(e.what()));
            
            // In strict mode, stop decoding
            if (strict_validation_) {
                break;
            }
            
            // Otherwise, try to continue (advance by one byte)
            if (context.position < block_end) {
                context.position++;
            }
        }
        
        // Avoid infinite loops
        if (record_count > 1000) {
            log_warning("Maximum record count reached, stopping decode");
            break;
        }
    }
    
    log_debug("Decoded " + std::to_string(block.messages.size()) + 
             " records from multi-record block");
}

AsterixMessage AsterixDecoder::decode_single_record(ParseContext& context) {
    AsterixMessage record;
    record.category = context.category->header.category;
    
    size_t record_start = context.position;
    
    // Read the record's FSPEC
    auto fspec = parse_field_specification(context);
    
    if (debug_mode_) {
        std::string fspec_hex;
        for (uint8_t byte : fspec) {
            char buf[4];
            sprintf(buf, "%02X ", byte);
            fspec_hex += buf;
        }
        log_debug("Record FSPEC: " + fspec_hex);
    }
    
    // Identify the data items present in this record
    auto present_items = parse_uap_items(fspec, context.category->uap);
    
    log_debug("Record has " + std::to_string(present_items.size()) + " data items");
    
    // Calculate the expected record length
    size_t expected_length = calculate_record_length(present_items, *context.category);
    size_t available_data = context.size - context.position;
    
    if (expected_length > available_data) {
        log_warning("Expected record length (" + std::to_string(expected_length) + 
                   ") exceeds available data (" + std::to_string(available_data) + ")");
    }
    
    // Decode each present data item
    for (const auto& item_id : present_items) {
        if (item_id == "spare" || item_id.empty()) {
            continue; // Skip spare and empty fields
        }
        
        auto item_it = context.category->data_items.find(item_id);
        if (item_it == context.category->data_items.end()) {
            log_warning("Unknown data item: " + item_id);
            continue;
        }
        
        size_t item_start = context.position;
        auto parsed_item = FieldParser::parse_data_item(item_it->second, context);
        size_t item_length = context.position - item_start;
        
        log_debug("Parsed " + item_id + " (" + std::to_string(item_length) + " bytes)");
        record.data_items.push_back(std::move(parsed_item));
    }
    
    record.length = context.position - record_start;
    record.valid = true;
    
    log_debug("Record decoded successfully: " + std::to_string(record.length) + " bytes total");
    
    return record;
}

void AsterixDecoder::decode_traditional_block(ParseContext& context, AsterixBlock& block) {
    log_debug("Decoding traditional block");
    
    // Decode the single message in the block
    while (context.position < block.length && context.has_data(1)) {
        auto message = decode_message_internal(context);
        block.messages.push_back(std::move(message));
        
        // For traditional blocks, usually one message only
        if (!message.valid) {
            break;
        }
    }
}

size_t AsterixDecoder::calculate_record_length(
    const std::vector<std::string>& present_items, 
    const AsterixCategory& category) {
    
    size_t total_length = 0;
    
    for (const auto& item_id : present_items) {
        if (item_id == "spare" || item_id.empty()) {
            continue;
        }
        
        auto item_it = category.data_items.find(item_id);
        if (item_it != category.data_items.end()) {
            const auto& item = item_it->second;
            
            switch (item.format) {
                case DataFormat::FIXED:
                    if (item.length.has_value()) {
                        total_length += item.length.value();
                    }
                    break;
                    
                case DataFormat::VARIABLE:
                    // For variable fields, estimate at least 1 byte
                    total_length += 1;
                    break;
                    
                case DataFormat::EXPLICIT:
                    // First byte indicates length, so at least 2 bytes
                    total_length += 2;
                    break;
                    
                case DataFormat::REPETITIVE:
                    // First byte indicates repetitions, so at least 2 bytes
                    total_length += 2;
                    break;
            }
        }
    }
    
    return total_length;
}

RecordStatistics AsterixDecoder::analyze_block_records(const AsterixBlock& block) {
    RecordStatistics stats;
    
    stats.total_records = block.messages.size();
    
    for (const auto& record : block.messages) {
        if (record.valid) {
            stats.valid_records++;
        } else {
            stats.invalid_records++;
        }
        
        stats.record_lengths.push_back(record.length);
        
        // Count data item frequency
        for (const auto& item : record.data_items) {
            stats.item_frequency[item.id]++;
        }
    }
    
    return stats;
}

void AsterixDecoder::print_record_statistics(const RecordStatistics& stats) {
    std::cout << "\n=== RECORD STATISTICS ===" << std::endl;
    std::cout << "Total records: " << stats.total_records << std::endl;
    std::cout << "Valid records: " << stats.valid_records << std::endl;
    std::cout << "Invalid records: " << stats.invalid_records << std::endl;
    
    if (stats.total_records > 0) {
        double success_rate = (static_cast<double>(stats.valid_records) / stats.total_records) * 100.0;
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
    }
    
    // Length statistics
    if (!stats.record_lengths.empty()) {
        size_t min_length = *std::min_element(stats.record_lengths.begin(), stats.record_lengths.end());
        size_t max_length = *std::max_element(stats.record_lengths.begin(), stats.record_lengths.end());
        double avg_length = std::accumulate(stats.record_lengths.begin(), stats.record_lengths.end(), 0.0) / stats.record_lengths.size();
        
        std::cout << "\nRecord lengths:" << std::endl;
        std::cout << "  Min: " << min_length << " bytes" << std::endl;
        std::cout << "  Max: " << max_length << " bytes" << std::endl;
        std::cout << "  Avg: " << std::fixed << std::setprecision(1) << avg_length << " bytes" << std::endl;
    }
    
    // Data item frequency
    if (!stats.item_frequency.empty()) {
        std::cout << "\nData item frequency:" << std::endl;
        
        // Sort by descending frequency
        std::vector<std::pair<std::string, size_t>> sorted_items(
            stats.item_frequency.begin(), stats.item_frequency.end());
        
        std::sort(sorted_items.begin(), sorted_items.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (const auto& pair : sorted_items) {
            double percentage = (static_cast<double>(pair.second) / stats.total_records) * 100.0;
            std::cout << "  " << std::setw(12) << pair.first 
                      << ": " << std::setw(4) << pair.second 
                      << " (" << std::fixed << std::setprecision(1) << percentage << "%)" << std::endl;
        }
    }
}

bool AsterixDecoder::validate_multirecord_block(const AsterixBlock& block) {
    if (block.category != 2) {
        return true; // Validation only applicable to CAT002
    }
    
    bool is_valid = true;
    
    // Check that all records are valid
    for (size_t i = 0; i < block.messages.size(); ++i) {
        const auto& record = block.messages[i];
        
        if (!record.valid) {
            log_error("Record #" + std::to_string(i + 1) + " is invalid: " + record.error_message);
            is_valid = false;
            continue;
        }
        
        // Check mandatory items for CAT002
        bool has_data_source = false;
        bool has_message_type = false;
        
        for (const auto& item : record.data_items) {
            if (item.id == "I002/010") has_data_source = true;
            if (item.id == "I002/000") has_message_type = true;
        }
        
        if (!has_data_source) {
            log_warning("Record #" + std::to_string(i + 1) + " missing mandatory Data Source Identifier (I002/010)");
            if (strict_validation_) is_valid = false;
        }
        
        if (!has_message_type) {
            log_warning("Record #" + std::to_string(i + 1) + " missing mandatory Message Type (I002/000)");
            if (strict_validation_) is_valid = false;
        }
    }
    
    // Check length consistency
    size_t calculated_length = 3; // Block header
    for (const auto& record : block.messages) {
        calculated_length += record.length;
    }
    
    if (calculated_length != block.length) {
        log_warning("Block length mismatch: declared=" + std::to_string(block.length) + 
                   ", calculated=" + std::to_string(calculated_length));
        if (strict_validation_) is_valid = false;
    }
    
    return is_valid;
}

AsterixMessage AsterixDecoder::decode_message(uint8_t category, const std::vector<uint8_t>& data) {
    AsterixMessage message;
    message.category = category;
    
    auto cat_it = categories_.find(category);
    if (cat_it == categories_.end()) {
        message.valid = false;
        message.error_message = "Unsupported category: " + std::to_string(category);
        return message;
    }
    
    ParseContext context(data.data(), data.size(), cat_it->second.get());
    
    try {
        message = decode_message_internal(context);
    } catch (const std::exception& e) {
        message.valid = false;
        message.error_message = e.what();
    }
    
    return message;
}

std::vector<AsterixBlock> AsterixDecoder::decode_file(const std::string& filename) {
    std::vector<AsterixBlock> blocks;
    
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        log_error("Cannot open file: " + filename);
        return blocks;
    }
    
    // Read the entire file
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    file.close();
    
    log_debug("Read " + std::to_string(data.size()) + " bytes from " + filename);
    
    // Decode block by block
    size_t offset = 0;
    while (offset < data.size()) {
        if (offset + 3 > data.size()) {
            log_warning("Insufficient data for block header at offset " + std::to_string(offset));
            break;
        }
        
        // Read the block length
        uint16_t block_length = (data[offset + 1] << 8) | data[offset + 2];
        
        if (offset + block_length > data.size()) {
            log_warning("Block length exceeds file size at offset " + std::to_string(offset));
            break;
        }
        
        // Extract block data
        std::vector<uint8_t> block_data(data.begin() + offset, 
                                       data.begin() + offset + block_length);
        
        // Decode the block
        auto block = decode_block(block_data);
        blocks.push_back(std::move(block));
        
        offset += block_length;
    }
    
    log_debug("Decoded " + std::to_string(blocks.size()) + " blocks from " + filename);
    return blocks;
}

bool AsterixDecoder::validate_message(const AsterixMessage& message) {
    auto cat_it = categories_.find(message.category);
    if (cat_it == categories_.end()) {
        return false;
    }
    
    const auto& category = *cat_it->second;
    
    // Validate mandatory fields
    if (!validate_mandatory_fields(message, category)) {
        return false;
    }
    
    // Validate conditional fields
    if (!validate_conditional_fields(message, category)) {
        return false;
    }
    
    return message.valid;
}

std::vector<uint8_t> AsterixDecoder::get_supported_categories() const {
    std::vector<uint8_t> categories;
    for (const auto& pair : categories_) {
        categories.push_back(pair.first);
    }
    std::sort(categories.begin(), categories.end());
    return categories;
}

const AsterixCategory* AsterixDecoder::get_category_definition(uint8_t category) const {
    auto it = categories_.find(category);
    return (it != categories_.end()) ? it->second.get() : nullptr;
}

AsterixMessage AsterixDecoder::decode_message_internal(ParseContext& context) {
    AsterixMessage message;
    message.category = context.category->header.category;
    
    try {
        // Read the Field Specification (FSPEC)
        auto fspec = parse_field_specification(context);
        
        // Identify which data items are present
        auto present_items = parse_uap_items(fspec, context.category->uap);
        
        log_debug("Message has " + std::to_string(present_items.size()) + " data items");
        
        // Decode each present data item
        for (const auto& item_id : present_items) {
            if (item_id == "spare") {
                continue; // Skip spare fields
            }
            
            auto item_it = context.category->data_items.find(item_id);
            if (item_it == context.category->data_items.end()) {
                log_warning("Unknown data item: " + item_id);
                continue;
            }
            
            auto parsed_item = FieldParser::parse_data_item(item_it->second, context);
            message.data_items.push_back(std::move(parsed_item));
        }
        
        message.valid = true;
        
    } catch (const std::exception& e) {
        message.valid = false;
        message.error_message = e.what();
        log_error("Failed to decode message: " + std::string(e.what()));
    }
    
    return message;
}

std::vector<uint8_t> AsterixDecoder::parse_field_specification(ParseContext& context) {
    std::vector<uint8_t> fspec;
    
    do {
        if (!context.has_data(1)) {
            throw std::runtime_error("Insufficient data for FSPEC");
        }
        
        uint8_t fspec_byte = context.read_uint8();
        fspec.push_back(fspec_byte);
        
        // If the FX bit (bit 0) is 0, this is the last byte
        if ((fspec_byte & 0x01) == 0) {
            break;
        }
        
    } while (fspec.size() < 16); // Safety limit
    
    return fspec;
}

std::vector<std::string> AsterixDecoder::parse_uap_items(
    const std::vector<uint8_t>& fspec, 
    const UserApplicationProfile& uap) {
    
    std::vector<std::string> present_items;
    
    size_t bit_index = 0;
    for (size_t byte_idx = 0; byte_idx < fspec.size(); ++byte_idx) {
        uint8_t fspec_byte = fspec[byte_idx];
        
        // Examine each bit (except the FX bit for extension bytes)
        int bits_to_check = (byte_idx == fspec.size() - 1) ? 8 : 7; // The last byte uses all bits
        
        for (int bit = 7; bit >= 8 - bits_to_check; --bit) {
            if (bit_index >= uap.items.size()) {
                break; // No more items in the UAP
            }
            
            if (fspec_byte & (1 << bit)) {
                present_items.push_back(uap.items[bit_index]);
            }
            
            bit_index++;
        }
    }
    
    return present_items;
}

bool AsterixDecoder::validate_mandatory_fields(const AsterixMessage& message, const AsterixCategory& category) {
    for (const auto& rule : category.validation_rules) {
        if (rule.type == "mandatory") {
            // Check that the field is present
            bool found = false;
            for (const auto& item : message.data_items) {
                if (item.id == rule.field) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                if (strict_validation_) {
                    return false;
                } else {
                    log_warning("Missing mandatory field: " + rule.field);
                }
            }
        }
    }
    
    return true;
}

bool AsterixDecoder::validate_conditional_fields(const AsterixMessage& message, const AsterixCategory& category) {
    for (const auto& rule : category.validation_rules) {
        if (rule.type == "conditional" && rule.condition.has_value()) {
            // TODO: Implement conditional field validation
            // This would require a more sophisticated expression evaluator
        }
    }
    
    return true;
}

void AsterixDecoder::log_error(const std::string& message) {
    if (debug_mode_) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
}

void AsterixDecoder::log_warning(const std::string& message) {
    if (debug_mode_) {
        std::cerr << "[WARNING] " << message << std::endl;
    }
}

void AsterixDecoder::log_debug(const std::string& message) {
    if (debug_mode_) {
        std::cout << "[DEBUG] " << message << std::endl;
    }
}

} // namespace skydecoder