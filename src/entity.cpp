#include "entity.h"

Entity::Entity(double hp, double atk, double ap, double armor, double mr) : hp(hp) {
    base[statIndex(Stat::MAX_HP)] = hp; base[statIndex(Stat::ATK)] = atk;
    base[statIndex(Stat::AP)] = ap;     base[statIndex(Stat::ARMOR)] = armor;
    base[statIndex(Stat::MAGIC_RESIST)] = mr;
}

void Entity::addStat(Stat s, double v) { bonus[statIndex(s)] += v; }
void Entity::setStatMax(Stat s, double v) { bonus[statIndex(s)] = std::max(bonus[statIndex(s)], v); }

double Entity::get(Stat s) const {
    double val = base[statIndex(s)] + bonus[statIndex(s)];
    for (auto& b : buffs) val += b->statMod(s, *this);

    if (s == Stat::ARMOR) {
        double shred = 0;
        for (auto& b : buffs) shred = std::max(shred, b->armorShred());
        val *= (1.0 - shred);
    }
    return std::max(0.0, val);
}

double Entity::getATK() const { return get(Stat::ATK); }
double Entity::getAP() const { return get(Stat::AP); }
double Entity::getMaxHP() const { return get(Stat::MAX_HP); }
double Entity::getArmor() const { return get(Stat::ARMOR); }
double Entity::getMR() const { return get(Stat::MAGIC_RESIST); }

double Entity::getHealReduction() const {
    double r = 0;
    for (auto& b : buffs) r = std::max(r, b->healReduction());
    return r;
}

double Entity::getHP() const { return hp; }
void Entity::setHP(double v) { hp = std::max(0.0, std::min(getMaxHP(), v)); }
bool Entity::isAlive() const { return hp > 0.1; }
void Entity::applyHeal(double amount) { setHP(hp + amount * (1.0 + get(Stat::HEAL_AMP)) * (1.0 - getHealReduction())); }
double Entity::calcPen(double def, double pctPen, double flatPen) { return std::max(0.0, def * (1.0 - pctPen) - flatPen); }
double Entity::penMR(const Entity& t) const { return calcPen(t.getMR(), get(Stat::MAG_PEN_PCT), get(Stat::MAG_PEN_FLAT)); }
double Entity::penArmor(const Entity& t) const { return calcPen(t.getArmor(), get(Stat::ARM_PEN_PCT), get(Stat::ARM_PEN_FLAT)); }

void Entity::addBuff(std::unique_ptr<Buff> b, CombatLog& log) {
    for (auto& ex : buffs) {
        if (ex->id() == b->id()) {
            ex->merge(b.get(), *this, log);
            return;
        }
    }
    b->onAdded(*this, log);
    buffs.push_back(std::move(b));
}

void Entity::addBuffSilent(std::unique_ptr<Buff> b) {
    for (auto& ex : buffs) if (ex->id() == b->id()) return;
    buffs.push_back(std::move(b));
}

void Entity::removeBuff(const std::string& id) {
    buffs.erase(std::remove_if(buffs.begin(), buffs.end(),
        [&](auto& b) { return b->id() == id; }), buffs.end());
}

Buff* Entity::getBuff(const std::string& id) const {
    for (auto& b : buffs) if (b->id() == id) return b.get();
    return nullptr;
}

void Entity::addAbility(std::unique_ptr<Ability> a) { a->onEquip(*this); abilities.push_back(std::move(a)); }

std::vector<Ability*> Entity::getActiveAbilities() const {
    std::vector<Ability*> result;
    for (auto& a : abilities) if (a->canActivate()) result.push_back(a.get());
    return result;
}

std::vector<Ability*> Entity::getPassiveAbilities() const {
    std::vector<Ability*> result;
    for (auto& a : abilities) if (!a->canActivate()) result.push_back(a.get());
    return result;
}

int Entity::getCooldown(const std::string& s) const {
    auto it = cooldowns.find(s);
    return it != cooldowns.end() ? it->second : 0;
}

void Entity::dealDamage(Entity& target, double rawDmg, bool magic, CombatLog& log) {
    double def = magic ? penMR(target) : penArmor(target);
    double eff = rawDmg * calcResist(def);

    double delayFrac = 0;
    Buff* delaySrc = nullptr;
    for (auto& b : target.buffs) {
        double f = b->delayedDamageFraction(magic);
        if (f > delayFrac) { delayFrac = f; delaySrc = b.get(); }
    }

    // Мгновенная часть
    target.hp -= eff * (1.0 - delayFrac);
    if (target.hp < 0) target.hp = 0;

    if (delayFrac > 0 && delaySrc) {
        auto dot = delaySrc->createDelayedDOT(eff * delayFrac / 3.0);
        if (dot) target.addBuff(std::move(dot), log);
    }

    double ov = get(Stat::OMNIVAMP);
    if (ov > 0) {
        double before = hp;
        applyHeal(eff * ov);
        if (hp > before + 0.5)
            log.add("  [Omnivamp] +" + std::to_string((int)(hp - before))
                + " (" + std::to_string((int)(ov*100)) + "% of " + std::to_string((int)eff) + ")");
    }
}

void Entity::performAttack(Entity& target, CombatLog& log,
                           const std::string& selfLabel, const std::string& tgtLabel) {
    AttackContext ctx;
    ctx.selfLabel = selfLabel;
    ctx.targetLabel = tgtLabel;
    ctx.atk = getATK();
    ctx.finalPhysRaw = ctx.atk;

    for (auto& a : abilities) {
        auto mod = a->onPreAttack(*this);
        if (mod.isCrit) {
            ctx.finalPhysRaw *= mod.atkMul;
            ctx.isCrit = true;
            ctx.critChance = mod.critChance;
        }
    }

    for (auto& item : items) item->onHit(*this, target, ctx, log);

    double physEff = ctx.finalPhysRaw * calcResist(penArmor(target));
    dealDamage(target, ctx.finalPhysRaw, false, log);

    log.add("[" + selfLabel + " attacks " + tgtLabel + "]");
    if (ctx.isCrit) log.add("  CRITICAL! (x2, " + std::to_string((int)ctx.critChance) + "%)");
    std::string line = "  " + std::to_string((int)ctx.finalPhysRaw) + " raw -> " + std::to_string((int)physEff) + " eff";
    if (ctx.isCrit) line += " (CRIT)";
    log.add(line);
    log.add("  " + tgtLabel + " HP: " + std::to_string((int)target.getHP()) + "/" + std::to_string((int)target.getMaxHP()));

    for (auto& a : abilities) a->onPostHit(*this, target, ctx, log);

    double ls = get(Stat::LIFESTEAL);
    if (ls > 0) {
        double before = hp;
        applyHeal(ctx.finalPhysRaw * ls);
        log.add("  [Lifesteal] +" + std::to_string((int)(hp - before))
            + " (" + std::to_string((int)(ls*100)) + "% of " + std::to_string((int)ctx.finalPhysRaw) + ")");
    }

    for (auto& item : target.items) item->onDefend(target, *this, ctx, log);

    for (auto it = buffs.begin(); it != buffs.end(); ) {
        if ((*it)->attacksRemaining > 0) (*it)->attacksRemaining--;
        (*it)->isExpired() ? it = buffs.erase(it) : ++it;
    }

    for (auto& a : abilities) a->onAttackDone(*this, log);
}

double Entity::takeDmgSimple(double rawDmg, double def) {
    double delayed = 0;
    for (auto& b : buffs) delayed = std::max(delayed, b->delayedDamageFraction(true));
    double eff = rawDmg * calcResist(def);
    hp -= eff * (1.0 - delayed);
    if (hp < 0) hp = 0;
    return delayed > 0 ? eff * delayed / 3.0 : 0;
}

bool Entity::buyItem(std::unique_ptr<Item> item, int& gold, CombatLog& log, const std::string& tag) {
    if (gold < item->getPrice() || items.size() >= 6) return false;
    gold -= item->getPrice();
    std::string name = item->getName();
    item->onPurchase(*this);
    items.push_back(std::move(item));
    log.add(tag + " " + name + " (gold: " + std::to_string(gold) + ")");
    return true;
}

bool Entity::useAbility(Ability* a, Entity& tgt, const std::string& lbl, CombatLog& log) {
    if (!a) return false;
    int cd = getCooldown(a->getName());
    if (cd > 0) {
        log.add("[" + lbl + "] " + a->getName() + " CD:" + std::to_string(cd));
        return false;
    }
    if (!a->activate(*this, tgt, lbl, log)) return false;
    if (a->getCooldown() > 0) {
        cooldowns[a->getName()] = a->getCooldown();
        cooldownStartedThisTurn[a->getName()] = true;
    }
    return true;
}

void Entity::endTurn(Entity& opp, CombatLog& log) {
    for (auto& b : buffs) b->onTurnEnd(*this, opp, log);
    for (auto& [name, turns] : cooldowns) {
        if (turns > 0 && cooldownStartedThisTurn.find(name) == cooldownStartedThisTurn.end()) {
            turns--;
        }
    }
    cooldownStartedThisTurn.clear();
    for (auto it = buffs.begin(); it != buffs.end(); ) {
        if ((*it)->durationTurns > 0) (*it)->durationTurns--;
        (*it)->isExpired() ? it = buffs.erase(it) : ++it;
    }
    setHP(hp);
}

void Entity::showStats(const std::string& label) const {
    auto pct = [](double a, double b) { return b > 0 ? std::min(100.0, a / b * 100) : 0.0; };
    std::cout << label << " STATS\n";
    std::cout << "HP: " << (int)hp << "/" << (int)getMaxHP() << " (" << (int)pct(hp, getMaxHP()) << "%)\n";
    std::cout << "ATK:" << (int)getATK() << " AP:" << (int)getAP();

    auto show = [&](Stat s, const char* name) {
        double v = get(s);
        if (v > 0.001) std::cout << " " << name << ":" << (int)(v < 1 ? v * 100 : v) << (v < 1 ? "%" : "");
    };
    show(Stat::ARM_PEN_PCT, "ArmorPen"); show(Stat::ARM_PEN_FLAT, "Lethality");
    show(Stat::MAG_PEN_PCT, "MagPen");   show(Stat::MAG_PEN_FLAT, "MagPenFlat");
    show(Stat::LIFESTEAL, "LS");          show(Stat::OMNIVAMP, "OV");
    show(Stat::HEAL_AMP, "HealAmp");

    std::cout << "\nArmor:" << (int)getArmor() << "(" << (int)((1 - calcResist(getArmor())) * 100) << "% red)"
         << " MR:" << (int)getMR() << "(" << (int)((1 - calcResist(getMR())) * 100) << "% red)\n";

    auto act = getActiveAbilities();
    std::cout << "Actives: "; if (act.empty()) std::cout << "-";
    for (std::size_t i = 0; i < act.size(); i++) {
        int cd = getCooldown(act[i]->getName());
        std::cout << act[i]->getName() << (cd > 0 ? "[CD:" + std::to_string(cd) + "]" : "[RDY]");
        if (i + 1 < act.size()) std::cout << ", ";
    }
    auto pas = getPassiveAbilities();
    std::cout << "\nPassives: "; if (pas.empty()) std::cout << "-";
    for (std::size_t i = 0; i < pas.size(); i++) {
        std::cout << pas[i]->getName(); if (i + 1 < pas.size()) std::cout << ", ";
    }
    std::cout << "\n";
    if (turnBlocked > 0) std::cout << "STUNNED " << turnBlocked << "t\n";
    bool any = false;
    for (auto& b : buffs) {
        std::string s = b->getStats(); if (s.empty()) continue;
        if (!any) { std::cout << "Buffs: "; any = true; }
        std::cout << "[" << s;
        if (b->durationTurns > 0) std::cout << " " << b->durationTurns << "t";
        if (b->attacksRemaining > 0) std::cout << " " << b->attacksRemaining << "atk";
        std::cout << "] ";
    }
    if (any) std::cout << "\n";
}

Player::Player() : Entity(1200, 80, 120, 40, 35) {}
void Player::attack(Entity& t, CombatLog& log) { performAttack(t, log, "Player", "Enemy"); }
void Player::showStats() const { Entity::showStats("PLAYER"); }

Mob::Mob(bool boss) : Entity(
    boss ? 250000 : 1000,
    boss ? 100 : 50,
    boss ? 50 : 20,
    boss ? 60 : 30,
    boss ? 50 : 20) {}
void Mob::attack(Entity& t, CombatLog& log) { performAttack(t, log, "Enemy", "Player"); }
void Mob::showStats() const { Entity::showStats("ENEMY"); }
