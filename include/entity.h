#pragma once
#include "buff.h"
#include "ability.h"
#include "item.h"

class Entity {
    double hp;
    std::array<double, statIndex(Stat::COUNT)> base{};
    std::array<double, statIndex(Stat::COUNT)> bonus{};

    std::vector<std::unique_ptr<Item>> items;
    std::vector<std::unique_ptr<Ability>> abilities;
    std::vector<std::unique_ptr<Buff>> buffs;
    std::map<std::string, int> cooldowns;
    std::map<std::string, bool> cooldownStartedThisTurn;

public:
    int turnBlocked = 0;

    Entity(double hp, double atk, double ap, double armor, double mr);
    virtual ~Entity() = default;

    void addStat(Stat s, double v);
    void setStatMax(Stat s, double v);

    double get(Stat s) const;
    double getATK() const;
    double getAP() const;
    double getMaxHP() const;
    double getArmor() const;
    double getMR() const;

    double getHealReduction() const;

    double getHP() const;
    void setHP(double v);
    bool isAlive() const;

    void applyHeal(double amount);

    static double calcPen(double def, double pctPen, double flatPen);
    double penMR(const Entity& t) const;
    double penArmor(const Entity& t) const;

    void addBuff(std::unique_ptr<Buff> b, CombatLog& log);
    void addBuffSilent(std::unique_ptr<Buff> b);
    void removeBuff(const std::string& id);
    Buff* getBuff(const std::string& id) const;

    void addAbility(std::unique_ptr<Ability> a);
    std::vector<Ability*> getActiveAbilities() const;
    std::vector<Ability*> getPassiveAbilities() const;

    int getCooldown(const std::string& s) const;

    void dealDamage(Entity& target, double rawDmg, bool magic, CombatLog& log);
    void performAttack(Entity& target, CombatLog& log, const std::string& self, const std::string& tgt);

    double takeDmgSimple(double rawDmg, double def);

    bool buyItem(std::unique_ptr<Item> item, int& gold, CombatLog& log, const std::string& tag);
    bool useAbility(Ability* a, Entity& tgt, const std::string& lbl, CombatLog& log);

    void endTurn(Entity& opp, CombatLog& log);
    void showStats(const std::string& label) const;
};

class Player : public Entity {
public:
    Player();
    void attack(Entity& t, CombatLog& log);
    void showStats() const;
};

class Mob : public Entity {
public:
    explicit Mob(bool boss);
    void attack(Entity& t, CombatLog& log);
    void showStats() const;
};
