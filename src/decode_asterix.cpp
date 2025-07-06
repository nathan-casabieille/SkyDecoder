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

// Function to test the decoder with test data
void test_decoder() {
    // Example ASTERIX Category 048 data (simplified)
    std::vector<uint8_t> test_data = {
        0x30,        // Category 048
        0x00, 0x1C,  // Length = 28 bytes
        0xFD, 0x00,  // FSPEC (I048/010, I048/140, I048/020, I048/040, I048/070, I048/090, I048/130)
        
        // I048/010 - Data Source Identifier
        0x01, 0x02,  // SAC=1, SIC=2
        
        // I048/140 - Time of Day  
        0x12, 0x34, 0x56,  // ToD = 1193046 (in 1/128 seconds)
        
        // I048/020 - Target Report Descriptor
        0x25,        // TYP=1, SIM=0, RDP=0, SPI=1, RAB=0, FX=1
        0x80,        // TST=1, others=0, FX=0
        
        // I048/040 - Measured Position
        0x10, 0x00,  // RHO = 4096 (16 NM)
        0x20, 0x00,  // THETA = 8192 (45 degrees)
        
        // I048/070 - Mode-3/A Code
        0x20, 0x12,  // V=0, G=0, L=1, MODE3A=0x012 (octal)
        
        // I048/090 - Flight Level
        0x00, 0x64,  // V=0, G=0, FL=100 (FL 025)
        
        // I048/130 - Radar Plot Characteristics  
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
    };
    
    AsterixDecoder decoder;
    decoder.set_debug_mode(true);
    
    // Load a category 048 definition (should be in the data/ directory)
    if (decoder.load_category_definition("data/asterix_categories/cat048.xml")) {
        auto block = decoder.decode_block(test_data);
        
        std::cout << "=== TEST DECODING RESULTS ===" << std::endl;
        print_block_summary(block);
        
        for (const auto& message : block.messages) {
            print_message(message);
        }
    } else {
        std::cout << "Failed to load category 048 definition for testing." << std::endl;
    }
}