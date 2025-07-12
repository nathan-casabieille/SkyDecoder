#include <skydecoder/asterix_decoder.h>
#include <skydecoder/utils.h>
#include <iostream>
#include <fstream>

using namespace skydecoder;

void print_message(const AsterixMessage& message) {
    std::cout << "=== ASTERIX Message (Category " << static_cast<int>(message.category) << ") ===" << std::endl;
    
    if (!message.valid) {
        std::cout << "INVALID MESSAGE: " << message.error_message << std::endl;
        return;
    }
    
    for (const auto& item : message.data_items) {
        std::cout << "\n[" << item.id << "] " << item.name << std::endl;
        
        if (!item.valid) {
            std::cout << "  ERROR: " << item.error_message << std::endl;
            continue;
        }
        
        for (const auto& field : item.fields) {
            std::cout << "  " << field.name << ": ";
            
            if (!field.valid) {
                std::cout << "ERROR - " << field.error_message << std::endl;
                continue;
            }
            
            // Format value according to type
            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, bool>) {
                    std::cout << (value ? "true" : "false");
                } else if constexpr (std::is_same_v<T, std::string>) {
                    std::cout << "\"" << value << "\"";
                } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    std::cout << utils::to_hex_string(value);
                } else {
                    std::cout << utils::format_value(field.value, field.unit);
                }
            }, field.value);
            
            if (!field.description.empty()) {
                std::cout << " (" << field.description << ")";
            }
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
}

void print_block_summary(const AsterixBlock& block) {
    std::cout << "Block Category " << static_cast<int>(block.category) 
              << " - Length: " << block.length 
              << " - Messages: " << block.messages.size() << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <asterix_file> [category_definitions_dir]" << std::endl;
        std::cout << "Example: " << argv[0] << " data.ast data/asterix_categories/" << std::endl;
        return 1;
    }
    
    std::string asterix_file = argv[1];
    std::string categories_dir = (argc > 2) ? argv[2] : "data/asterix_categories/";
    
    try {
        // Create the decoder
        AsterixDecoder decoder;
        decoder.set_debug_mode(true);
        
        // Load category definitions
        std::cout << "Loading category definitions from: " << categories_dir << std::endl;
        if (!decoder.load_categories_from_directory(categories_dir)) {
            std::cerr << "Failed to load category definitions!" << std::endl;
            return 1;
        }
        
        // Display supported categories
        auto supported_cats = decoder.get_supported_categories();
        std::cout << "Supported categories: ";
        for (auto cat : supported_cats) {
            std::cout << static_cast<int>(cat) << " ";
        }
        std::cout << std::endl;
        
        // Decode the ASTERIX file
        std::cout << "\nDecoding file: " << asterix_file << std::endl;
        auto blocks = decoder.decode_file(asterix_file);
        
        if (blocks.empty()) {
            std::cout << "No blocks decoded from file." << std::endl;
            return 1;
        }
        
        // Global statistics
        utils::MessageStatistics stats;
        std::vector<AsterixMessage> all_messages;
        
        // Process each block
        for (size_t i = 0; i < blocks.size(); ++i) {
            const auto& block = blocks[i];
            
            std::cout << "\n=== Block " << (i + 1) << " ====" << std::endl;
            print_block_summary(block);
            
            // Process each message in the block
            for (size_t j = 0; j < block.messages.size(); ++j) {
                const auto& message = block.messages[j];
                
                std::cout << "\n--- Message " << (j + 1) << " ---" << std::endl;
                print_message(message);
                
                all_messages.push_back(message);
                
                // Validation
                if (decoder.validate_message(message)) {
                    std::cout << "✓ Message validation: PASSED" << std::endl;
                } else {
                    std::cout << "✗ Message validation: FAILED" << std::endl;
                }
            }
        }
        
        // Display final statistics
        std::cout << "\n=== DECODING STATISTICS ===" << std::endl;
        stats = utils::analyze_messages(all_messages);
        utils::print_statistics(stats);
        
        // Export to JSON (optional)
        if (all_messages.size() > 0) {
            std::cout << "\nExporting first message to JSON..." << std::endl;
            std::string json = utils::to_json(all_messages[0]);
            
            std::ofstream json_file("output.json");
            if (json_file) {
                json_file << json;
                std::cout << "JSON exported to output.json" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nDecoding completed successfully!" << std::endl;
    return 0;
}