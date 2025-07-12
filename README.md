# SkyDecoder

A light C++ library for decoding ASTERIX (All Purpose STructured Eurocontrol Radar Information EXchange) messages used in air traffic control systems.

```diff
- WARNING! -

This repository is a work in progress
```

## Features

- **Multi-category Support**: Supports various ASTERIX categories (CAT002, CAT048, CAT062, etc.)
- **Multi-record Decoding**: Handles complex multi-record structures
- **XML-based Configuration**: Category definitions loaded from XML files
- **Comprehensive Validation**: Built-in message validation and error reporting
- **Performance Optimized**: Efficient parsing with optional debug modes
- **JSON Export**: Convert decoded messages to JSON format

## Requirements

- C++17 or later
- CMake 3.15 or higher
- TinyXML2 library

## Installation

### Building from Source

```bash
git clone git@github.com:nathan-casabieille/SkyDecoder.git
cd SkyDecoder
mkdir build && cd build
cmake ..
make
```

## Quick Start

### Basic Usage

```cpp
#include <skydecoder/asterix_decoder.h>
#include <skydecoder/utils.h>

using namespace skydecoder;

int main() {
    // Create decoder instance
    AsterixDecoder decoder;
    decoder.set_debug_mode(true);
    
    // Load category definitions
    if (!decoder.load_categories_from_directory("data/asterix_categories/")) {
        std::cerr << "Failed to load category definitions!" << std::endl;
        return 1;
    }
    
    // Example ASTERIX message (CAT002)
    std::vector<uint8_t> message = {
        0x02, 0x00, 0x16,           // Header: CAT=2, LEN=22
        0xF0,                       // FSPEC: I002/010, I002/000, I002/020, I002/030
        0x00, 0x10,                 // I002/010: SAC=0, SIC=16
        0x01,                       // I002/000: Message Type=1
        0x00,                       // I002/020: Sector=0
        0x12, 0x34, 0x56,           // I002/030: Time of Day
        // ... more data
    };
    
    // Decode the block
    auto block = decoder.decode_block(message);
    
    if (block.valid) {
        std::cout << "Successfully decoded " << block.messages.size() 
                  << " records" << std::endl;
        
        // Process each record
        for (const auto& record : block.messages) {
            std::cout << "Record with " << record.data_items.size() 
                      << " data items" << std::endl;
        }
    }
    
    return 0;
}
```

### Decoding from File

```cpp
#include <skydecoder/asterix_decoder.h>

int main() {
    AsterixDecoder decoder;
    
    // Load category definitions
    decoder.load_categories_from_directory("data/asterix_categories/");
    
    // Decode ASTERIX file
    auto blocks = decoder.decode_file("data/asterix_data.ast");
    
    for (const auto& block : blocks) {
        std::cout << "Block CAT" << static_cast<int>(block.category) 
                  << " with " << block.messages.size() << " messages" << std::endl;
    }
    
    return 0;
}
```

## Category Definitions

SkyDecoder uses XML files to define ASTERIX category structures. Place your category definition files in a directory structure like:

```
data/asterix_categories/
├── cat01.xml       # CAT001 - Monoradar Target Reports
├── cat02.xml       # CAT002 - Monoradar Service Messages
├── cat34.xml       # CAT034 - Monoradar Service Messages
└── cat48.xml       # CAT048 - Monoradar Target Reports
```

### Example Category Definition (CAT002)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<asterix_category>
    <header>
        <category>2</category>
        <name>Monoradar Target Reports</name>
        <description>Messages for radar target detection</description>
        <version>1.0</version>
        <date>2024-01-01</date>
    </header>
    
    <user_application_profile>
        <uap_items>
            <item>I002/010</item>  <!-- Data Source Identifier -->
            <item>I002/000</item>  <!-- Message Type -->
            <item>I002/020</item>  <!-- Sector Number -->
            <item>I002/030</item>  <!-- Time of Day -->
            <item>I002/041</item>  <!-- Antenna Rotation Period -->
            <item>spare</item>
            <item>spare</item>
            <!-- More items... -->
        </uap_items>
    </user_application_profile>
    
    <data_items>
        <data_item id="I002/010">
            <name>Data Source Identifier</name>
            <definition>Identification of the radar station</definition>
            <format>fixed</format>
            <length>2</length>
            <structure>
                <field name="SAC" type="uint8" bits="8" description="System Area Code"/>
                <field name="SIC" type="uint8" bits="8" description="System Identification Code"/>
            </structure>
        </data_item>
        
        <data_item id="I002/000">
            <name>Message Type</name>
            <definition>Type of radar message</definition>
            <format>fixed</format>
            <length>1</length>
            <structure>
                <field name="Message Type" type="uint8" bits="8" description="Message Type">
                    <enum value="1">North Marker</enum>
                    <enum value="2">Sector Crossing</enum>
                    <enum value="3">South Marker</enum>
                </field>
            </structure>
        </data_item>
        
        <!-- More data items... -->
    </data_items>
</asterix_category>
```

## Advanced Features

### Multi-Record Support

SkyDecoder supports multi-record structures where a single ASTERIX block can contain multiple records:

```cpp
// The decoder automatically handles multi-record blocks
auto block = decoder.decode_block(cat002_data);

// Each message represents one record
for (size_t i = 0; i < block.messages.size(); ++i) {
    const auto& record = block.messages[i];
    std::cout << "Record #" << (i + 1) << " has " 
              << record.data_items.size() << " data items" << std::endl;
}

// Validate the multi-record structure
if (decoder.validate_multirecord_block(block)) {
    std::cout << "Multi-record validation passed" << std::endl;
}
```

### Message Validation

```cpp
AsterixDecoder decoder;
decoder.set_strict_validation(true);  // Enable strict validation

auto message = decoder.decode_message(2, message_data);

if (decoder.validate_message(message)) {
    std::cout << "Message validation passed" << std::endl;
} else {
    std::cout << "Validation failed: " << message.error_message << std::endl;
}
```

### Accessing Decoded Fields

```cpp
for (const auto& item : message.data_items) {
    std::cout << "[" << item.id << "] " << item.name << std::endl;
    
    for (const auto& field : item.fields) {
        std::cout << "  " << field.name << ": ";
        
        // Access typed values
        std::visit([](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, uint8_t>) {
                std::cout << "0x" << std::hex << static_cast<int>(value);
            } else if constexpr (std::is_same_v<T, std::string>) {
                std::cout << "\"" << value << "\"";
            } else {
                std::cout << value;
            }
        }, field.value);
        
        std::cout << std::endl;
    }
}
```

### JSON Export

```cpp
#include <skydecoder/utils.h>

// Export single message to JSON
std::string json = utils::to_json(message);
std::cout << json << std::endl;

// Export entire block to JSON
std::string block_json = utils::to_json(block);

// Save to file
std::ofstream file("output.json");
file << block_json;
```

## Error Handling

```cpp
try {
    auto block = decoder.decode_block(data);
    
    if (!block.valid) {
        std::cerr << "Block decoding failed" << std::endl;
        return;
    }
    
    for (const auto& message : block.messages) {
        if (!message.valid) {
            std::cerr << "Message error: " << message.error_message << std::endl;
            continue;
        }
        
        for (const auto& item : message.data_items) {
            if (!item.valid) {
                std::cerr << "Item " << item.id << " error: " 
                          << item.error_message << std::endl;
            }
        }
    }
    
} catch (const std::exception& e) {
    std::cerr << "Decoding exception: " << e.what() << std::endl;
}
```
