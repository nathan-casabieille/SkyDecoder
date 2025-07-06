#pragma once

#include "skydecoder/asterix_types.h"
#include <string>
#include <memory>

// Forward declaration to avoid including tinyxml2
namespace tinyxml2 {
    class XMLDocument;
    class XMLElement;
}

namespace skydecoder {

class XmlParser {
public:
    XmlParser();
    ~XmlParser();
    
    // Parse an ASTERIX category from an XML file
    std::unique_ptr<AsterixCategory> parse_category(const std::string& xml_file);
    
    // Parse an ASTERIX category from an XML string
    std::unique_ptr<AsterixCategory> parse_category_from_string(const std::string& xml_content);

private:
    // Private methods to parse different sections
    std::unique_ptr<AsterixCategory> parse_category_from_root(const tinyxml2::XMLElement* root);
    CategoryHeader parse_header(const tinyxml2::XMLElement* header_elem);
    UserApplicationProfile parse_uap(const tinyxml2::XMLElement* uap_elem);
    DataItem parse_data_item(const tinyxml2::XMLElement* item_elem);
    Field parse_field(const tinyxml2::XMLElement* field_elem);
    std::vector<EnumValue> parse_enums(const tinyxml2::XMLElement* field_elem);
    std::vector<ParsingRule> parse_parsing_rules(const tinyxml2::XMLElement* rules_elem);
    std::vector<ValidationRule> parse_validation_rules(const tinyxml2::XMLElement* rules_elem);
    
    // Utilities
    FieldType string_to_field_type(const std::string& type_str);
    DataFormat string_to_data_format(const std::string& format_str);
    Unit string_to_unit(const std::string& unit_str);
    
    std::unique_ptr<tinyxml2::XMLDocument> doc_;
};

} // namespace skydecoder