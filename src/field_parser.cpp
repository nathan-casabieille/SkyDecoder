#include "skydecoder/field_parser.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <bitset>

namespace skydecoder {

ParsedField FieldParser::parse_field(const Field& field_def, ParseContext& context) {
    ParsedField result;
    result.name = field_def.name;
    result.description = field_def.description;
    result.unit = field_def.unit;
    
    try {
        // Lire les bytes nécessaires pour ce champ
        auto field_bytes = read_field_bytes(field_def, context);
        
        // Extraire la valeur brute selon le nombre de bits
        uint32_t raw_value = extract_bits(field_bytes, 0, field_def.bits);
        
        // Convertir en valeur typée
        result.value = convert_raw_value(raw_value, field_def);
        
        result.valid = true;
    } catch (const std::exception& e) {
        result.valid = false;
        result.error_message = e.what();
    }
    
    return result;
}

ParsedDataItem FieldParser::parse_data_item(const DataItem& item_def, ParseContext& context) {
    ParsedDataItem result;
    result.id = item_def.id;
    result.name = item_def.name;
    
    try {
        size_t start_position = context.position;
        size_t bytes_to_read = 0;
        
        // Déterminer combien de bytes lire selon le format
        switch (item_def.format) {
            case DataFormat::FIXED:
                if (item_def.length.has_value()) {
                    bytes_to_read = item_def.length.value();
                } else {
                    throw std::runtime_error("Fixed format requires length specification");
                }
                break;
                
            case DataFormat::EXPLICIT:
                // Le premier byte indique la longueur
                if (!context.has_data(1)) {
                    throw std::runtime_error("Insufficient data for explicit length");
                }
                bytes_to_read = context.read_uint8();
                break;
                
            case DataFormat::REPETITIVE:
                // Le premier byte indique le nombre de répétitions
                if (!context.has_data(1)) {
                    throw std::runtime_error("Insufficient data for repetitive length");
                }
                {
                    uint8_t rep_count = context.read_uint8();
                    if (item_def.length.has_value()) {
                        bytes_to_read = rep_count * item_def.length.value();
                    } else {
                        throw std::runtime_error("Repetitive format requires length specification");
                    }
                }
                break;
                
            case DataFormat::VARIABLE:
                // Lire byte par byte jusqu'à ce que FX=0
                bytes_to_read = 1; // Au moins un byte
                while (true) {
                    if (!context.has_data(bytes_to_read)) {
                        throw std::runtime_error("Insufficient data for variable length field");
                    }
                    
                    // Vérifier le bit FX (bit 0) du dernier byte lu
                    uint8_t last_byte = context.data[start_position + bytes_to_read - 1];
                    if ((last_byte & 0x01) == 0) {
                        break; // FX=0, arrêter
                    }
                    bytes_to_read++;
                }
                break;
        }
        
        // Créer un contexte temporaire pour les données de cet item
        ParseContext item_context(context.data + start_position, bytes_to_read, context.category);
        
        // Parser tous les champs
        size_t bit_offset = 0;
        for (const auto& field_def : item_def.fields) {
            if (field_def.name == "spare") {
                // Ignorer les champs spare
                bit_offset += field_def.bits;
                continue;
            }
            
            // Créer un contexte pour ce champ spécifique
            ParseContext field_context = item_context;
            field_context.position = bit_offset / 8;
            
            auto parsed_field = parse_field(field_def, field_context);
            result.fields.push_back(parsed_field);
            
            // Vérifier s'il y a des champs d'extension
            if (field_def.condition.has_value() && !field_def.extension_fields.empty()) {
                if (evaluate_condition(field_def.condition.value(), result.fields)) {
                    auto extension_fields = parse_extension_fields(
                        field_def.extension_fields, item_context, result.fields
                    );
                    result.fields.insert(result.fields.end(), 
                                       extension_fields.begin(), extension_fields.end());
                }
            }
            
            bit_offset += field_def.bits;
        }
        
        // Avancer le contexte principal
        context.position = start_position + bytes_to_read;
        result.valid = true;
        
    } catch (const std::exception& e) {
        result.valid = false;
        result.error_message = e.what();
    }
    
    return result;
}

std::vector<ParsedField> FieldParser::parse_extension_fields(
    const std::vector<Field>& extension_fields,
    ParseContext& context,
    const std::vector<ParsedField>& parsed_fields) {
    
    std::vector<ParsedField> result;
    
    for (const auto& field_def : extension_fields) {
        if (field_def.name == "spare") {
            continue; // Ignorer les champs spare
        }
        
        auto parsed_field = parse_field(field_def, context);
        result.push_back(parsed_field);
    }
    
    return result;
}

uint32_t FieldParser::extract_bits(const std::vector<uint8_t>& data, size_t start_bit, size_t num_bits) {
    if (num_bits > 32) {
        throw std::runtime_error("Cannot extract more than 32 bits");
    }
    
    uint32_t result = 0;
    size_t byte_offset = start_bit / 8;
    size_t bit_offset = start_bit % 8;
    
    for (size_t i = 0; i < num_bits; ++i) {
        size_t current_byte = byte_offset + (bit_offset + i) / 8;
        size_t current_bit = 7 - ((bit_offset + i) % 8); // MSB first
        
        if (current_byte >= data.size()) {
            throw std::runtime_error("Bit extraction exceeds data size");
        }
        
        if (data[current_byte] & (1 << current_bit)) {
            result |= (1 << (num_bits - 1 - i));
        }
    }
    
    return result;
}

std::vector<uint8_t> FieldParser::read_field_bytes(const Field& field, ParseContext& context) {
    size_t bytes_needed = (field.bits + 7) / 8; // Arrondir vers le haut
    return context.read_bytes(bytes_needed);
}

FieldValue FieldParser::convert_raw_value(uint32_t raw_value, const Field& field) {
    switch (field.type) {
        // Small unsigned integers (fit in uint8_t)
        case FieldType::UINT8:
        case FieldType::UINT1:
        case FieldType::UINT2:
        case FieldType::UINT3:
        case FieldType::UINT4:
        case FieldType::UINT5:
        case FieldType::UINT6:
        case FieldType::UINT7:
            return static_cast<uint8_t>(raw_value);
            
        // Medium unsigned integers (fit in uint16_t)
        case FieldType::UINT16:
        case FieldType::UINT12:
        case FieldType::UINT14:
            return static_cast<uint16_t>(raw_value);
            
        // Large unsigned integers
        case FieldType::UINT24:
        case FieldType::UINT32:
            return raw_value;
            
        // Signed integers - need to handle two's complement conversion
        case FieldType::INT8:
            {
                // Convert to signed 8-bit
                if (raw_value & 0x80) {
                    return static_cast<int8_t>(raw_value | 0xFFFFFF00);
                } else {
                    return static_cast<int8_t>(raw_value);
                }
            }
            
        case FieldType::INT16:
            {
                // Convert to signed 16-bit
                if (raw_value & 0x8000) {
                    return static_cast<int16_t>(raw_value | 0xFFFF0000);
                } else {
                    return static_cast<int16_t>(raw_value);
                }
            }
            
        case FieldType::INT24:
            {
                // Convert to signed 24-bit (stored in int32_t)
                if (raw_value & 0x800000) {
                    return static_cast<int32_t>(raw_value | 0xFF000000);
                } else {
                    return static_cast<int32_t>(raw_value);
                }
            }
            
        case FieldType::INT32:
            return static_cast<int32_t>(raw_value);
            
        case FieldType::BOOL:
            return raw_value != 0;
            
        case FieldType::STRING:
            if (field.encoding.has_value() && field.encoding.value() == "6bit_ascii") {
                std::vector<uint8_t> bytes;
                size_t num_bytes = (field.bits + 7) / 8;
                for (size_t i = 0; i < num_bytes; ++i) {
                    bytes.push_back((raw_value >> (8 * (num_bytes - 1 - i))) & 0xFF);
                }
                return decode_6bit_ascii(bytes);
            } else {
                return std::to_string(raw_value);
            }
            
        case FieldType::BYTES:
            {
                std::vector<uint8_t> bytes;
                size_t num_bytes = (field.bits + 7) / 8;
                for (size_t i = 0; i < num_bytes; ++i) {
                    bytes.push_back((raw_value >> (8 * (num_bytes - 1 - i))) & 0xFF);
                }
                return bytes;
            }
    }
    
    return raw_value;
}

std::string FieldParser::decode_6bit_ascii(const std::vector<uint8_t>& data) {
    std::string result;
    
    // Table de conversion 6-bit ASCII ICAO
    const char icao_alphabet[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ     0123456789      ";
    
    // Extraire les caractères 6 bits par 6 bits
    size_t total_bits = data.size() * 8;
    for (size_t bit_pos = 0; bit_pos < total_bits; bit_pos += 6) {
        if (bit_pos + 6 > total_bits) break;
        
        uint8_t char_code = 0;
        for (int i = 0; i < 6; ++i) {
            size_t byte_idx = (bit_pos + i) / 8;
            size_t bit_idx = 7 - ((bit_pos + i) % 8);
            
            if (data[byte_idx] & (1 << bit_idx)) {
                char_code |= (1 << (5 - i));
            }
        }
        
        if (char_code < sizeof(icao_alphabet)) {
            char c = icao_alphabet[char_code];
            if (c != ' ' || !result.empty()) { // Éviter les espaces en début
                result += c;
            }
        }
    }
    
    // Supprimer les espaces en fin
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    
    return result;
}

bool FieldParser::evaluate_condition(const std::string& condition, const std::vector<ParsedField>& fields) {
    // Parser simple pour les conditions comme "FX==1"
    if (condition.find("==") != std::string::npos) {
        size_t pos = condition.find("==");
        std::string field_name = condition.substr(0, pos);
        std::string expected_value = condition.substr(pos + 2);
        
        // Nettoyer les espaces
        field_name.erase(std::remove_if(field_name.begin(), field_name.end(), ::isspace), field_name.end());
        expected_value.erase(std::remove_if(expected_value.begin(), expected_value.end(), ::isspace), expected_value.end());
        
        // Chercher le champ
        for (const auto& field : fields) {
            if (field.name == field_name) {
                // Vérifier la valeur
                if (std::holds_alternative<bool>(field.value)) {
                    bool field_val = std::get<bool>(field.value);
                    return (expected_value == "1" && field_val) || (expected_value == "0" && !field_val);
                } else if (std::holds_alternative<uint8_t>(field.value)) {
                    uint8_t field_val = std::get<uint8_t>(field.value);
                    return field_val == std::stoi(expected_value);
                }
            }
        }
    }
    
    return false;
}

double FieldParser::apply_lsb(uint32_t raw_value, double lsb) {
    return raw_value * lsb;
}

} // namespace skydecoder