#pragma once
#include "types.h"

class Buff {
public:
    int durationTurns = -1;
    int attacksRemaining = -1;
    virtual ~Buff() = default;

    virtual std::string id() const = 0;
    virtual void onAdded(Entity&, CombatLog&) {}
    virtual void merge(Buff*, Entity&, CombatLog&) {}
    virtual void onTurnEnd(Entity&, Entity&, CombatLog&) {}

    virtual double statMod(Stat, const Entity&) const { return 0; }

    virtual double armorShred() const { return 0; }
    virtual double healReduction() const { return 0; }

    virtual double delayedDamageFraction(bool /*magic*/) const { return 0; }
    virtual std::unique_ptr<Buff> createDelayedDOT(double /*dotPerTick*/) { return nullptr; }

    virtual std::string getStats() const { return ""; }
    virtual bool isExpired() const { return durationTurns == 0 || attacksRemaining == 0; }
};
