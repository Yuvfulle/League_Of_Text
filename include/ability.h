#pragma once
#include "types.h"

class Ability {
public:
    virtual ~Ability() = default;
    virtual std::string getName() const = 0;
    virtual int getCooldown() const { return 0; }
    virtual bool isToggle() const { return false; }
    virtual bool canActivate() const { return false; }
    virtual bool activate(Entity&, Entity&, const std::string&, CombatLog&) { return false; }
    virtual void onEquip(Entity&) {}

    struct PreAttackMod { double atkMul = 1.0; bool isCrit = false; double critChance = 0; };
    virtual PreAttackMod onPreAttack(Entity&) { return {}; }
    virtual void onPostHit(Entity&, Entity&, AttackContext&, CombatLog&) {}
    virtual void onAttackDone(Entity&, CombatLog&) {}
};
