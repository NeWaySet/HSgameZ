#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class AutoBattlerGame
{
public:
    struct MinionCard
    {
        int instanceId = 0;
        QString name;
        int attack = 0;
        int health = 0;
        int cost = 3;
    };

    struct HeroState
    {
        QString name;
        int health = 30;
    };

    struct PlayerState
    {
        HeroState hero;
        int round = 1;
        int maxGold = 3;
        int gold = 3;
        bool ready = false;
        QVector<MinionCard> tavern;
        QVector<MinionCard> hand;
        QVector<MinionCard> board;
    };

    struct PlayerSnapshot
    {
        QString heroName;
        int heroHealth = 30;
        int round = 1;
        bool ready = false;
        QVector<MinionCard> board;
    };

    struct BattleResult
    {
        QStringList log;
        int hostHeroHealth = 30;
        int clientHeroHealth = 30;
        bool hostWon = false;
        bool clientWon = false;
        bool draw = false;
        bool gameOver = false;
    };

    AutoBattlerGame();

    void resetMatch(const QString &localHeroName, const QString &remoteHeroName);
    void beginNextRound();

    bool refreshTavern(QString *errorMessage = nullptr);
    bool buyFromTavern(int index, QString *errorMessage = nullptr);
    bool playFromHand(int index, QString *errorMessage = nullptr);
    bool sellFromBoard(int index, QString *errorMessage = nullptr);
    void setLocalReady(bool ready);

    PlayerState &localPlayer();
    const PlayerState &localPlayer() const;
    const PlayerSnapshot &remotePlayer() const;
    void setRemotePlayer(const PlayerSnapshot &snapshot);
    void setRemoteReady(bool ready);
    void setLocalHeroHealth(int health);
    void setRemoteHeroHealth(int health);

    PlayerSnapshot makeLocalSnapshot() const;

    static BattleResult resolveBattle(const PlayerSnapshot &hostPlayer, const PlayerSnapshot &clientPlayer);
    static QString formatCard(const MinionCard &card);
    static QString formatBoardCard(const MinionCard &card);
    static QJsonObject snapshotToJson(const PlayerSnapshot &snapshot);
    static PlayerSnapshot snapshotFromJson(const QJsonObject &object);
    static QJsonObject battleResultToJson(const BattleResult &result);
    static BattleResult battleResultFromJson(const QJsonObject &object);

private:
    struct BaseTemplate
    {
        QString name;
        int attack = 0;
        int health = 0;
        int cost = 3;
    };

    QVector<BaseTemplate> cardPool() const;
    MinionCard drawRandomCard();
    void refillTavern();

    static QJsonObject cardToJson(const MinionCard &card);
    static MinionCard cardFromJson(const QJsonObject &object);
    static QJsonArray cardsToJson(const QVector<MinionCard> &cards);
    static QVector<MinionCard> cardsFromJson(const QJsonArray &array);

    PlayerState m_localPlayer;
    PlayerSnapshot m_remotePlayer;
    int m_nextInstanceId = 1;
};
