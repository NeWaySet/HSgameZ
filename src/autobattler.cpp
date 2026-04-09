#include "autobattler.h"

#include <QJsonValue>
#include <QRandomGenerator>

#include <algorithm>

namespace
{
struct SimMinion
{
    AutoBattlerGame::MinionCard card;
    bool attackedThisCycle = false;
};

QString attackLine(const AutoBattlerGame::MinionCard &attacker, const AutoBattlerGame::MinionCard &target)
{
    return QString("%1 [%2/%3] атакует %4 [%5/%6]")
        .arg(attacker.name)
        .arg(attacker.attack)
        .arg(attacker.health)
        .arg(target.name)
        .arg(target.attack)
        .arg(target.health);
}

QString heroAttackLine(const AutoBattlerGame::MinionCard &attacker, const QString &heroName, int heroHealth)
{
    return QString("%1 [%2/%3] атакует героя %4 [здоровье %5]")
        .arg(attacker.name)
        .arg(attacker.attack)
        .arg(attacker.health)
        .arg(heroName)
        .arg(heroHealth);
}

void removeDead(QVector<SimMinion> &board, QStringList &log, const QString &sideName)
{
    for (int i = board.size() - 1; i >= 0; --i)
    {
        if (board[i].card.health <= 0)
        {
            log.append(QString("%1: %2 погибает").arg(sideName, board[i].card.name));
            board.removeAt(i);
        }
    }
}

bool allAttacked(const QVector<SimMinion> &board)
{
    return std::all_of(board.cbegin(), board.cend(), [](const SimMinion &minion) {
        return minion.attackedThisCycle;
    });
}

bool hasTaunt(const QVector<SimMinion> &board)
{
    return std::any_of(board.cbegin(), board.cend(), [](const SimMinion &minion) {
        return minion.card.taunt;
    });
}

QVector<int> tauntIndexes(const QVector<SimMinion> &board)
{
    QVector<int> indexes;
    for (int index = 0; index < board.size(); ++index)
    {
        if (board[index].card.taunt)
        {
            indexes.append(index);
        }
    }
    return indexes;
}

void resetCycleIfNeeded(QVector<SimMinion> &board)
{
    if (!board.isEmpty() && allAttacked(board))
    {
        for (SimMinion &minion : board)
        {
            minion.attackedThisCycle = false;
        }
    }
}

int nextAttackerIndex(const QVector<SimMinion> &board)
{
    for (int index = 0; index < board.size(); ++index)
    {
        if (!board[index].attackedThisCycle)
        {
            return index;
        }
    }
    return -1;
}
}

AutoBattlerGame::AutoBattlerGame()
{
    resetMatch(QStringLiteral("Ваш герой"), QStringLiteral("Соперник"));
}

void AutoBattlerGame::resetMatch(const QString &localHeroName, const QString &remoteHeroName)
{
    m_nextInstanceId = 1;
    resetPlayer(m_localPlayer, localHeroName);

    m_remotePlayer = {};
    m_remotePlayer.heroName = remoteHeroName;
    m_remotePlayer.heroHealth = 30;
    m_remotePlayer.round = 1;
    m_remotePlayer.ready = false;
}

void AutoBattlerGame::beginNextRound()
{
    beginNextRound(m_localPlayer);
}

void AutoBattlerGame::resetPlayer(PlayerState &player, const QString &heroName)
{
    player = {};
    player.hero.name = heroName;
    player.hero.health = 30;
    player.round = 1;
    player.maxGold = 3;
    player.gold = 3;
    player.ready = false;

    player.tavern.clear();
    player.tavern.reserve(7);
    for (int i = 0; i < 7; ++i)
    {
        player.tavern.append(drawRandomCard());
    }
}

void AutoBattlerGame::beginNextRound(PlayerState &player)
{
    player.round += 1;
    player.maxGold = std::min(10, player.round + 2);
    player.gold = player.maxGold;
    player.ready = false;

    player.tavern.clear();
    player.tavern.reserve(7);
    for (int i = 0; i < 7; ++i)
    {
        player.tavern.append(drawRandomCard());
    }
}

bool AutoBattlerGame::refreshTavern(QString *errorMessage)
{
    return refreshTavern(m_localPlayer, errorMessage);
}

bool AutoBattlerGame::refreshTavern(PlayerState &player, QString *errorMessage)
{
    if (player.ready)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Во время ожидания боя таверну обновлять нельзя.");
        }
        return false;
    }

    if (player.gold < 1)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Недостаточно монет для обновления таверны.");
        }
        return false;
    }

    player.gold -= 1;
    player.tavern.clear();
    player.tavern.reserve(7);
    for (int i = 0; i < 7; ++i)
    {
        player.tavern.append(drawRandomCard());
    }
    return true;
}

bool AutoBattlerGame::buyFromTavern(int index, QString *errorMessage)
{
    return buyFromTavern(m_localPlayer, index, errorMessage);
}

bool AutoBattlerGame::buyFromTavern(PlayerState &player, int index, QString *errorMessage)
{
    if (player.ready)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Сначала снимите готовность.");
        }
        return false;
    }

    if (index < 0 || index >= player.tavern.size())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Выберите карту в таверне.");
        }
        return false;
    }

    if (player.hand.size() >= 10)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Рука переполнена.");
        }
        return false;
    }

    const MinionCard card = player.tavern.at(index);
    if (player.gold < card.cost)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Недостаточно монет для покупки.");
        }
        return false;
    }

    player.gold -= card.cost;
    player.hand.append(card);
    player.tavern.removeAt(index);
    return true;
}

bool AutoBattlerGame::playFromHand(int index, QString *errorMessage)
{
    return playFromHand(m_localPlayer, index, errorMessage);
}

bool AutoBattlerGame::playFromHand(PlayerState &player, int index, QString *errorMessage)
{
    if (player.ready)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Во время ожидания боя менять стол нельзя.");
        }
        return false;
    }

    if (index < 0 || index >= player.hand.size())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Выберите карту в руке.");
        }
        return false;
    }

    if (player.board.size() >= 7)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Стол заполнен.");
        }
        return false;
    }

    player.board.append(player.hand.at(index));
    player.hand.removeAt(index);
    return true;
}

bool AutoBattlerGame::sellFromBoard(int index, QString *errorMessage)
{
    return sellFromBoard(m_localPlayer, index, errorMessage);
}

bool AutoBattlerGame::sellFromBoard(PlayerState &player, int index, QString *errorMessage)
{
    if (player.ready)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Сначала снимите готовность.");
        }
        return false;
    }

    if (index < 0 || index >= player.board.size())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Выберите существо на столе.");
        }
        return false;
    }

    player.board.removeAt(index);
    player.gold = std::min(player.maxGold, player.gold + 1);
    return true;
}

void AutoBattlerGame::setLocalReady(bool ready)
{
    setPlayerReady(m_localPlayer, ready);
}

void AutoBattlerGame::setPlayerReady(PlayerState &player, bool ready)
{
    player.ready = ready;
}

AutoBattlerGame::PlayerState &AutoBattlerGame::localPlayer()
{
    return m_localPlayer;
}

const AutoBattlerGame::PlayerState &AutoBattlerGame::localPlayer() const
{
    return m_localPlayer;
}

const AutoBattlerGame::PlayerSnapshot &AutoBattlerGame::remotePlayer() const
{
    return m_remotePlayer;
}

void AutoBattlerGame::setRemotePlayer(const PlayerSnapshot &snapshot)
{
    m_remotePlayer = snapshot;
}

void AutoBattlerGame::setRemoteReady(bool ready)
{
    m_remotePlayer.ready = ready;
}

void AutoBattlerGame::setLocalHeroHealth(int health)
{
    m_localPlayer.hero.health = health;
}

void AutoBattlerGame::setRemoteHeroHealth(int health)
{
    m_remotePlayer.heroHealth = health;
}

AutoBattlerGame::PlayerSnapshot AutoBattlerGame::makeLocalSnapshot() const
{
    return makeSnapshot(m_localPlayer);
}

AutoBattlerGame::PlayerSnapshot AutoBattlerGame::makeSnapshot(const PlayerState &player) const
{
    PlayerSnapshot snapshot;
    snapshot.heroName = player.hero.name;
    snapshot.heroHealth = player.hero.health;
    snapshot.round = player.round;
    snapshot.ready = player.ready;
    snapshot.board = player.board;
    return snapshot;
}

AutoBattlerGame::BattleResult AutoBattlerGame::resolveBattle(const PlayerSnapshot &hostPlayer,
                                                             const PlayerSnapshot &clientPlayer)
{
    BattleResult result;
    result.hostHeroHealth = hostPlayer.heroHealth;
    result.clientHeroHealth = clientPlayer.heroHealth;

    QVector<SimMinion> hostBoard;
    QVector<SimMinion> clientBoard;
    hostBoard.reserve(hostPlayer.board.size());
    clientBoard.reserve(clientPlayer.board.size());

    for (const MinionCard &card : hostPlayer.board)
    {
        hostBoard.append({card, false});
    }

    for (const MinionCard &card : clientPlayer.board)
    {
        clientBoard.append({card, false});
    }

    bool hostTurn = hostBoard.size() >= clientBoard.size();

    while (!hostBoard.isEmpty() && !clientBoard.isEmpty())
    {
        QVector<SimMinion> &attackers = hostTurn ? hostBoard : clientBoard;
        QVector<SimMinion> &defenders = hostTurn ? clientBoard : hostBoard;
        int &defenderHeroHealth = hostTurn ? result.clientHeroHealth : result.hostHeroHealth;
        const QString defenderHeroName = hostTurn ? clientPlayer.heroName : hostPlayer.heroName;

        resetCycleIfNeeded(attackers);
        const int attackerIndex = nextAttackerIndex(attackers);
        if (attackerIndex < 0)
        {
            hostTurn = !hostTurn;
            continue;
        }

        const bool defendersHaveTaunt = hasTaunt(defenders);
        const bool canHitHero = !defendersHaveTaunt;
        const bool attackHero = canHitHero
            && QRandomGenerator::global()->bounded(defenders.size() + 1) == defenders.size();

        attackers[attackerIndex].attackedThisCycle = true;

        if (attackHero)
        {
            result.log.append(heroAttackLine(attackers[attackerIndex].card, defenderHeroName, defenderHeroHealth));
            defenderHeroHealth = std::max(0, defenderHeroHealth - attackers[attackerIndex].card.attack);
            result.log.append(QStringLiteral("%1 получает %2 урона и остаётся с %3 здоровья.")
                                  .arg(defenderHeroName)
                                  .arg(attackers[attackerIndex].card.attack)
                                  .arg(defenderHeroHealth));

            if (defenderHeroHealth <= 0)
            {
                result.log.append(QStringLiteral("Герой %1 пал прямо во время боя.").arg(defenderHeroName));
                break;
            }
        }
        else
        {
            int targetIndex = 0;
            if (defendersHaveTaunt)
            {
                const QVector<int> taunts = tauntIndexes(defenders);
                targetIndex = taunts.at(QRandomGenerator::global()->bounded(taunts.size()));
            }
            else
            {
                targetIndex = QRandomGenerator::global()->bounded(defenders.size());
            }

            result.log.append(attackLine(attackers[attackerIndex].card, defenders[targetIndex].card));

            defenders[targetIndex].card.health -= attackers[attackerIndex].card.attack;
            attackers[attackerIndex].card.health -= defenders[targetIndex].card.attack;
        }

        removeDead(hostBoard, result.log, QStringLiteral("Хост"));
        removeDead(clientBoard, result.log, QStringLiteral("Клиент"));

        if (hostBoard.isEmpty() || clientBoard.isEmpty() || result.hostHeroHealth <= 0 || result.clientHeroHealth <= 0)
        {
            break;
        }

        hostTurn = !hostTurn;
    }

    if (result.hostHeroHealth <= 0 && result.clientHeroHealth <= 0)
    {
        result.draw = true;
        result.log.append(QStringLiteral("Оба героя пали. Ничья."));
    }
    else if (result.clientHeroHealth <= 0)
    {
        result.hostWon = true;
        result.log.append(QStringLiteral("Хост побеждает, потому что герой клиента повержен."));
    }
    else if (result.hostHeroHealth <= 0)
    {
        result.clientWon = true;
        result.log.append(QStringLiteral("Клиент побеждает, потому что герой хоста повержен."));
    }
    else if (hostBoard.isEmpty() && clientBoard.isEmpty())
    {
        result.draw = true;
        result.log.append(QStringLiteral("Оба стола опустели. Ничья."));
    }
    else if (clientBoard.isEmpty())
    {
        result.hostWon = true;
        const int damage = hostBoard.size();
        result.clientHeroHealth = std::max(0, result.clientHeroHealth - damage);
        result.log.append(QStringLiteral("Хост выигрывает бой и наносит %1 урона герою клиента.").arg(damage));
    }
    else if (hostBoard.isEmpty())
    {
        result.clientWon = true;
        const int damage = clientBoard.size();
        result.hostHeroHealth = std::max(0, result.hostHeroHealth - damage);
        result.log.append(QStringLiteral("Клиент выигрывает бой и наносит %1 урона герою хоста.").arg(damage));
    }

    result.gameOver = result.hostHeroHealth <= 0 || result.clientHeroHealth <= 0;
    if (result.gameOver)
    {
        result.log.append(QStringLiteral("Матч завершён."));
    }

    return result;
}

QString AutoBattlerGame::formatCard(const MinionCard &card)
{
    return QString("%1%2 | %3/%4 | цена %5")
        .arg(card.name)
        .arg(card.taunt ? QStringLiteral(" | Провокация") : QString())
        .arg(card.attack)
        .arg(card.health)
        .arg(card.cost);
}

QString AutoBattlerGame::formatBoardCard(const MinionCard &card)
{
    return QString("%1%2 | %3/%4")
        .arg(card.name)
        .arg(card.taunt ? QStringLiteral(" | Провокация") : QString())
        .arg(card.attack)
        .arg(card.health);
}

QJsonObject AutoBattlerGame::snapshotToJson(const PlayerSnapshot &snapshot)
{
    QJsonObject object;
    object.insert(QStringLiteral("heroName"), snapshot.heroName);
    object.insert(QStringLiteral("heroHealth"), snapshot.heroHealth);
    object.insert(QStringLiteral("round"), snapshot.round);
    object.insert(QStringLiteral("ready"), snapshot.ready);
    object.insert(QStringLiteral("board"), cardsToJson(snapshot.board));
    return object;
}

AutoBattlerGame::PlayerSnapshot AutoBattlerGame::snapshotFromJson(const QJsonObject &object)
{
    PlayerSnapshot snapshot;
    snapshot.heroName = object.value(QStringLiteral("heroName")).toString(QStringLiteral("Соперник"));
    snapshot.heroHealth = object.value(QStringLiteral("heroHealth")).toInt(30);
    snapshot.round = object.value(QStringLiteral("round")).toInt(1);
    snapshot.ready = object.value(QStringLiteral("ready")).toBool(false);
    snapshot.board = cardsFromJson(object.value(QStringLiteral("board")).toArray());
    return snapshot;
}

QJsonObject AutoBattlerGame::battleResultToJson(const BattleResult &result)
{
    QJsonObject object;
    object.insert(QStringLiteral("hostHeroHealth"), result.hostHeroHealth);
    object.insert(QStringLiteral("clientHeroHealth"), result.clientHeroHealth);
    object.insert(QStringLiteral("hostWon"), result.hostWon);
    object.insert(QStringLiteral("clientWon"), result.clientWon);
    object.insert(QStringLiteral("draw"), result.draw);
    object.insert(QStringLiteral("gameOver"), result.gameOver);

    QJsonArray log;
    for (const QString &line : result.log)
    {
        log.append(line);
    }
    object.insert(QStringLiteral("log"), log);
    return object;
}

AutoBattlerGame::BattleResult AutoBattlerGame::battleResultFromJson(const QJsonObject &object)
{
    BattleResult result;
    result.hostHeroHealth = object.value(QStringLiteral("hostHeroHealth")).toInt(30);
    result.clientHeroHealth = object.value(QStringLiteral("clientHeroHealth")).toInt(30);
    result.hostWon = object.value(QStringLiteral("hostWon")).toBool(false);
    result.clientWon = object.value(QStringLiteral("clientWon")).toBool(false);
    result.draw = object.value(QStringLiteral("draw")).toBool(false);
    result.gameOver = object.value(QStringLiteral("gameOver")).toBool(false);

    const QJsonArray log = object.value(QStringLiteral("log")).toArray();
    for (const QJsonValue &value : log)
    {
        result.log.append(value.toString());
    }
    return result;
}

QVector<AutoBattlerGame::BaseTemplate> AutoBattlerGame::cardPool() const
{
    return {
        {QStringLiteral("Тавернный громила"), 4, 4, 3, false},
        {QStringLiteral("Лунный разведчик"), 2, 5, 3, false},
        {QStringLiteral("Железный голем"), 5, 3, 4, true},
        {QStringLiteral("Пироман"), 6, 2, 4, false},
        {QStringLiteral("Лесной охотник"), 3, 3, 2, false},
        {QStringLiteral("Болотный великан"), 7, 6, 5, true},
        {QStringLiteral("Механический паук"), 2, 2, 1, false},
        {QStringLiteral("Щитоносец таверны"), 1, 6, 2, true},
        {QStringLiteral("Боевой волк"), 4, 2, 2, false},
        {QStringLiteral("Штормовой адепт"), 3, 4, 3, false},
        {QStringLiteral("Дракончик пепла"), 5, 5, 5, false},
        {QStringLiteral("Кобольд-подрывник"), 6, 1, 2, false}
    };
}

AutoBattlerGame::MinionCard AutoBattlerGame::drawRandomCard()
{
    const QVector<BaseTemplate> pool = cardPool();
    const BaseTemplate &base = pool.at(QRandomGenerator::global()->bounded(pool.size()));

    MinionCard card;
    card.instanceId = m_nextInstanceId++;
    card.name = base.name;
    card.attack = base.attack;
    card.health = base.health;
    card.cost = base.cost;
    card.taunt = base.taunt;
    return card;
}

void AutoBattlerGame::refillTavern()
{
    m_localPlayer.tavern.clear();
    m_localPlayer.tavern.reserve(7);
    for (int i = 0; i < 7; ++i)
    {
        m_localPlayer.tavern.append(drawRandomCard());
    }
}

QJsonObject AutoBattlerGame::cardToJson(const MinionCard &card)
{
    QJsonObject object;
    object.insert(QStringLiteral("instanceId"), card.instanceId);
    object.insert(QStringLiteral("name"), card.name);
    object.insert(QStringLiteral("attack"), card.attack);
    object.insert(QStringLiteral("health"), card.health);
    object.insert(QStringLiteral("cost"), card.cost);
    object.insert(QStringLiteral("taunt"), card.taunt);
    return object;
}

AutoBattlerGame::MinionCard AutoBattlerGame::cardFromJson(const QJsonObject &object)
{
    MinionCard card;
    card.instanceId = object.value(QStringLiteral("instanceId")).toInt(0);
    card.name = object.value(QStringLiteral("name")).toString();
    card.attack = object.value(QStringLiteral("attack")).toInt(0);
    card.health = object.value(QStringLiteral("health")).toInt(0);
    card.cost = object.value(QStringLiteral("cost")).toInt(3);
    card.taunt = object.value(QStringLiteral("taunt")).toBool(false);
    return card;
}

QJsonArray AutoBattlerGame::cardsToJson(const QVector<MinionCard> &cards)
{
    QJsonArray array;
    for (const MinionCard &card : cards)
    {
        array.append(cardToJson(card));
    }
    return array;
}

QVector<AutoBattlerGame::MinionCard> AutoBattlerGame::cardsFromJson(const QJsonArray &array)
{
    QVector<MinionCard> cards;
    cards.reserve(array.size());
    for (const QJsonValue &value : array)
    {
        cards.append(cardFromJson(value.toObject()));
    }
    return cards;
}
