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
        
        // Decode all messages in the block
        while (context.position < block.length && context.has_data(1)) {
            auto message = decode_message_internal(context);
            block.messages.push_back(std::move(message));
        }
        
    } catch (const std::exception& e) {
        log_error("Failed to decode block: " + std::string(e.what()));
        // The block can be partially valid
    }
    
    return block;
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
    
    // Read the FSPEC byte by byte until the FX bit is 0
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