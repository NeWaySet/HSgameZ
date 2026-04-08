#include "mainwindow.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    connect(&m_network, &NetworkManager::connected, this, &MainWindow::onConnected);
    connect(&m_network, &NetworkManager::disconnected, this, &MainWindow::onDisconnected);
    connect(&m_network, &NetworkManager::messageReceived, this, &MainWindow::onNetworkMessage);
    connect(&m_network, &NetworkManager::errorOccurred, this, &MainWindow::showError);
    connect(&m_network, &NetworkManager::info, this, &MainWindow::showInfo);

    updateTransportUi();
    updateUi();
    appendLog(QStringLiteral("Подключитесь к сопернику и соберите стол через таверну."));
}

void MainWindow::hostSession()
{
    const UiTransport transport = selectedTransport();
    const bool started = transport == UiTransport::Tcp
        ? m_network.startTcpServer(static_cast<quint16>(m_portSpin->value()))
        : m_network.startBluetoothServer(QStringLiteral("Qt Battlegrounds"));

    if (started)
    {
        appendLog(transport == UiTransport::Tcp
                      ? QStringLiteral("Ожидание TCP подключения.")
                      : QStringLiteral("Ожидание Bluetooth подключения."));
    }

    updateUi();
}

void MainWindow::connectToSession()
{
    if (selectedTransport() == UiTransport::Tcp)
    {
        m_network.connectTcp(m_addressEdit->text().trimmed(), static_cast<quint16>(m_portSpin->value()));
    }
    else
    {
        m_network.connectBluetooth(m_addressEdit->text().trimmed());
    }

    updateUi();
}

void MainWindow::refreshTavern()
{
    QString error;
    if (!m_game.refreshTavern(&error))
    {
        showError(error);
        return;
    }

    appendLog(QStringLiteral("Таверна обновлена за 1 монету."));
    updateUi();
    syncLocalState();
}

void MainWindow::buySelectedCard()
{
    QString error;
    if (!m_game.buyFromTavern(m_tavernList->currentRow(), &error))
    {
        showError(error);
        return;
    }

    appendLog(QStringLiteral("Существо куплено в руку."));
    updateUi();
    syncLocalState();
}

void MainWindow::playSelectedCard()
{
    QString error;
    if (!m_game.playFromHand(m_handList->currentRow(), &error))
    {
        showError(error);
        return;
    }

    appendLog(QStringLiteral("Существо выставлено на стол."));
    updateUi();
    syncLocalState();
}

void MainWindow::sellSelectedMinion()
{
    QString error;
    if (!m_game.sellFromBoard(m_localBoardList->currentRow(), &error))
    {
        showError(error);
        return;
    }

    appendLog(QStringLiteral("Существо продано за 1 монету."));
    updateUi();
    syncLocalState();
}

void MainWindow::toggleReady()
{
    if (!m_network.isConnected())
    {
        showError(QStringLiteral("Сначала подключитесь к сопернику."));
        return;
    }

    const bool newReadyState = !m_game.localPlayer().ready;
    m_game.setLocalReady(newReadyState);

    appendLog(newReadyState
                  ? QStringLiteral("Готовность подтверждена. Ожидание соперника.")
                  : QStringLiteral("Готовность снята. Можно продолжать покупки."));

    updateUi();
    syncLocalState();
    resolveBattleIfHost();
}

void MainWindow::onConnected()
{
    const QString heroName = m_network.isHosting()
        ? QStringLiteral("Хозяин таверны")
        : QStringLiteral("Искатель славы");

    m_game.resetMatch(heroName, QStringLiteral("Соперник"));
    m_battleLog->clear();
    appendLog(QStringLiteral("Соединение установлено. Раунд 1 начинается с 3 монет."));
    updateUi();
    syncLocalState();
}

void MainWindow::onDisconnected()
{
    m_game.setRemoteReady(false);
    appendLog(QStringLiteral("Соединение разорвано."));
    updateUi();
}

void MainWindow::onNetworkMessage(const QJsonObject &object)
{
    const QString type = object.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("playerState"))
    {
        const AutoBattlerGame::PlayerSnapshot snapshot =
            AutoBattlerGame::snapshotFromJson(object.value(QStringLiteral("payload")).toObject());
        m_game.setRemotePlayer(snapshot);
        updateUi();
        resolveBattleIfHost();
        return;
    }

    if (type == QLatin1String("battleResult"))
    {
        const AutoBattlerGame::BattleResult result =
            AutoBattlerGame::battleResultFromJson(object.value(QStringLiteral("payload")).toObject());
        applyBattleResult(result);
    }
}

void MainWindow::showError(const QString &message)
{
    m_statusLabel->setText(message);
    appendLog(QStringLiteral("Ошибка: %1").arg(message));
    updateUi();
}

void MainWindow::showInfo(const QString &message)
{
    m_statusLabel->setText(message);
    appendLog(message);
    updateUi();
}

void MainWindow::updateTransportUi()
{
    const bool tcpMode = selectedTransport() == UiTransport::Tcp;
    m_portSpin->setVisible(tcpMode);
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);

    auto *leftColumn = new QVBoxLayout();
    auto *rightColumn = new QVBoxLayout();

    auto *networkBox = new QGroupBox(QStringLiteral("Сеть"), this);
    auto *networkLayout = new QFormLayout(networkBox);

    m_transportBox = new QComboBox(networkBox);
    m_transportBox->addItem(QStringLiteral("LAN (TCP)"), static_cast<int>(UiTransport::Tcp));
    if (m_network.bluetoothSupported())
    {
        m_transportBox->addItem(QStringLiteral("Bluetooth"), static_cast<int>(UiTransport::Bluetooth));
    }

    m_addressEdit = new QLineEdit(QStringLiteral("127.0.0.1"), networkBox);
    m_portSpin = new QSpinBox(networkBox);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(4242);
    m_hostButton = new QPushButton(QStringLiteral("Создать комнату"), networkBox);
    m_connectButton = new QPushButton(QStringLiteral("Подключиться"), networkBox);
    m_connectionLabel = new QLabel(QStringLiteral("Нет подключения"), networkBox);
    m_connectionLabel->setWordWrap(true);

    networkLayout->addRow(QStringLiteral("Транспорт"), m_transportBox);
    networkLayout->addRow(QStringLiteral("Адрес"), m_addressEdit);
    networkLayout->addRow(QStringLiteral("Порт"), m_portSpin);
    networkLayout->addRow(m_hostButton);
    networkLayout->addRow(m_connectButton);
    networkLayout->addRow(QStringLiteral("Статус"), m_connectionLabel);

    auto *stateBox = new QGroupBox(QStringLiteral("Состояние матча"), this);
    auto *stateLayout = new QFormLayout(stateBox);
    m_roundLabel = new QLabel(stateBox);
    m_goldLabel = new QLabel(stateBox);
    m_localHeroLabel = new QLabel(stateBox);
    m_remoteHeroLabel = new QLabel(stateBox);
    m_statusLabel = new QLabel(QStringLiteral("Соберите армию и дождитесь боя."), stateBox);
    m_statusLabel->setWordWrap(true);

    stateLayout->addRow(QStringLiteral("Раунд"), m_roundLabel);
    stateLayout->addRow(QStringLiteral("Монеты"), m_goldLabel);
    stateLayout->addRow(QStringLiteral("Ваш герой"), m_localHeroLabel);
    stateLayout->addRow(QStringLiteral("Соперник"), m_remoteHeroLabel);
    stateLayout->addRow(QStringLiteral("Подсказка"), m_statusLabel);

    auto *actionsBox = new QGroupBox(QStringLiteral("Действия"), this);
    auto *actionsLayout = new QVBoxLayout(actionsBox);
    m_refreshButton = new QPushButton(QStringLiteral("Обновить таверну (-1)"), actionsBox);
    m_buyButton = new QPushButton(QStringLiteral("Купить в руку"), actionsBox);
    m_playButton = new QPushButton(QStringLiteral("Выставить на стол"), actionsBox);
    m_sellButton = new QPushButton(QStringLiteral("Продать со стола (+1)"), actionsBox);
    m_readyButton = new QPushButton(QStringLiteral("Готов к бою"), actionsBox);

    actionsLayout->addWidget(m_refreshButton);
    actionsLayout->addWidget(m_buyButton);
    actionsLayout->addWidget(m_playButton);
    actionsLayout->addWidget(m_sellButton);
    actionsLayout->addWidget(m_readyButton);

    auto *tavernBox = new QGroupBox(QStringLiteral("Таверна"), this);
    auto *tavernLayout = new QVBoxLayout(tavernBox);
    m_tavernList = new QListWidget(tavernBox);
    tavernLayout->addWidget(m_tavernList);

    auto *handBox = new QGroupBox(QStringLiteral("Рука"), this);
    auto *handLayout = new QVBoxLayout(handBox);
    m_handList = new QListWidget(handBox);
    handLayout->addWidget(m_handList);

    auto *boardsBox = new QGroupBox(QStringLiteral("Стол"), this);
    auto *boardsLayout = new QHBoxLayout(boardsBox);
    auto *localBoardLayout = new QVBoxLayout();
    auto *remoteBoardLayout = new QVBoxLayout();

    m_localBoardList = new QListWidget(boardsBox);
    m_remoteBoardList = new QListWidget(boardsBox);

    localBoardLayout->addWidget(new QLabel(QStringLiteral("Ваш стол"), boardsBox));
    localBoardLayout->addWidget(m_localBoardList);
    remoteBoardLayout->addWidget(new QLabel(QStringLiteral("Стол соперника"), boardsBox));
    remoteBoardLayout->addWidget(m_remoteBoardList);

    boardsLayout->addLayout(localBoardLayout);
    boardsLayout->addLayout(remoteBoardLayout);

    auto *battleBox = new QGroupBox(QStringLiteral("Журнал боя"), this);
    auto *battleLayout = new QVBoxLayout(battleBox);
    m_battleLog = new QTextEdit(battleBox);
    m_battleLog->setReadOnly(true);
    battleLayout->addWidget(m_battleLog);

    leftColumn->addWidget(networkBox);
    leftColumn->addWidget(stateBox);
    leftColumn->addWidget(actionsBox);
    leftColumn->addStretch();

    rightColumn->addWidget(tavernBox);
    rightColumn->addWidget(handBox);
    rightColumn->addWidget(boardsBox);
    rightColumn->addWidget(battleBox, 1);

    rootLayout->addLayout(leftColumn, 0);
    rootLayout->addLayout(rightColumn, 1);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("Qt Battlegrounds"));
    resize(1200, 760);

    connect(m_transportBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { updateTransportUi(); });
    connect(m_hostButton, &QPushButton::clicked, this, &MainWindow::hostSession);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectToSession);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshTavern);
    connect(m_buyButton, &QPushButton::clicked, this, &MainWindow::buySelectedCard);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::playSelectedCard);
    connect(m_sellButton, &QPushButton::clicked, this, &MainWindow::sellSelectedMinion);
    connect(m_readyButton, &QPushButton::clicked, this, &MainWindow::toggleReady);
}

void MainWindow::updateUi()
{
    const AutoBattlerGame::PlayerState &local = m_game.localPlayer();
    const AutoBattlerGame::PlayerSnapshot &remote = m_game.remotePlayer();
    const bool connected = m_network.isConnected();
    const bool gameOver = local.hero.health <= 0 || remote.heroHealth <= 0;
    const bool buyPhase = connected && !local.ready && !gameOver;

    m_roundLabel->setText(QString::number(local.round));
    m_goldLabel->setText(QString("%1 / %2").arg(local.gold).arg(local.maxGold));
    m_localHeroLabel->setText(QString("%1 | здоровье %2").arg(local.hero.name).arg(local.hero.health));
    m_remoteHeroLabel->setText(QString("%1 | здоровье %2%3")
                                   .arg(remote.heroName)
                                   .arg(remote.heroHealth)
                                   .arg(remote.ready ? QStringLiteral(" | готов") : QString()));

    if (connected)
    {
        const QString transportName = m_network.transport() == NetworkManager::Transport::Bluetooth
            ? QStringLiteral("Bluetooth")
            : QStringLiteral("TCP");
        m_connectionLabel->setText(m_network.isHosting()
                                       ? QStringLiteral("Подключено (%1). Вы хост.").arg(transportName)
                                       : QStringLiteral("Подключено (%1). Вы клиент.").arg(transportName));
    }
    else if (m_network.isHosting())
    {
        m_connectionLabel->setText(QStringLiteral("Комната создана. Ожидание соперника."));
    }
    else
    {
        m_connectionLabel->setText(QStringLiteral("Нет подключения"));
    }

    m_tavernList->clear();
    for (const AutoBattlerGame::MinionCard &card : local.tavern)
    {
        m_tavernList->addItem(AutoBattlerGame::formatCard(card));
    }

    m_handList->clear();
    for (const AutoBattlerGame::MinionCard &card : local.hand)
    {
        m_handList->addItem(AutoBattlerGame::formatCard(card));
    }

    m_localBoardList->clear();
    for (const AutoBattlerGame::MinionCard &card : local.board)
    {
        m_localBoardList->addItem(AutoBattlerGame::formatBoardCard(card));
    }

    m_remoteBoardList->clear();
    for (const AutoBattlerGame::MinionCard &card : remote.board)
    {
        m_remoteBoardList->addItem(AutoBattlerGame::formatBoardCard(card));
    }

    m_addressEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected && selectedTransport() == UiTransport::Tcp);
    m_transportBox->setEnabled(!connected);
    m_hostButton->setEnabled(!connected);
    m_connectButton->setEnabled(!connected);

    m_refreshButton->setEnabled(buyPhase);
    m_buyButton->setEnabled(buyPhase && m_tavernList->count() > 0);
    m_playButton->setEnabled(buyPhase && m_handList->count() > 0);
    m_sellButton->setEnabled(buyPhase && m_localBoardList->count() > 0);
    m_readyButton->setEnabled(connected && !gameOver);
    m_readyButton->setText(local.ready ? QStringLiteral("Снять готовность") : QStringLiteral("Готов к бою"));
}

void MainWindow::appendLog(const QString &message)
{
    if (message.isEmpty())
    {
        return;
    }

    m_battleLog->append(message);
}

void MainWindow::appendBattleLog(const QStringList &lines)
{
    for (const QString &line : lines)
    {
        appendLog(line);
    }
}

void MainWindow::syncLocalState()
{
    if (!m_network.isConnected())
    {
        return;
    }

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("playerState"));
    object.insert(QStringLiteral("payload"), AutoBattlerGame::snapshotToJson(m_game.makeLocalSnapshot()));
    m_network.sendMessage(object);
}

void MainWindow::resolveBattleIfHost()
{
    if (!m_network.isConnected() || !m_network.isHosting())
    {
        return;
    }

    if (!m_game.localPlayer().ready || !m_game.remotePlayer().ready)
    {
        return;
    }

    m_statusLabel->setText(QStringLiteral("Оба игрока готовы. Начинается авто-бой."));
    const AutoBattlerGame::BattleResult result =
        AutoBattlerGame::resolveBattle(m_game.makeLocalSnapshot(), m_game.remotePlayer());

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("battleResult"));
    object.insert(QStringLiteral("payload"), AutoBattlerGame::battleResultToJson(result));
    m_network.sendMessage(object);

    applyBattleResult(result);
}

void MainWindow::applyBattleResult(const AutoBattlerGame::BattleResult &result)
{
    const bool localIsHost = m_network.isHosting();
    const int localHealth = localIsHost ? result.hostHeroHealth : result.clientHeroHealth;
    const int remoteHealth = localIsHost ? result.clientHeroHealth : result.hostHeroHealth;

    m_game.setLocalHeroHealth(localHealth);
    m_game.setRemoteHeroHealth(remoteHealth);
    m_game.setLocalReady(false);
    m_game.setRemoteReady(false);

    appendLog(QStringLiteral("========== БОЙ =========="));
    appendBattleLog(result.log);

    if (result.gameOver)
    {
        if (localHealth <= 0 && remoteHealth <= 0)
        {
            m_statusLabel->setText(QStringLiteral("Оба героя пали. Ничья."));
        }
        else if (localHealth <= 0)
        {
            m_statusLabel->setText(QStringLiteral("Ваш герой пал. Поражение."));
        }
        else
        {
            m_statusLabel->setText(QStringLiteral("Герой соперника пал. Победа."));
        }

        updateUi();
        return;
    }

    m_game.beginNextRound();
    m_statusLabel->setText(QStringLiteral("Начинается новый раунд покупок. Монеты пополнены."));
    updateUi();
    syncLocalState();
}

MainWindow::UiTransport MainWindow::selectedTransport() const
{
    return static_cast<UiTransport>(m_transportBox->currentData().toInt());
}
