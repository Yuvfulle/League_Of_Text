#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <map>
#include <array>
#include <memory>
#include <utility>

class Entity; 

enum class Stat {
    ATK, AP, MAX_HP, ARMOR, MAGIC_RESIST,
    LIFESTEAL, OMNIVAMP, HEAL_AMP,
    MAG_PEN_PCT, MAG_PEN_FLAT,
    ARM_PEN_PCT, ARM_PEN_FLAT,
    COUNT
};

constexpr std::size_t statIndex(Stat s) {
    return static_cast<std::size_t>(s);
}

class CombatLog {
    std::vector<std::string> lines;
public:
    void add(const std::string& s) { lines.push_back(s); }
    void flush() { 
        for (auto& l : lines) std::cout << l << "\n"; 
        lines.clear(); 
    }
};

inline double calcResist(double def) {
    return 100.0 / (100.0 + std::max(0.0, def));
}

struct AttackContext {
    std::string selfLabel, targetLabel;
    double atk = 0;
    bool isCrit = false;
    double critChance = 0;
    double finalPhysRaw = 0;
};
