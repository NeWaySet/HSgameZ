#pragma once

#include "autobattler.h"
#include "networkmanager.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>

#include <array>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void hostSession();
    void connectToSession();
    void startSelfGame();
    void switchSelfPlayer();
    void revealSelfTurn();
    void refreshTavern();
    void buySelectedCard();
    void playSelectedCard();
    void sellSelectedMinion();
    void toggleReady();
    void onConnected();
    void onDisconnected();
    void onNetworkMessage(const QJsonObject &object);
    void showError(const QString &message);
    void showInfo(const QString &message);
    void updateTransportUi();

private:
    enum class UiTransport
    {
        Tcp = 0,
        Bluetooth = 1
    };

    enum class SessionMode
    {
        Network = 0,
        SelfPlay = 1
    };

    void setupUi();
    void updateUi();
    void appendLog(const QString &message);
    void appendBattleLog(const QStringList &lines);
    void syncLocalState();
    void resolveBattleIfHost();
    void resolveSelfBattleIfReady();
    void applyBattleResult(const AutoBattlerGame::BattleResult &result);
    void applySelfBattleResult(const AutoBattlerGame::BattleResult &result);
    UiTransport selectedTransport() const;
    SessionMode selectedMode() const;
    bool isSelfPlayMode() const;
    AutoBattlerGame::PlayerState &currentSelfPlayer();
    const AutoBattlerGame::PlayerState &currentSelfPlayer() const;
    AutoBattlerGame::PlayerState &otherSelfPlayer();
    const AutoBattlerGame::PlayerState &otherSelfPlayer() const;

    AutoBattlerGame m_game;
    NetworkManager m_network;
    std::array<AutoBattlerGame::PlayerState, 2> m_selfPlayers {};

    QComboBox *m_modeBox = nullptr;
    QComboBox *m_transportBox = nullptr;
    QLineEdit *m_addressEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QPushButton *m_hostButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_localGameButton = nullptr;
    QPushButton *m_switchPlayerButton = nullptr;
    QPushButton *m_revealTurnButton = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_roundLabel = nullptr;
    QLabel *m_goldLabel = nullptr;
    QLabel *m_localHeroLabel = nullptr;
    QLabel *m_remoteHeroLabel = nullptr;
    QLabel *m_activePlayerLabel = nullptr;
    QListWidget *m_tavernList = nullptr;
    QListWidget *m_handList = nullptr;
    QListWidget *m_localBoardList = nullptr;
    QListWidget *m_remoteBoardList = nullptr;
    QTextEdit *m_battleLog = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_buyButton = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_sellButton = nullptr;
    QPushButton *m_readyButton = nullptr;

    bool m_selfPlayActive = false;
    int m_activeSelfPlayer = 0;
    bool m_selfViewLocked = false;
};
