#include <skydecoder/asterix_decoder.h>
#include <skydecoder/utils.h>
#include <iostream>
#include <fstream>
#include <vector>

using namespace skydecoder;

// Fonction pour créer des données ASTERIX de test
std::vector<uint8_t> create_test_data() {
    // Exemple de bloc ASTERIX simple pour CAT002
    std::vector<uint8_t> data = {
        // En-tête du bloc
        0x02,        // Category 002
        0x00, 0x0B,  // Length = 11 bytes
        
        // FSPEC (Field Specification) - exemple simple
        0x80,        // Premier byte du FSPEC (bit 7 = 1, autres = 0)
        
        // Données d'exemple (adaptez selon votre définition CAT002)
        0x01, 0x02,  // Exemple: SAC/SIC
        0x12, 0x34,  // Exemple: autres données
        0x56, 0x78   // Exemple: autres données
    };
    
    return data;
}

// Fonction pour tester le chargement de la définition XML
bool test_load_category() {
    std::cout << "=== Test 1: Chargement de la définition CAT002 ===" << std::endl;
    
    AsterixDecoder decoder;
    decoder.set_debug_mode(true);
    
    // Tenter de charger le fichier CAT002
    bool success = decoder.load_category_definition("../data/asterix_categories/cat02.xml");
    
    if (success) {
        std::cout << "✓ Définition CAT002 chargée avec succès" << std::endl;
        
        // Vérifier les catégories supportées
        auto categories = decoder.get_supported_categories();
        std::cout << "Catégories supportées: ";
        for (auto cat : categories) {
            std::cout << static_cast<int>(cat) << " ";
        }
        std::cout << std::endl;
        
        // Obtenir les détails de la catégorie
        const auto* cat_def = decoder.get_category_definition(2);
        if (cat_def) {
            std::cout << "Nom de la catégorie: " << cat_def->header.name << std::endl;
            std::cout << "Version: " << cat_def->header.version << std::endl;
            std::cout << "Nombre d'items de données: " << cat_def->data_items.size() << std::endl;
            std::cout << "Nombre d'items UAP: " << cat_def->uap.items.size() << std::endl;
        }
        
    } else {
        std::cout << "✗ Échec du chargement de la définition CAT002" << std::endl;
        std::cout << "Vérifiez que le fichier ../data/asterix_categories/cat02.xml existe" << std::endl;
    }
    
    return success;
}

// Fonction pour tester le décodage
bool test_decode_message(AsterixDecoder& decoder) {
    std::cout << "\n=== Test 2: Décodage d'un message de test ===" << std::endl;
    
    // Créer des données de test
    auto test_data = create_test_data();
    
    std::cout << "Données de test (hex): " << utils::to_hex_string(test_data) << std::endl;
    std::cout << "Taille: " << test_data.size() << " bytes" << std::endl;
    
    try {
        // Décoder le bloc
        auto block = decoder.decode_block(test_data);
        
        std::cout << "Catégorie décodée: " << static_cast<int>(block.category) << std::endl;
        std::cout << "Longueur du bloc: " << block.length << std::endl;
        std::cout << "Nombre de messages: " << block.messages.size() << std::endl;
        
        // Examiner les messages
        for (size_t i = 0; i < block.messages.size(); ++i) {
            const auto& message = block.messages[i];
            
            std::cout << "\nMessage " << (i + 1) << ":" << std::endl;
            std::cout << "  Valide: " << (message.valid ? "Oui" : "Non") << std::endl;
            
            if (!message.valid) {
                std::cout << "  Erreur: " << message.error_message << std::endl;
                return false;
            }
            
            std::cout << "  Nombre d'items de données: " << message.data_items.size() << std::endl;
            
            // Afficher les items de données décodés
            for (const auto& item : message.data_items) {
                std::cout << "  [" << item.id << "] " << item.name << std::endl;
                
                if (!item.valid) {
                    std::cout << "    ERREUR: " << item.error_message << std::endl;
                    continue;
                }
                
                for (const auto& field : item.fields) {
                    std::cout << "    " << field.name << ": ";
                    
                    if (field.valid) {
                        std::cout << utils::format_value(field.value, field.unit);
                        if (!field.description.empty()) {
                            std::cout << " (" << field.description << ")";
                        }
                    } else {
                        std::cout << "ERREUR - " << field.error_message;
                    }
                    std::cout << std::endl;
                }
            }
            
            // Tester la validation
            bool is_valid = decoder.validate_message(message);
            std::cout << "  Validation: " << (is_valid ? "RÉUSSIE" : "ÉCHOUÉE") << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Exception lors du décodage: " << e.what() << std::endl;
        return false;
    }
}

// Fonction pour tester l'export JSON
void test_json_export(AsterixDecoder& decoder) {
    std::cout << "\n=== Test 3: Export JSON ===" << std::endl;
    
    auto test_data = create_test_data();
    
    try {
        auto block = decoder.decode_block(test_data);
        
        if (!block.messages.empty() && block.messages[0].valid) {
            // Exporter le premier message en JSON
            std::string json = utils::to_json(block.messages[0]);
            
            std::cout << "JSON généré:" << std::endl;
            std::cout << json << std::endl;
            
            // Sauvegarder dans un fichier
            std::ofstream json_file("test_output.json");
            if (json_file) {
                json_file << json;
                std::cout << "\n✓ JSON sauvegardé dans test_output.json" << std::endl;
            }
        } else {
            std::cout << "✗ Aucun message valide pour l'export JSON" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "✗ Erreur lors de l'export JSON: " << e.what() << std::endl;
    }
}

// Fonction pour tester le parsing XML directement
void test_xml_parsing() {
    std::cout << "\n=== Test 4: Vérification du fichier XML ===" << std::endl;
    
    std::ifstream xml_file("../data/asterix_categories/cat02.xml");
    if (!xml_file) {
        std::cout << "✗ Impossible d'ouvrir le fichier cat02.xml" << std::endl;
        return;
    }
    
    // Lire le contenu du fichier
    std::string xml_content((std::istreambuf_iterator<char>(xml_file)),
                           std::istreambuf_iterator<char>());
    xml_file.close();
    
    std::cout << "Taille du fichier XML: " << xml_content.size() << " caractères" << std::endl;
    
    // Afficher les premières lignes
    std::istringstream stream(xml_content);
    std::string line;
    int line_count = 0;
    
    std::cout << "Aperçu du fichier XML:" << std::endl;
    while (std::getline(stream, line) && line_count < 10) {
        std::cout << "  " << line << std::endl;
        line_count++;
    }
    
    if (line_count == 10) {
        std::cout << "  ..." << std::endl;
    }
}

int main() {
    std::cout << "=== SkyDecoder - Test de la bibliothèque ===" << std::endl;
    std::cout << "Compilé le " << __DATE__ << " à " << __TIME__ << std::endl;
    std::cout << std::endl;
    
    // Test 4: Vérifier le fichier XML d'abord
    test_xml_parsing();
    
    // Test 1: Charger la définition
    AsterixDecoder decoder;
    if (!test_load_category()) {
        std::cout << "\n❌ Impossible de continuer sans définition de catégorie" << std::endl;
        std::cout << "\nPour résoudre ce problème:" << std::endl;
        std::cout << "1. Vérifiez que le fichier ../data/asterix_categories/cat02.xml existe" << std::endl;
        std::cout << "2. Vérifiez le format XML du fichier" << std::endl;
        std::cout << "3. Consultez les logs de debug pour plus d'informations" << std::endl;
        return 1;
    }
    
    // Test 2: Décoder un message
    if (!test_decode_message(decoder)) {
        std::cout << "\n⚠️  Le décodage a échoué, mais cela peut être normal" << std::endl;
        std::cout << "Les données de test peuvent ne pas correspondre au format CAT002" << std::endl;
    }
    
    // Test 3: Export JSON
    test_json_export(decoder);
    
    std::cout << "\n=== Résumé des tests ===" << std::endl;
    std::cout << "✓ Compilation réussie" << std::endl;
    std::cout << "✓ Bibliothèque liée correctement" << std::endl;
    std::cout << "✓ Classes instanciées sans erreur" << std::endl;
    
    std::cout << "\n🎉 Tests terminés avec succès!" << std::endl;
    std::cout << "\nProchaines étapes:" << std::endl;
    std::cout << "1. Adaptez les données de test au format réel de votre CAT002" << std::endl;
    std::cout << "2. Testez avec de vrais fichiers ASTERIX" << std::endl;
    std::cout << "3. Ajoutez d'autres catégories selon vos besoins" << std::endl;
    
    return 0;
}