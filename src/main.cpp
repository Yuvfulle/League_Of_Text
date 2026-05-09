#include "items.h"
#include "abilities.h"

namespace {
using AbilityFactory = std::unique_ptr<Ability> (*)(int);

bool readChoice(const std::string& prompt, int& choice) {
    std::cout << prompt;
    if (std::cin >> choice) return true;
    std::cout << "\nInput stopped.\n";
    return false;
}

bool chooseAbilities(Entity& entity, const std::string& title, const std::string& menu, AbilityFactory create) {
    std::cout << title << menu;
    for (int i = 0; i < 2; i++) {
        int c;
        if (!readChoice("Choice " + std::to_string(i + 1) + " (0=skip): ", c)) return false;
        if (c == 0) break;
        if (auto ability = create(c)) entity.addAbility(std::move(ability));
    }
    return true;
}

bool buyItems(Entity& entity, const std::string& title, int& gold, CombatLog& log, const std::string& tag) {
    std::cout << "\n" << title << " (gold: " << gold << ")\n";
    printShopMenu();
    for (int i = 0; i < 6; i++) {
        if (gold < 300) { std::cout << "Not enough gold!\n"; break; }
        int c;
        std::string prompt = std::string(tag == "[Boss]" ? "Boss item " : "Item ") + std::to_string(i + 1) + " (0=skip): ";
        if (!readChoice(prompt, c)) {
            return false;
        }
        if (c == 0) continue;
        if (auto item = createItem(c)) {
            entity.buyItem(std::move(item), gold, log, tag);
            log.flush();
        }
    }
    return true;
}
}

int main() {
    std::srand((unsigned)std::time(nullptr));
    CombatLog log;
    Player player;
    Mob boss(true);

    const std::string activeMenu =
        "1. FIGHT_OR_BE_FORGOTTEN  2. BLOOD_DRAIN  3. OBLITERATE\n"
        "4. JUDGEMENT              5. DOMINUS\n";
    const std::string passiveMenu =
        "1. CORROSION  2. REAPER  3. SOUL_EATER (+10% OV)\n"
        "4. STRENGTH_ABOVE_ALL_ELSE  5. BATTLE_FURY\n";

    std::cout << "CHARACTER CREATION\n\n";
    if (!chooseAbilities(player, "Active Abilities (up to 2):\n", activeMenu, createActiveAbility)) return 0;
    if (!chooseAbilities(player, "\nPassive Abilities (up to 2):\n", passiveMenu, createPassiveAbility)) return 0;
    if (!chooseAbilities(boss, "\nBoss Actives (up to 2):\n", activeMenu, createActiveAbility)) return 0;
    if (!chooseAbilities(boss, "Boss Passives (up to 2):\n", passiveMenu, createPassiveAbility)) return 0;

    int pGold = 1800;
    int bGold = 1800;
    if (!buyItems(player, "YOUR SHOP", pGold, log, "[Shop]")) return 0;
    if (!buyItems(boss, "BOSS SHOP", bGold, log, "[Boss]")) return 0;

    int turn = 0;
    std::cout << "\n=== BOSS FIGHT ===\n";

    while (player.isAlive() && boss.isAlive()) {
        turn++;
        std::cout << "\n--- TURN " << turn << " ---\n";
        player.showStats(); std::cout << "\n"; boss.showStats(); std::cout << "\n";

        if (player.turnBlocked > 0) {
            std::cout << "  [Player] Stunned!\n"; player.turnBlocked--;
        } else {
            bool done = false;
            while (!done) {
                auto actives = player.getActiveAbilities();
                std::cout << "1. Attack\n";
                for (std::size_t i = 0; i < actives.size(); i++) {
                    int cd = player.getCooldown(actives[i]->getName());
                    std::cout << (i+2) << ". " << actives[i]->getName()
                         << (cd > 0 ? " [CD:"+std::to_string(cd)+"]" : " [RDY]") << "\n";
                }
                int ch;
                if (!readChoice("Choice: ", ch)) return 0;

                if (ch == 1) {
                    player.attack(boss, log); log.flush(); done = true;
                } else if (ch >= 2 && ch <= (int)actives.size()+1) {
                    auto* sel = actives[ch-2];
                    if (player.useAbility(sel, boss, "Player", log)) {
                        log.flush();
                        if (sel->isToggle()) { player.attack(boss, log); log.flush(); }
                        done = true;
                    } else { log.flush(); }
                }
            }
        }

        if (!boss.isAlive()) break;
        if (!player.isAlive()) break;

        if (boss.turnBlocked > 0) {
            std::cout << "  [Boss] Stunned!\n"; boss.turnBlocked--;
        } else {
            bool acted = false;
            auto bossAct = boss.getActiveAbilities();
            if (!bossAct.empty() && std::rand() % 2 == 0) {
                for (auto* a : bossAct) {
                    if (boss.getCooldown(a->getName()) == 0 &&
                        boss.useAbility(a, player, "Enemy", log)) {
                        log.flush();
                        if (a->isToggle()) { boss.attack(player, log); log.flush(); }
                        acted = true;
                        break;
                    }
                }
            }
            if (!acted) { boss.attack(player, log); log.flush(); }
        }

        std::cout << "\nEnd of turn\n";
        player.endTurn(boss, log); boss.endTurn(player, log); log.flush();
    }

    std::cout << (player.isAlive() ? "\n*** VICTORY! ***\n" : "\n*** DEFEAT ***\n");
}
