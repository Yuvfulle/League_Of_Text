#pragma once
#include "entity.h"

class DDBleedBuff : public Buff {
public:
    std::vector<std::pair<double, int>> portions;

    DDBleedBuff(double dot) { 
        portions.push_back({dot, 3});
        durationTurns = -1; 
    }
    std::string id() const override { return "DD_BLEED"; }

    void merge(Buff* other, Entity&, CombatLog&) override {
        for (auto& p : static_cast<DDBleedBuff*>(other)->portions)
            portions.push_back(p);
    }

    void onTurnEnd(Entity& owner, Entity&, CombatLog& log) override {
        double total = 0;
        for (auto& [dmg, t] : portions) { total += dmg; t--; }
        owner.setHP(owner.getHP() - total);
        portions.erase(std::remove_if(portions.begin(), portions.end(),
            [](auto& p) { return p.second <= 0; }), portions.end());
        int mx = 0; for (auto& [d, t] : portions) mx = std::max(mx, t);
        log.add("  [DD Bleed] " + std::to_string((int)total) + " true dmg (" + std::to_string(mx) + "t)");
    }

    bool isExpired() const override { return portions.empty(); }

    std::string getStats() const override {
        double t = 0; for (auto& [d, _] : portions) t += d;
        return "DD " + std::to_string((int)t) + "/t";
    }
};

class DDPassiveBuff : public Buff {
public:
    std::string id() const override { return "DD_PASSIVE"; }
    double delayedDamageFraction(bool magic) const override { return magic ? 0.0 : 0.30; }
    std::unique_ptr<Buff> createDelayedDOT(double dotPerTick) override {
        return std::make_unique<DDBleedBuff>(dotPerTick);
    }
};

class TitanicBuff : public Buff {
public:
    std::string id() const override { return "TITANIC"; }
    double statMod(Stat s, const Entity& o) const override {
        return s == Stat::ATK ? o.getMaxHP() * 0.02 : 0;
    }
};

class BCShredBuff : public Buff {
    int stacks = 1;
public:
    BCShredBuff() { durationTurns = 4; }
    std::string id() const override { return "BC_SHRED"; }
    void onAdded(Entity& o, CombatLog& log) override {
        log.add("  [BC] -5% (1/6) armor=" + std::to_string((int)o.getArmor()));
    }
    void merge(Buff* other, Entity& o, CombatLog& log) override {
        if (stacks < 6) stacks++;
        durationTurns = other->durationTurns;
        log.add("  [BC] -" + std::to_string(stacks*5) + "% (" + std::to_string(stacks) + "/6) armor=" + std::to_string((int)o.getArmor()));
    }
    double armorShred() const override { return stacks * 0.05; }
    std::string getStats() const override { return "BC -" + std::to_string(stacks*5) + "%"; }
};

class GWBuff : public Buff {
public:
    GWBuff() { durationTurns = 2; }
    std::string id() const override { return "GW"; }
    void merge(Buff*, Entity&, CombatLog&) override { durationTurns = 2; }
    double healReduction() const override { return 0.60; }
    std::string getStats() const override { return "GW -60%heal"; }
};

class SAERageBuff : public Buff {
public:
    SAERageBuff() { attacksRemaining = 3; }
    std::string id() const override { return "SAE_RAGE"; }
    double statMod(Stat s, const Entity&) const override {
        return s == Stat::ATK ? 170.0 : 0;
    }
    std::string getStats() const override { return "RAGE +170ATK"; }
};

class SAECounterBuff : public Buff {
    int stacks = 1;
public:
    std::string id() const override { return "SAE_COUNTER"; }
    void onAdded(Entity&, CombatLog& log) override { log.add("[SAE] 1/3"); }
    void merge(Buff*, Entity& o, CombatLog& log) override {
        if (++stacks >= 3) {
            o.removeBuff("SAE_RAGE");
            o.addBuffSilent(std::make_unique<SAERageBuff>());
            log.add("[SAE] RAGE! +170 ATK x3");
            attacksRemaining = 0;
        } else {
            log.add("[SAE] " + std::to_string(stacks) + "/3");
        }
    }
    std::string getStats() const override { return "SAE " + std::to_string(stacks) + "/3"; }
};

class FOBFBuff : public Buff {
    double atkBonus;
public:
    FOBFBuff(double atk) : atkBonus(atk) { attacksRemaining = 3; }
    std::string id() const override { return "FOBF"; }
    double statMod(Stat s, const Entity&) const override {
        if (s == Stat::ATK) return atkBonus;
        if (s == Stat::LIFESTEAL) return 1.0;
        return 0;
    }
    std::string getStats() const override { return "FOBF +" + std::to_string((int)atkBonus) + "ATK"; }
};

class DominusBuff : public Buff {
    double dot;
public:
    DominusBuff(double d) : dot(d) { durationTurns = 5; }
    std::string id() const override { return "DOMINUS"; }
    double statMod(Stat s, const Entity&) const override {
        return s == Stat::MAX_HP ? 1000.0 : 0;
    }
    void onTurnEnd(Entity& owner, Entity& opp, CombatLog& log) override {
        if (durationTurns == 0) return;
        double eff = dot * calcResist(owner.penMR(opp));
        owner.dealDamage(opp, dot, true, log);
        log.add("  [DOMINUS] " + std::to_string((int)dot) + " -> " + std::to_string((int)eff) + " magic (" + std::to_string(durationTurns) + "t)");
    }
    std::string getStats() const override { return "DOMINUS"; }
};
