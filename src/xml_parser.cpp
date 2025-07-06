#include "skydecoder/xml_parser.h"
#include <tinyxml2.h>
#include <stdexcept>
#include <iostream>

namespace skydecoder {

XmlParser::XmlParser() : doc_(std::make_unique<tinyxml2::XMLDocument>()) {}

XmlParser::~XmlParser() = default;

std::unique_ptr<AsterixCategory> XmlParser::parse_category(const std::string& xml_file) {
    if (doc_->LoadFile(xml_file.c_str()) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load XML file: " + xml_file);
    }
    
    auto root = doc_->FirstChildElement("asterix_category");
    if (!root) {
        throw std::runtime_error("Invalid XML format: missing asterix_category root element");
    }
    
    return parse_category_from_root(root);
}

std::unique_ptr<AsterixCategory> XmlParser::parse_category_from_string(const std::string& xml_content) {
    if (doc_->Parse(xml_content.c_str()) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to parse XML content");
    }
    
    auto root = doc_->FirstChildElement("asterix_category");
    if (!root) {
        throw std::runtime_error("Invalid XML format: missing asterix_category root element");
    }
    
    return parse_category_from_root(root);
}

std::unique_ptr<AsterixCategory> XmlParser::parse_category_from_root(const tinyxml2::XMLElement* root) {
    auto category = std::make_unique<AsterixCategory>();
    
    // Parse header
    auto header_elem = root->FirstChildElement("header");
    if (header_elem) {
        category->header = parse_header(header_elem);
    }
    
    // Parse UAP
    auto uap_elem = root->FirstChildElement("user_application_profile");
    if (uap_elem) {
        category->uap = parse_uap(uap_elem);
    }
    
    // Parse data items
    auto data_items_elem = root->FirstChildElement("data_items");
    if (data_items_elem) {
        for (auto item_elem = data_items_elem->FirstChildElement("data_item");
             item_elem; item_elem = item_elem->NextSiblingElement("data_item")) {
            
            auto data_item = parse_data_item(item_elem);
            category->data_items[data_item.id] = std::move(data_item);
        }
    }
    
    // Parse parsing rules
    auto parsing_rules_elem = root->FirstChildElement("parsing_rules");
    if (parsing_rules_elem) {
        category->parsing_rules = parse_parsing_rules(parsing_rules_elem);
    }
    
    // Parse validation rules
    auto validation_rules_elem = root->FirstChildElement("validation_rules");
    if (validation_rules_elem) {
        category->validation_rules = parse_validation_rules(validation_rules_elem);
    }
    
    return category;
}

CategoryHeader XmlParser::parse_header(const tinyxml2::XMLElement* header_elem) {
    CategoryHeader header;
    
    auto category_elem = header_elem->FirstChildElement("category");
    if (category_elem && category_elem->GetText()) {
        header.category = std::stoi(category_elem->GetText());
    }
    
    auto name_elem = header_elem->FirstChildElement("name");
    if (name_elem && name_elem->GetText()) {
        header.name = name_elem->GetText();
    }
    
    auto desc_elem = header_elem->FirstChildElement("description");
    if (desc_elem && desc_elem->GetText()) {
        header.description = desc_elem->GetText();
    }
    
    auto version_elem = header_elem->FirstChildElement("version");
    if (version_elem && version_elem->GetText()) {
        header.version = version_elem->GetText();
    }
    
    auto date_elem = header_elem->FirstChildElement("date");
    if (date_elem && date_elem->GetText()) {
        header.date = date_elem->GetText();
    }
    
    return header;
}

UserApplicationProfile XmlParser::parse_uap(const tinyxml2::XMLElement* uap_elem) {
    UserApplicationProfile uap;
    
    auto uap_items_elem = uap_elem->FirstChildElement("uap_items");
    if (uap_items_elem) {
        for (auto item_elem = uap_items_elem->FirstChildElement("item");
             item_elem; item_elem = item_elem->NextSiblingElement("item")) {
            
            if (item_elem->GetText()) {
                uap.items.push_back(item_elem->GetText());
            }
        }
    }
    
    return uap;
}

DataItem XmlParser::parse_data_item(const tinyxml2::XMLElement* item_elem) {
    DataItem item;
    
    // Parse attributes
    auto id_attr = item_elem->Attribute("id");
    if (id_attr) {
        item.id = id_attr;
    }
    
    // Parse elements
    auto name_elem = item_elem->FirstChildElement("name");
    if (name_elem && name_elem->GetText()) {
        item.name = name_elem->GetText();
    }
    
    auto def_elem = item_elem->FirstChildElement("definition");
    if (def_elem && def_elem->GetText()) {
        item.definition = def_elem->GetText();
    }
    
    auto format_elem = item_elem->FirstChildElement("format");
    if (format_elem && format_elem->GetText()) {
        item.format = string_to_data_format(format_elem->GetText());
    }
    
    auto length_elem = item_elem->FirstChildElement("length");
    if (length_elem && length_elem->GetText()) {
        item.length = std::stoi(length_elem->GetText());
    }
    
    // Parse structure
    auto structure_elem = item_elem->FirstChildElement("structure");
    if (structure_elem) {
        for (auto field_elem = structure_elem->FirstChildElement("field");
             field_elem; field_elem = field_elem->NextSiblingElement("field")) {
            
            item.fields.push_back(parse_field(field_elem));
        }
        
        // Parse extensions
        for (auto ext_elem = structure_elem->FirstChildElement("extension");
             ext_elem; ext_elem = ext_elem->NextSiblingElement("extension")) {
            
            // Find the field with FX condition
            for (auto& field : item.fields) {
                if (field.name == "FX" || field.name == "FX2") {
                    auto condition_attr = ext_elem->Attribute("condition");
                    if (condition_attr) {
                        field.condition = condition_attr;
                    }
                    
                    // Parse extension fields
                    for (auto ext_field_elem = ext_elem->FirstChildElement("field");
                         ext_field_elem; ext_field_elem = ext_field_elem->NextSiblingElement("field")) {
                        
                        field.extension_fields.push_back(parse_field(ext_field_elem));
                    }
                    break;
                }
            }
        }
    }
    
    return item;
}

Field XmlParser::parse_field(const tinyxml2::XMLElement* field_elem) {
    Field field;
    
    // Parse attributes
    auto name_attr = field_elem->Attribute("name");
    if (name_attr) {
        field.name = name_attr;
    }
    
    auto type_attr = field_elem->Attribute("type");
    if (type_attr) {
        field.type = string_to_field_type(type_attr);
    }
    
    auto bits_attr = field_elem->Attribute("bits");
    if (bits_attr) {
        field.bits = std::stoi(bits_attr);
    }
    
    auto desc_attr = field_elem->Attribute("description");
    if (desc_attr) {
        field.description = desc_attr;
    }
    
    auto lsb_attr = field_elem->Attribute("lsb");
    if (lsb_attr) {
        // Parse LSB (peut être une fraction comme "1/256")
        std::string lsb_str = lsb_attr;
        if (lsb_str.find('/') != std::string::npos) {
            size_t pos = lsb_str.find('/');
            double numerator = std::stod(lsb_str.substr(0, pos));
            double denominator = std::stod(lsb_str.substr(pos + 1));
            field.lsb = numerator / denominator;
        } else {
            field.lsb = std::stod(lsb_str);
        }
    }
    
    auto unit_attr = field_elem->Attribute("unit");
    if (unit_attr) {
        field.unit = string_to_unit(unit_attr);
    }
    
    auto encoding_attr = field_elem->Attribute("encoding");
    if (encoding_attr) {
        field.encoding = encoding_attr;
    }
    
    // Parse enums
    field.enums = parse_enums(field_elem);
    
    return field;
}

std::vector<EnumValue> XmlParser::parse_enums(const tinyxml2::XMLElement* field_elem) {
    std::vector<EnumValue> enums;
    
    for (auto enum_elem = field_elem->FirstChildElement("enum");
         enum_elem; enum_elem = enum_elem->NextSiblingElement("enum")) {
        
        EnumValue enum_val;
        
        auto value_attr = enum_elem->Attribute("value");
        if (value_attr) {
            enum_val.value = std::stoi(value_attr);
        }
        
        if (enum_elem->GetText()) {
            enum_val.description = enum_elem->GetText();
        }
        
        enums.push_back(enum_val);
    }
    
    return enums;
}

std::vector<ParsingRule> XmlParser::parse_parsing_rules(const tinyxml2::XMLElement* rules_elem) {
    std::vector<ParsingRule> rules;
    
    for (auto rule_elem = rules_elem->FirstChildElement("rule");
         rule_elem; rule_elem = rule_elem->NextSiblingElement("rule")) {
        
        ParsingRule rule;
        
        auto name_attr = rule_elem->Attribute("name");
        if (name_attr) {
            rule.name = name_attr;
        }
        
        auto desc_elem = rule_elem->FirstChildElement("description");
        if (desc_elem && desc_elem->GetText()) {
            rule.description = desc_elem->GetText();
        }
        
        auto condition_elem = rule_elem->FirstChildElement("condition");
        if (condition_elem && condition_elem->GetText()) {
            rule.condition = condition_elem->GetText();
        }
        
        auto action_elem = rule_elem->FirstChildElement("action");
        if (action_elem && action_elem->GetText()) {
            rule.action = action_elem->GetText();
        }
        
        rules.push_back(rule);
    }
    
    return rules;
}

std::vector<ValidationRule> XmlParser::parse_validation_rules(const tinyxml2::XMLElement* rules_elem) {
    std::vector<ValidationRule> rules;
    
    for (auto rule_elem = rules_elem->FirstChildElement("rule");
         rule_elem; rule_elem = rule_elem->NextSiblingElement("rule")) {
        
        ValidationRule rule;
        
        auto field_attr = rule_elem->Attribute("field");
        if (field_attr) {
            rule.field = field_attr;
        }
        
        auto type_attr = rule_elem->Attribute("type");
        if (type_attr) {
            rule.type = type_attr;
        }
        
        auto condition_attr = rule_elem->Attribute("condition");
        if (condition_attr) {
            rule.condition = condition_attr;
        }
        
        rules.push_back(rule);
    }
    
    return rules;
}

FieldType XmlParser::string_to_field_type(const std::string& type_str) {
    if (type_str == "uint8") return FieldType::UINT8;
    if (type_str == "uint16") return FieldType::UINT16;
    if (type_str == "uint24") return FieldType::UINT24;
    if (type_str == "uint32") return FieldType::UINT32;
    if (type_str == "uint1") return FieldType::UINT1;
    if (type_str == "uint3") return FieldType::UINT3;
    if (type_str == "uint12") return FieldType::UINT12;
    if (type_str == "uint14") return FieldType::UINT14;
    if (type_str == "bool") return FieldType::BOOL;
    if (type_str == "string") return FieldType::STRING;
    if (type_str == "bytes") return FieldType::BYTES;
    
    throw std::runtime_error("Unknown field type: " + type_str);
}

DataFormat XmlParser::string_to_data_format(const std::string& format_str) {
    if (format_str == "fixed") return DataFormat::FIXED;
    if (format_str == "variable") return DataFormat::VARIABLE;
    if (format_str == "explicit") return DataFormat::EXPLICIT;
    
    throw std::runtime_error("Unknown data format: " + format_str);
}

Unit XmlParser::string_to_unit(const std::string& unit_str) {
    if (unit_str == "s") return Unit::SECONDS;
    if (unit_str == "NM") return Unit::NAUTICAL_MILES;
    if (unit_str == "degrees") return Unit::DEGREES;
    if (unit_str == "FL") return Unit::FLIGHT_LEVEL;
    if (unit_str == "ft") return Unit::FEET;
    if (unit_str == "kts") return Unit::KNOTS;
    if (unit_str == "m/s") return Unit::METERS_PER_SECOND;
    
    return Unit::NONE;
}

} // namespace skydecoder