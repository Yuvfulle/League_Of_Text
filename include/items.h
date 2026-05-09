#pragma once
#include <iomanip>
#include "entity.h"
#include "buffs.h"

enum class ItemEffect {
    None, BlackCleaver, BotRK, TitanicHydra, DivineSunderer, DeathsDance, Thornmail
};

struct ItemStat { Stat stat; double value; bool maxOnly = false; };

struct ItemDef {
    std::string name;
    std::string shopText;
    std::vector<ItemStat> stats;
    ItemEffect effect = ItemEffect::None;
    int price = 300;
};

inline const std::vector<ItemDef>& itemDefs() {
    static const std::vector<ItemDef> defs = {
        {"Black Cleaver",       "(+400 HP, +40 ATK  | -5% armor/hit, max -30%)",
            {{Stat::MAX_HP, 400}, {Stat::ATK, 40}}, ItemEffect::BlackCleaver},
        {"BOTRK",               "(+40 ATK, 10% LS   | +8% target HP on-hit)",
            {{Stat::ATK, 40}, {Stat::LIFESTEAL, 0.10}}, ItemEffect::BotRK},
        {"Titanic Hydra",       "(+500 HP           | +2% max HP as ATK)",
            {{Stat::MAX_HP, 500}}, ItemEffect::TitanicHydra},
        {"Divine Sunderer",     "(+400 HP, +40 ATK  | 12% target HP magic + 50% heal)",
            {{Stat::MAX_HP, 400}, {Stat::ATK, 40}}, ItemEffect::DivineSunderer},
        {"Ravenous Hydra",      "(+40 ATK, +150 HP  | 10% omnivamp)",
            {{Stat::ATK, 40}, {Stat::MAX_HP, 150}, {Stat::OMNIVAMP, 0.10}}},
        {"Death's Dance",       "(+45 ATK           | 30% phys -> 3t bleed)",
            {{Stat::ATK, 45}}, ItemEffect::DeathsDance},
        {"Thornmail",           "(+60 Armor         | reflect 25% + GW)",
            {{Stat::ARMOR, 60}}, ItemEffect::Thornmail},
        {"Spirit Visage",       "(+400 HP, +50 MR   | +25% healing)",
            {{Stat::MAX_HP, 400}, {Stat::MAGIC_RESIST, 50}, {Stat::HEAL_AMP, 0.25}}},
        {"Void Staff",          "(+70 AP            | 40% magic pen)",
            {{Stat::AP, 70}, {Stat::MAG_PEN_PCT, 0.40, true}}},
        {"Serylda's Grudge",    "(+45 ATK           | 30% armor pen)",
            {{Stat::ATK, 45}, {Stat::ARM_PEN_PCT, 0.30, true}}},
        {"Youmuu's Ghostblade", "(+55 ATK           | 18 lethality)",
            {{Stat::ATK, 55}, {Stat::ARM_PEN_FLAT, 18}}},
        {"Shadowflame",         "(+100 AP           | 12 flat magic pen)",
            {{Stat::AP, 100}, {Stat::MAG_PEN_FLAT, 12}}},
        {"Riftmaker",           "(+70 AP, +300 HP   | 8% omnivamp)",
            {{Stat::AP, 70}, {Stat::MAX_HP, 300}, {Stat::OMNIVAMP, 0.08}}},
    };
    return defs;
}

class DefinedItem : public Item {
    const ItemDef& def;

public:
    explicit DefinedItem(const ItemDef& def) : def(def) {}

    std::string getName() const override { return def.name; }
    int getPrice() const override { return def.price; }

    void onPurchase(Entity& o) override {
        for (const auto& stat : def.stats) {
            if (stat.maxOnly) {
                o.setStatMax(stat.stat, stat.value);
            } else {
                o.addStat(stat.stat, stat.value);
                if (stat.stat == Stat::MAX_HP) o.setHP(o.getHP() + stat.value);
            }
        }

        if (def.effect == ItemEffect::TitanicHydra) {
            o.addBuffSilent(std::make_unique<TitanicBuff>());
        } else if (def.effect == ItemEffect::DeathsDance) {
            o.addBuffSilent(std::make_unique<DDPassiveBuff>());
        }
    }

    void onHit(Entity& self, Entity& tgt, AttackContext& ctx, CombatLog& log) override {
        switch (def.effect) {
            case ItemEffect::BlackCleaver:
                tgt.addBuff(std::make_unique<BCShredBuff>(), log);
                break;
            case ItemEffect::BotRK: {
                double bonus = tgt.getHP() * 0.08;
                ctx.finalPhysRaw += bonus;
                log.add("  [BOTRK] +" + std::to_string((int)bonus) + " (8% HP)");
                break;
            }
            case ItemEffect::DivineSunderer: {
                if (!tgt.isAlive()) return;
                double raw = tgt.getMaxHP() * 0.12;
                double eff = raw * calcResist(self.penMR(tgt));
                self.dealDamage(tgt, raw, true, log);
                double before = self.getHP();
                self.applyHeal(raw * 0.50);
                log.add("  [Divine] " + std::to_string((int)raw) + " -> " + std::to_string((int)eff)
                    + " magic, heal +" + std::to_string((int)(self.getHP() - before)));
                break;
            }
            default:
                break;
        }
    }

    void onDefend(Entity&, Entity& attacker, AttackContext& ctx, CombatLog& log) override {
        if (def.effect != ItemEffect::Thornmail) return;

        double raw = ctx.finalPhysRaw * 0.25;
        attacker.takeDmgSimple(raw, attacker.getMR());
        log.add("  [Thornmail] " + std::to_string((int)raw) + " reflected ("
            + ctx.selfLabel + " " + std::to_string((int)attacker.getHP()) + " HP)");

        if (auto* gw = attacker.getBuff("GW")) {
            gw->durationTurns = 2;
        } else {
            attacker.addBuff(std::make_unique<GWBuff>(), log);
        }
        log.add("  [GW] on " + ctx.selfLabel);
    }
};

inline std::unique_ptr<Item> createItem(int c) {
    const auto& defs = itemDefs();
    if (c < 1 || c > (int)defs.size()) return nullptr;
    return std::make_unique<DefinedItem>(defs[c - 1]);
}

inline void printShopMenu() {
    const auto& defs = itemDefs();
    for (std::size_t i = 0; i < defs.size(); i++) {
        std::cout << std::right << std::setw(2) << (i + 1) << ". "
             << std::left << std::setw(20) << defs[i].name << defs[i].shopText << "\n";
    }
    std::cout << std::right;
}
