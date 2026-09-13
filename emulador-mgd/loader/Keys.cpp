// KeyManager implementation - key loading from prod.keys/title.keys

#include "Keys.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace mgd {
namespace emu {

// Master key indices (from switchbrew/hactool)
enum MasterKeyIndex {
    MASTER_KEY_00 = 0,   // 1.0.0
    MASTER_KEY_01 = 1,   // 2.0.0
    MASTER_KEY_02 = 2,   // 3.0.0
    MASTER_KEY_03 = 3,   // 4.0.0
    MASTER_KEY_04 = 4,   // 5.0.0
    MASTER_KEY_05 = 5,   // 6.0.0
    MASTER_KEY_06 = 6,   // 7.0.0
    MASTER_KEY_07 = 7,   // 8.0.0
    MASTER_KEY_08 = 8,   // 9.0.0
    MASTER_KEY_09 = 9,   // 10.0.0
    MASTER_KEY_10 = 10,  // 11.0.0
    MASTER_KEY_11 = 11,  // 12.0.0
    MASTER_KEY_12 = 12,  // 13.0.0
    MASTER_KEY_13 = 13,  // 14.0.0
    MASTER_KEY_14 = 14,  // 15.0.0
    MASTER_KEY_15 = 15,  // 16.0.0
    MASTER_KEY_16 = 16,  // 17.0.0
    MASTER_KEY_COUNT
};

// Key derivation constants
static const uint8_t XTS_KEY1_MODIFIER[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t XTS_KEY2_MODIFIER[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

bool KeyManager::loadKeysFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        if (line.empty() || line[0] == '#') continue;
        
        // Parse key=value format
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key_name = line.substr(0, eq);
        std::string key_value = line.substr(eq + 1);
        
        // Trim whitespace
        key_name.erase(0, key_name.find_first_not_of(" \t"));
        key_name.erase(key_name.find_last_not_of(" \t") + 1);
        key_value.erase(0, key_value.find_first_not_of(" \t"));
        key_value.erase(key_value.find_last_not_of(" \t") + 1);
        
        // Parse master_key_XX = value
        if (key_name.rfind("master_key_", 0) == 0) {
            int idx = std::stoi(key_name.substr(11));
            if (idx >= 0 && idx < MASTER_KEY_COUNT) {
                uint8_t key[16];
                if (parseHex(key_value, key, 16)) {
                    setSlot(idx, key);
                }
            }
        }
    }
    return true;
}

bool KeyManager::loadProdKeys(const std::string& path) {
    return loadKeysFromFile(path);
}

bool KeyManager::loadTitleKeys(const std::string& path) {
    return loadKeysFromFile(path);
}

bool KeyManager::parseHex(const std::string& hex, uint8_t* out, size_t len) {
    if (hex.size() != len * 2) return false;
    for (size_t i = 0; i < len; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        out[i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
    }
    return true;
}

// Derive NCA header key (XTS) from master key
void KeyManager::deriveNcaHeaderKey(int master_key_idx, uint8_t* out_key1, uint8_t* out_key2) const {
    if (master_key_idx < 0 || master_key_idx >= MASTER_KEY_COUNT || !have_[master_key_idx]) return;
    
    // Key1 = master_key + 0x00...00 (no modifier for key1)
    std::memcpy(out_key1, keys_[master_key_idx], 16);
    
    // Key2 = master_key + 0x01 (XTS key2 modifier)
    for (int i = 0; i < 16; ++i) {
        out_key2[i] = keys_[master_key_idx][i] ^ XTS_KEY2_MODIFIER[i];
    }
}

// Derive NCA section key (CTR) from master key and key generation
void KeyManager::deriveNcaSectionKey(int master_key_idx, int key_generation, uint8_t* out_key) const {
    if (master_key_idx < 0 || master_key_idx >= MASTER_KEY_COUNT || !have_[master_key_idx]) return;
    
    // Key = master_key + key_generation (simplified - real derivation is more complex)
    // Real implementation uses keyblob/keyarea derivation
    std::memcpy(out_key, keys_[master_key_idx], 16);
    
    // Mix in key generation (simplified)
    for (int i = 0; i < 4 && i < 16; ++i) {
        out_key[i] ^= static_cast<uint8_t>((key_generation >> (i * 8)) & 0xFF);
    }
}

// Load prod.keys and title.keys from directory
bool KeyManager::loadFromDirectory(const std::string& dir) {
    std::string prod = dir + "/prod.keys";
    std::string title = dir + "/title.keys";
    bool ok = true;
    if (!loadProdKeys(prod)) ok = false;
    if (!loadTitleKeys(title)) ok = false;
    return ok;
}

} // namespace emu
} // namespace mgd