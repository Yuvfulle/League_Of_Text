#pragma once
#include "types.h"

class Item {
public:
    virtual ~Item() = default;
    virtual std::string getName() const = 0;
    virtual int getPrice() const { return 300; }
    virtual void onPurchase(Entity&) = 0;
    virtual void onHit(Entity&, Entity&, AttackContext&, CombatLog&) {}
    virtual void onDefend(Entity&, Entity&, AttackContext&, CombatLog&) {}
};
