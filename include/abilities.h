#pragma once
#include "entity.h"
#include "buffs.h"

enum class AbilityEffect {
    Corrosion, Reaper, SoulEater, StrengthAboveAllElse, BattleFury,
    FightOrBeForgotten, BloodDrain, Obliterate, Judgement, Dominus
};

struct AbilityDef {
    std::string name;
    AbilityEffect effect;
    int cooldown = 0;
    bool active = false;
    bool toggle = false;
};

inline const std::vector<AbilityDef>& activeAbilityDefs() {
    static const std::vector<AbilityDef> defs = {
        {"FIGHT_OR_BE_FORGOTTEN", AbilityEffect::FightOrBeForgotten, 5, true, true},
        {"BLOOD_DRAIN", AbilityEffect::BloodDrain, 4, true},
        {"OBLITERATE", AbilityEffect::Obliterate, 1, true},
        {"JUDGEMENT", AbilityEffect::Judgement, 4, true},
        {"DOMINUS", AbilityEffect::Dominus, 8, true},
    };
    return defs;
}

inline const std::vector<AbilityDef>& passiveAbilityDefs() {
    static const std::vector<AbilityDef> defs = {
        {"CORROSION", AbilityEffect::Corrosion},
        {"REAPER", AbilityEffect::Reaper},
        {"SOUL_EATER", AbilityEffect::SoulEater},
        {"STRENGTH_ABOVE_ALL_ELSE", AbilityEffect::StrengthAboveAllElse},
        {"BATTLE_FURY", AbilityEffect::BattleFury},
    };
    return defs;
}

class DefinedAbility : public Ability {
    const AbilityDef& def;

public:
    explicit DefinedAbility(const AbilityDef& def) : def(def) {}

    std::string getName() const override { return def.name; }
    int getCooldown() const override { return def.cooldown; }
    bool isToggle() const override { return def.toggle; }
    bool canActivate() const override { return def.active; }

    void onEquip(Entity& self) override {
        if (def.effect == AbilityEffect::SoulEater) self.addStat(Stat::OMNIVAMP, 0.10);
    }

    PreAttackMod onPreAttack(Entity& self) override {
        if (def.effect != AbilityEffect::BattleFury) return {};
        double chance = std::min(self.getATK() * 0.15, 100.0);
        return (std::rand() % 100) < (int)chance ? PreAttackMod{2.0, true, chance} : PreAttackMod{1.0, false, chance};
    }

    void onPostHit(Entity& self, Entity& tgt, AttackContext&, CombatLog& log) override {
        if (def.effect == AbilityEffect::Corrosion) {
            if (tgt.getHP() <= 0) return;
            double raw = tgt.getHP() * 0.05;
            double eff = raw * calcResist(self.penMR(tgt));
            self.dealDamage(tgt, raw, true, log);
            log.add("  [Corrosion] " + std::to_string((int)raw) + " -> " + std::to_string((int)eff) + " magic");
        } else if (def.effect == AbilityEffect::Reaper && tgt.getHP() > 0 && tgt.getHP() <= tgt.getMaxHP() * 0.05) {
            tgt.setHP(0);
            log.add("  [Reaper] *** EXECUTE ***");
        }
    }

    void onAttackDone(Entity& self, CombatLog& log) override {
        if (def.effect == AbilityEffect::StrengthAboveAllElse) self.addBuff(std::make_unique<SAECounterBuff>(), log);
    }

    bool activate(Entity& self, Entity& tgt, const std::string& lbl, CombatLog& log) override {
        switch (def.effect) {
            case AbilityEffect::FightOrBeForgotten: {
                double cost = self.getMaxHP() * 0.50;
                if (self.getHP() <= cost) { log.add("[" + lbl + "] FOBF: not enough HP!"); return false; }
                self.setHP(self.getHP() - cost);
                double bonus = self.getATK();
                self.addBuff(std::make_unique<FOBFBuff>(bonus), log);
                log.add("[" + lbl + " FOBF] -" + std::to_string((int)cost) + " HP, +" + std::to_string((int)bonus) + " ATK, 100% LS x3");
                return true;
            }
            case AbilityEffect::BloodDrain: {
                double selfDmg = self.getHP() * 0.10 + self.getAP() * 0.10;
                self.setHP(std::max(0.0, self.getHP() - selfDmg));
                double raw = tgt.getMaxHP() * (0.10 + self.getAP() * 0.0025);
                double eff = raw * calcResist(self.penMR(tgt));
                self.dealDamage(tgt, raw, true, log);
                double before = self.getHP();
                self.applyHeal(eff * 1.50);
                log.add("[" + lbl + " BLOOD_DRAIN] self:-" + std::to_string((int)selfDmg)
                    + " | " + std::to_string((int)raw) + " -> " + std::to_string((int)eff)
                    + " magic | heal:+" + std::to_string((int)(self.getHP() - before)));
                return true;
            }
            case AbilityEffect::Obliterate: {
                double raw = 300.0 + self.getAP() * 1.5;
                double eff = raw * calcResist(self.penMR(tgt));
                self.dealDamage(tgt, raw, true, log);
                log.add("[" + lbl + " OBLITERATE] " + std::to_string((int)raw) + " -> " + std::to_string((int)eff) + " magic");
                return true;
            }
            case AbilityEffect::Judgement: {
                double raw = self.getATK() + self.getHP() * 0.20;
                double eff = raw * calcResist(self.penArmor(tgt));
                self.dealDamage(tgt, raw, false, log);
                tgt.turnBlocked = 1;
                log.add("[" + lbl + " JUDGEMENT] " + std::to_string((int)raw) + " -> " + std::to_string((int)eff) + " phys (STUN 1t)");
                return true;
            }
            case AbilityEffect::Dominus: {
                if (self.getBuff("DOMINUS")) { log.add("[" + lbl + "] Dominus active!"); return false; }
                double dot = 80.0 + self.getAP() * 0.30;
                self.addBuff(std::make_unique<DominusBuff>(dot), log);
                self.setHP(self.getHP() + 1000.0);
                log.add("[" + lbl + " DOMINUS] +1000 HP (5t), DoT:" + std::to_string((int)dot) + "/t");
                return true;
            }
            default:
                return false;
        }
    }
};

inline std::unique_ptr<Ability> createActiveAbility(int c) {
    const auto& defs = activeAbilityDefs();
    if (c < 1 || c > (int)defs.size()) return nullptr;
    return std::make_unique<DefinedAbility>(defs[c - 1]);
}

inline std::unique_ptr<Ability> createPassiveAbility(int c) {
    const auto& defs = passiveAbilityDefs();
    if (c < 1 || c > (int)defs.size()) return nullptr;
    return std::make_unique<DefinedAbility>(defs[c - 1]);
}
