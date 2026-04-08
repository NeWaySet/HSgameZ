#include "mainwindow.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QResizeEvent>
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
    appendLog(QStringLiteral("Можно играть по сети или выбрать режим 'сам с собой' на одном компьютере."));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (m_privacyOverlay != nullptr && centralWidget() != nullptr)
    {
        m_privacyOverlay->setGeometry(centralWidget()->rect());
    }
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

void MainWindow::startSelfGame()
{
    m_network.disconnectTransport();
    m_selfPlayActive = true;
    m_activeSelfPlayer = 0;
    m_selfViewLocked = false;
    m_game.resetPlayer(m_selfPlayers[0], QStringLiteral("Игрок 1"));
    m_game.resetPlayer(m_selfPlayers[1], QStringLiteral("Игрок 2"));
    m_battleLog->clear();
    m_statusLabel->setText(QStringLiteral("Локальная игра началась. Ход игрока 1."));
    appendLog(QStringLiteral("Запущен режим 'сам с собой'. Сначала закупается Игрок 1."));
    updateUi();
}

void MainWindow::switchSelfPlayer()
{
    if (!isSelfPlayMode() || !m_selfPlayActive || m_selfViewLocked)
    {
        return;
    }

    m_activeSelfPlayer = 1 - m_activeSelfPlayer;
    m_selfViewLocked = true;
    m_statusLabel->setText(QStringLiteral("Экран скрыт. Передайте устройство %1 и нажмите \"Показать ход\".")
                               .arg(currentSelfPlayer().hero.name));
    appendLog(QStringLiteral("Управление передано: %1. Экран скрыт до подтверждения.")
                  .arg(currentSelfPlayer().hero.name));
    updateUi();
}

void MainWindow::revealSelfTurn()
{
    if (!isSelfPlayMode() || !m_selfPlayActive || !m_selfViewLocked)
    {
        return;
    }

    m_selfViewLocked = false;
    m_statusLabel->setText(QStringLiteral("Сейчас ходит %1. Можно покупать и выставлять существ.")
                               .arg(currentSelfPlayer().hero.name));
    appendLog(QStringLiteral("Ход открыт для %1.").arg(currentSelfPlayer().hero.name));
    updateUi();
}

void MainWindow::refreshTavern()
{
    QString error;

    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            showError(QStringLiteral("Сначала начните локальную игру."));
            return;
        }

        if (!m_game.refreshTavern(currentSelfPlayer(), &error))
        {
            showError(error);
            return;
        }

        appendLog(QStringLiteral("%1 обновляет таверну за 1 монету.").arg(currentSelfPlayer().hero.name));
        updateUi();
        return;
    }

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

    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            showError(QStringLiteral("Сначала начните локальную игру."));
            return;
        }

        if (!m_game.buyFromTavern(currentSelfPlayer(), m_tavernList->currentRow(), &error))
        {
            showError(error);
            return;
        }

        appendLog(QStringLiteral("%1 покупает существо в руку.").arg(currentSelfPlayer().hero.name));
        updateUi();
        return;
    }

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

    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            showError(QStringLiteral("Сначала начните локальную игру."));
            return;
        }

        if (!m_game.playFromHand(currentSelfPlayer(), m_handList->currentRow(), &error))
        {
            showError(error);
            return;
        }

        appendLog(QStringLiteral("%1 выставляет существо на стол.").arg(currentSelfPlayer().hero.name));
        updateUi();
        return;
    }

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

    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            showError(QStringLiteral("Сначала начните локальную игру."));
            return;
        }

        if (!m_game.sellFromBoard(currentSelfPlayer(), m_localBoardList->currentRow(), &error))
        {
            showError(error);
            return;
        }

        appendLog(QStringLiteral("%1 продаёт существо и получает 1 монету.").arg(currentSelfPlayer().hero.name));
        updateUi();
        return;
    }

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
    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            showError(QStringLiteral("Сначала начните локальную игру."));
            return;
        }

        AutoBattlerGame::PlayerState &player = currentSelfPlayer();
        const bool newReadyState = !player.ready;
        m_game.setPlayerReady(player, newReadyState);

        appendLog(QStringLiteral("%1 %2.")
                      .arg(player.hero.name,
                           newReadyState ? QStringLiteral("закончил подготовку")
                                         : QStringLiteral("снял готовность")));

        if (newReadyState)
        {
            if (m_selfPlayers[0].ready && m_selfPlayers[1].ready)
            {
                resolveSelfBattleIfReady();
                return;
            }

            m_activeSelfPlayer = 1 - m_activeSelfPlayer;
            m_selfViewLocked = true;
            m_statusLabel->setText(QStringLiteral("%1 готов. Экран скрыт для %2, нажмите \"Показать ход\".")
                                       .arg(otherSelfPlayer().hero.name, currentSelfPlayer().hero.name));
        }
        else
        {
            m_statusLabel->setText(QStringLiteral("%1 снова может менять стол.")
                                       .arg(player.hero.name));
        }

        updateUi();
        return;
    }

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
    const bool networkMode = selectedMode() == SessionMode::Network;
    const bool tcpMode = selectedTransport() == UiTransport::Tcp;

    m_transportBox->setEnabled(networkMode && !m_network.isConnected());
    m_addressEdit->setEnabled(networkMode && !m_network.isConnected());
    m_portSpin->setVisible(networkMode && tcpMode);
    m_hostButton->setEnabled(networkMode && !m_network.isConnected());
    m_connectButton->setEnabled(networkMode && !m_network.isConnected());
    m_localGameButton->setEnabled(selectedMode() == SessionMode::SelfPlay);
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);

    auto *leftColumn = new QVBoxLayout();
    auto *rightColumn = new QVBoxLayout();

    auto *networkBox = new QGroupBox(QStringLiteral("Режим и сеть"), this);
    auto *networkLayout = new QFormLayout(networkBox);

    m_modeBox = new QComboBox(networkBox);
    m_modeBox->addItem(QStringLiteral("По сети"), static_cast<int>(SessionMode::Network));
    m_modeBox->addItem(QStringLiteral("Сам с собой"), static_cast<int>(SessionMode::SelfPlay));

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
    m_localGameButton = new QPushButton(QStringLiteral("Начать локальную игру"), networkBox);
    m_connectionLabel = new QLabel(QStringLiteral("Нет подключения"), networkBox);
    m_connectionLabel->setWordWrap(true);

    networkLayout->addRow(QStringLiteral("Режим"), m_modeBox);
    networkLayout->addRow(QStringLiteral("Транспорт"), m_transportBox);
    networkLayout->addRow(QStringLiteral("Адрес"), m_addressEdit);
    networkLayout->addRow(QStringLiteral("Порт"), m_portSpin);
    networkLayout->addRow(m_hostButton);
    networkLayout->addRow(m_connectButton);
    networkLayout->addRow(m_localGameButton);
    networkLayout->addRow(QStringLiteral("Статус"), m_connectionLabel);

    auto *stateBox = new QGroupBox(QStringLiteral("Состояние матча"), this);
    auto *stateLayout = new QFormLayout(stateBox);
    m_activePlayerLabel = new QLabel(stateBox);
    m_roundLabel = new QLabel(stateBox);
    m_goldLabel = new QLabel(stateBox);
    m_localHeroLabel = new QLabel(stateBox);
    m_remoteHeroLabel = new QLabel(stateBox);
    m_statusLabel = new QLabel(QStringLiteral("Выберите режим и начните матч."), stateBox);
    m_statusLabel->setWordWrap(true);

    stateLayout->addRow(QStringLiteral("Активный игрок"), m_activePlayerLabel);
    stateLayout->addRow(QStringLiteral("Раунд"), m_roundLabel);
    stateLayout->addRow(QStringLiteral("Монеты"), m_goldLabel);
    stateLayout->addRow(QStringLiteral("Текущая сторона"), m_localHeroLabel);
    stateLayout->addRow(QStringLiteral("Оппонент"), m_remoteHeroLabel);
    stateLayout->addRow(QStringLiteral("Подсказка"), m_statusLabel);

    auto *actionsBox = new QGroupBox(QStringLiteral("Действия"), this);
    auto *actionsLayout = new QVBoxLayout(actionsBox);
    m_refreshButton = new QPushButton(QStringLiteral("Обновить таверну (-1)"), actionsBox);
    m_buyButton = new QPushButton(QStringLiteral("Купить в руку"), actionsBox);
    m_playButton = new QPushButton(QStringLiteral("Выставить на стол"), actionsBox);
    m_sellButton = new QPushButton(QStringLiteral("Продать со стола (+1)"), actionsBox);
    m_readyButton = new QPushButton(QStringLiteral("Готов к бою"), actionsBox);
    m_switchPlayerButton = new QPushButton(QStringLiteral("Передать управление"), actionsBox);
    m_revealTurnButton = new QPushButton(QStringLiteral("Показать ход"), actionsBox);

    actionsLayout->addWidget(m_refreshButton);
    actionsLayout->addWidget(m_buyButton);
    actionsLayout->addWidget(m_playButton);
    actionsLayout->addWidget(m_sellButton);
    actionsLayout->addWidget(m_readyButton);
    actionsLayout->addWidget(m_switchPlayerButton);
    actionsLayout->addWidget(m_revealTurnButton);

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

    localBoardLayout->addWidget(new QLabel(QStringLiteral("Текущий стол"), boardsBox));
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
    resize(1240, 760);

    m_privacyOverlay = new QWidget(central);
    m_privacyOverlay->setAttribute(Qt::WA_StyledBackground, true);
    m_privacyOverlay->setStyleSheet(QStringLiteral("background-color: rgba(12, 16, 24, 228);"));
    m_privacyOverlay->setGeometry(central->rect());

    auto *overlayLayout = new QVBoxLayout(m_privacyOverlay);
    overlayLayout->setContentsMargins(48, 48, 48, 48);
    overlayLayout->setAlignment(Qt::AlignCenter);

    auto *overlayPanel = new QWidget(m_privacyOverlay);
    overlayPanel->setAttribute(Qt::WA_StyledBackground, true);
    overlayPanel->setStyleSheet(QStringLiteral(
        "background-color: rgba(255, 248, 236, 242);"
        "border: 2px solid rgba(127, 86, 44, 180);"
        "border-radius: 18px;"));
    overlayPanel->setMaximumWidth(520);

    auto *overlayPanelLayout = new QVBoxLayout(overlayPanel);
    overlayPanelLayout->setContentsMargins(32, 28, 32, 28);
    overlayPanelLayout->setSpacing(14);

    m_privacyTitleLabel = new QLabel(QStringLiteral("Передайте ход"), overlayPanel);
    QFont overlayTitleFont = m_privacyTitleLabel->font();
    overlayTitleFont.setPointSize(18);
    overlayTitleFont.setBold(true);
    m_privacyTitleLabel->setFont(overlayTitleFont);
    m_privacyTitleLabel->setAlignment(Qt::AlignCenter);

    m_privacyTextLabel = new QLabel(overlayPanel);
    m_privacyTextLabel->setWordWrap(true);
    m_privacyTextLabel->setAlignment(Qt::AlignCenter);

    m_privacyRevealButton = new QPushButton(QStringLiteral("Показать ход"), overlayPanel);
    m_privacyRevealButton->setMinimumHeight(40);

    overlayPanelLayout->addWidget(m_privacyTitleLabel);
    overlayPanelLayout->addWidget(m_privacyTextLabel);
    overlayPanelLayout->addWidget(m_privacyRevealButton);
    overlayLayout->addWidget(overlayPanel);

    m_privacyOverlay->hide();
    m_revealTurnButton->hide();

    connect(m_modeBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                updateTransportUi();
                updateUi();
            });
    connect(m_transportBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { updateTransportUi(); });
    connect(m_hostButton, &QPushButton::clicked, this, &MainWindow::hostSession);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectToSession);
    connect(m_localGameButton, &QPushButton::clicked, this, &MainWindow::startSelfGame);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshTavern);
    connect(m_buyButton, &QPushButton::clicked, this, &MainWindow::buySelectedCard);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::playSelectedCard);
    connect(m_sellButton, &QPushButton::clicked, this, &MainWindow::sellSelectedMinion);
    connect(m_readyButton, &QPushButton::clicked, this, &MainWindow::toggleReady);
    connect(m_switchPlayerButton, &QPushButton::clicked, this, &MainWindow::switchSelfPlayer);
    connect(m_revealTurnButton, &QPushButton::clicked, this, &MainWindow::revealSelfTurn);
    connect(m_privacyRevealButton, &QPushButton::clicked, this, &MainWindow::revealSelfTurn);
}

void MainWindow::updateUi()
{
    const bool selfPlay = isSelfPlayMode();

    QString connectionText = QStringLiteral("Нет подключения");
    QString activePlayerText = QStringLiteral("Не выбран");
    QString statusText = m_statusLabel->text();
    QString currentHeroText = QStringLiteral("Матч не начат");
    QString opponentHeroText = QStringLiteral("Матч не начат");
    QString roundText = QStringLiteral("-");
    QString goldText = QStringLiteral("-");

    bool buyPhase = false;
    bool gameOver = false;

    m_tavernList->clear();
    m_handList->clear();
    m_localBoardList->clear();
    m_remoteBoardList->clear();

    if (selfPlay)
    {
        connectionText = QStringLiteral("Локальная игра на одном компьютере.");

        if (m_selfPlayActive)
        {
            const AutoBattlerGame::PlayerState &current = currentSelfPlayer();
            const AutoBattlerGame::PlayerState &other = otherSelfPlayer();

            roundText = QString::number(current.round);
            opponentHeroText = QString("%1 | здоровье %2%3")
                                   .arg(other.hero.name)
                                   .arg(other.hero.health)
                                   .arg(other.ready ? QStringLiteral(" | готов") : QString());
            gameOver = current.hero.health <= 0 || other.hero.health <= 0;

            for (const AutoBattlerGame::MinionCard &card : other.board)
            {
                m_remoteBoardList->addItem(AutoBattlerGame::formatBoardCard(card));
            }

            if (m_selfViewLocked)
            {
                activePlayerText = QStringLiteral("Скрыт");
                goldText = QStringLiteral("Скрыто");
                currentHeroText = QStringLiteral("Скрыто до нажатия \"Показать ход\"");
                statusText = QStringLiteral("Экран скрыт. Передайте устройство следующему игроку.");
                m_tavernList->addItem(QStringLiteral("Таверна скрыта"));
                m_handList->addItem(QStringLiteral("Рука скрыта"));
                m_localBoardList->addItem(QStringLiteral("Ваш стол скрыт"));
            }
            else
            {
                activePlayerText = current.hero.name;
                goldText = QString("%1 / %2").arg(current.gold).arg(current.maxGold);
                currentHeroText = QString("%1 | здоровье %2%3")
                                      .arg(current.hero.name)
                                      .arg(current.hero.health)
                                      .arg(current.ready ? QStringLiteral(" | готов") : QString());
                buyPhase = !current.ready && !gameOver;

                for (const AutoBattlerGame::MinionCard &card : current.tavern)
                {
                    m_tavernList->addItem(AutoBattlerGame::formatCard(card));
                }

                for (const AutoBattlerGame::MinionCard &card : current.hand)
                {
                    m_handList->addItem(AutoBattlerGame::formatCard(card));
                }

                for (const AutoBattlerGame::MinionCard &card : current.board)
                {
                    m_localBoardList->addItem(AutoBattlerGame::formatBoardCard(card));
                }
            }
        }
        else
        {
            statusText = QStringLiteral("Нажмите 'Начать локальную игру', чтобы играть за обе стороны.");
        }
    }
    else
    {
        const AutoBattlerGame::PlayerState &local = m_game.localPlayer();
        const AutoBattlerGame::PlayerSnapshot &remote = m_game.remotePlayer();
        const bool connected = m_network.isConnected();

        activePlayerText = connected ? QStringLiteral("Вы") : QStringLiteral("Сеть");
        roundText = QString::number(local.round);
        goldText = QString("%1 / %2").arg(local.gold).arg(local.maxGold);
        currentHeroText = QString("%1 | здоровье %2").arg(local.hero.name).arg(local.hero.health);
        opponentHeroText = QString("%1 | здоровье %2%3")
                               .arg(remote.heroName)
                               .arg(remote.heroHealth)
                               .arg(remote.ready ? QStringLiteral(" | готов") : QString());

        gameOver = local.hero.health <= 0 || remote.heroHealth <= 0;
        buyPhase = connected && !local.ready && !gameOver;

        if (connected)
        {
            const QString transportName = m_network.transport() == NetworkManager::Transport::Bluetooth
                ? QStringLiteral("Bluetooth")
                : QStringLiteral("TCP");
            connectionText = m_network.isHosting()
                ? QStringLiteral("Подключено (%1). Вы хост.").arg(transportName)
                : QStringLiteral("Подключено (%1). Вы клиент.").arg(transportName);
        }

        for (const AutoBattlerGame::MinionCard &card : local.tavern)
        {
            m_tavernList->addItem(AutoBattlerGame::formatCard(card));
        }

        for (const AutoBattlerGame::MinionCard &card : local.hand)
        {
            m_handList->addItem(AutoBattlerGame::formatCard(card));
        }

        for (const AutoBattlerGame::MinionCard &card : local.board)
        {
            m_localBoardList->addItem(AutoBattlerGame::formatBoardCard(card));
        }

        for (const AutoBattlerGame::MinionCard &card : remote.board)
        {
            m_remoteBoardList->addItem(AutoBattlerGame::formatBoardCard(card));
        }
    }

    m_connectionLabel->setText(connectionText);
    m_activePlayerLabel->setText(activePlayerText);
    m_roundLabel->setText(roundText);
    m_goldLabel->setText(goldText);
    m_localHeroLabel->setText(currentHeroText);
    m_remoteHeroLabel->setText(opponentHeroText);
    m_statusLabel->setText(statusText);

    updateTransportUi();

    m_refreshButton->setEnabled(buyPhase);
    m_buyButton->setEnabled(buyPhase && m_tavernList->count() > 0);
    m_playButton->setEnabled(buyPhase && m_handList->count() > 0);
    m_sellButton->setEnabled(buyPhase && m_localBoardList->count() > 0);

    if (selfPlay)
    {
        const bool readyEnabled = m_selfPlayActive && !gameOver && !m_selfViewLocked;
        m_readyButton->setEnabled(readyEnabled);
        m_readyButton->setText(m_selfPlayActive && !m_selfViewLocked && currentSelfPlayer().ready
                                   ? QStringLiteral("Снять готовность")
                                   : QStringLiteral("Готов к бою"));
        m_switchPlayerButton->setEnabled(m_selfPlayActive && !gameOver && !m_selfViewLocked);
        m_revealTurnButton->setEnabled(m_selfPlayActive && m_selfViewLocked && !gameOver);
    }
    else
    {
        m_readyButton->setEnabled(m_network.isConnected() && !gameOver);
        m_readyButton->setText(m_game.localPlayer().ready
                                   ? QStringLiteral("Снять готовность")
                                   : QStringLiteral("Готов к бою"));
        m_switchPlayerButton->setEnabled(false);
        m_revealTurnButton->setEnabled(false);
    }

    const bool showPrivacyOverlay = selfPlay && m_selfPlayActive && m_selfViewLocked && !gameOver;
    if (m_privacyOverlay != nullptr)
    {
        if (showPrivacyOverlay)
        {
            m_privacyTitleLabel->setText(QStringLiteral("Передайте ход"));
            m_privacyTextLabel->setText(
                QStringLiteral("Передайте устройство %1. Когда следующий игрок будет готов, нажмите кнопку ниже, чтобы открыть его фазу покупки.")
                    .arg(currentSelfPlayer().hero.name));
            m_privacyRevealButton->setEnabled(true);
            m_privacyOverlay->setGeometry(centralWidget()->rect());
            m_privacyOverlay->show();
            m_privacyOverlay->raise();
            m_privacyRevealButton->setFocus();
        }
        else
        {
            m_privacyOverlay->hide();
        }
    }
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
    if (isSelfPlayMode() || !m_network.isConnected())
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
    if (isSelfPlayMode() || !m_network.isConnected() || !m_network.isHosting())
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

void MainWindow::resolveSelfBattleIfReady()
{
    if (!m_selfPlayActive || !m_selfPlayers[0].ready || !m_selfPlayers[1].ready)
    {
        return;
    }

    m_statusLabel->setText(QStringLiteral("Оба локальных игрока готовы. Начинается авто-бой."));
    const AutoBattlerGame::BattleResult result =
        AutoBattlerGame::resolveBattle(m_game.makeSnapshot(m_selfPlayers[0]),
                                       m_game.makeSnapshot(m_selfPlayers[1]));
    applySelfBattleResult(result);
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

void MainWindow::applySelfBattleResult(const AutoBattlerGame::BattleResult &result)
{
    m_selfPlayers[0].hero.health = result.hostHeroHealth;
    m_selfPlayers[1].hero.health = result.clientHeroHealth;
    m_selfPlayers[0].ready = false;
    m_selfPlayers[1].ready = false;

    appendLog(QStringLiteral("========== ЛОКАЛЬНЫЙ БОЙ =========="));
    appendBattleLog(result.log);

    if (result.gameOver)
    {
        if (result.hostHeroHealth <= 0 && result.clientHeroHealth <= 0)
        {
            m_statusLabel->setText(QStringLiteral("Локальная партия завершилась ничьей."));
        }
        else if (result.hostHeroHealth <= 0)
        {
            m_statusLabel->setText(QStringLiteral("Игрок 2 победил в локальной партии."));
        }
        else
        {
            m_statusLabel->setText(QStringLiteral("Игрок 1 победил в локальной партии."));
        }

        updateUi();
        return;
    }

    m_game.beginNextRound(m_selfPlayers[0]);
    m_game.beginNextRound(m_selfPlayers[1]);
    m_activeSelfPlayer = 0;
    m_selfViewLocked = true;
    m_statusLabel->setText(QStringLiteral("Новый раунд. Передайте устройство Игроку 1 и нажмите \"Показать ход\"."));
    appendLog(QStringLiteral("Начинается новый локальный раунд."));
    updateUi();
}

MainWindow::UiTransport MainWindow::selectedTransport() const
{
    return static_cast<UiTransport>(m_transportBox->currentData().toInt());
}

MainWindow::SessionMode MainWindow::selectedMode() const
{
    return static_cast<SessionMode>(m_modeBox->currentData().toInt());
}

bool MainWindow::isSelfPlayMode() const
{
    return selectedMode() == SessionMode::SelfPlay;
}

AutoBattlerGame::PlayerState &MainWindow::currentSelfPlayer()
{
    return m_selfPlayers[m_activeSelfPlayer];
}

const AutoBattlerGame::PlayerState &MainWindow::currentSelfPlayer() const
{
    return m_selfPlayers[m_activeSelfPlayer];
}

AutoBattlerGame::PlayerState &MainWindow::otherSelfPlayer()
{
    return m_selfPlayers[1 - m_activeSelfPlayer];
}

const AutoBattlerGame::PlayerState &MainWindow::otherSelfPlayer() const
{
    return m_selfPlayers[1 - m_activeSelfPlayer];
}
