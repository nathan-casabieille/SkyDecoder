#pragma once

#include "skydecoder/asterix_types.h"
#include "skydecoder/xml_parser.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace skydecoder {

class AsterixDecoder {
public:
    AsterixDecoder();
    ~AsterixDecoder();
    
    // Load a category definition from an XML file
    bool load_category_definition(const std::string& xml_file);
    
    // Load a category definition from an XML string
    bool load_category_definition_from_string(const std::string& xml_content);
    
    // Load all definitions from a directory
    bool load_categories_from_directory(const std::string& directory);
    
    // Decode a complete ASTERIX block
    AsterixBlock decode_block(const std::vector<uint8_t>& data);
    
    // Decode an individual ASTERIX message
    AsterixMessage decode_message(uint8_t category, const std::vector<uint8_t>& data);
    
    // Decode from a binary file
    std::vector<AsterixBlock> decode_file(const std::string& filename);
    
    // Message validation
    bool validate_message(const AsterixMessage& message);
    
    // Utilities
    std::vector<uint8_t> get_supported_categories() const;
    const AsterixCategory* get_category_definition(uint8_t category) const;
    
    // Configuration
    void set_strict_validation(bool strict) { strict_validation_ = strict; }
    void set_debug_mode(bool debug) { debug_mode_ = debug; }
    
private:
    // Private methods
    AsterixMessage decode_message_internal(ParseContext& context);
    std::vector<std::string> parse_uap_items(const std::vector<uint8_t>& fspec, 
                                            const UserApplicationProfile& uap);
    std::vector<uint8_t> parse_field_specification(ParseContext& context);
    
    // Validation
    bool validate_mandatory_fields(const AsterixMessage& message, const AsterixCategory& category);
    bool validate_conditional_fields(const AsterixMessage& message, const AsterixCategory& category);
    
    // Error handling
    void log_error(const std::string& message);
    void log_warning(const std::string& message);
    void log_debug(const std::string& message);
    
    // Member data
    std::unordered_map<uint8_t, std::unique_ptr<AsterixCategory>> categories_;
    std::unique_ptr<XmlParser> xml_parser_;
    
    // Configuration
    bool strict_validation_ = false;
    bool debug_mode_ = false;
};

} // namespace skydecoder