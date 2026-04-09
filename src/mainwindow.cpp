#include "mainwindow.h"

#include <QGuiApplication>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QResizeEvent>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

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
    const QString firstName = m_localNameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("Игрок 1")
        : m_localNameEdit->text().trimmed();
    const QString secondName = m_secondNameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("Игрок 2")
        : m_secondNameEdit->text().trimmed();
    m_game.resetPlayer(m_selfPlayers[0], firstName);
    m_game.resetPlayer(m_selfPlayers[1], secondName);
    m_battleLog->clear();
    m_statusLabel->setText(QStringLiteral("Локальная игра началась. Ходит %1.").arg(firstName));
    appendLog(QStringLiteral("Запущен режим 'сам с собой'. Сначала закупается %1.").arg(firstName));
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

void MainWindow::attackSelectedMinion()
{
    QString error;
    QStringList log;

    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            showError(QStringLiteral("Сначала начните локальную игру."));
            return;
        }

        if (!m_game.attackMinion(currentSelfPlayer(),
                                 otherSelfPlayer(),
                                 m_localBoardList->currentRow(),
                                 m_remoteBoardList->currentRow(),
                                 &error,
                                 &log))
        {
            showError(error);
            return;
        }

        appendLog(QStringLiteral("========== АТАКА =========="));
        appendBattleLog(log);
        applyGameOverStatus();
        updateUi();
        return;
    }

    if (!m_network.isConnected())
    {
        showError(QStringLiteral("Сначала подключитесь к сопернику."));
        return;
    }

    if (!m_game.attackRemoteMinion(m_localBoardList->currentRow(), m_remoteBoardList->currentRow(), &error, &log))
    {
        showError(error);
        return;
    }

    appendLog(QStringLiteral("========== ВАША АТАКА =========="));
    appendBattleLog(log);

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("attackMinion"));
    object.insert(QStringLiteral("attackerIndex"), m_localBoardList->currentRow());
    object.insert(QStringLiteral("targetIndex"), m_remoteBoardList->currentRow());
    m_network.sendMessage(object);

    applyGameOverStatus();
    updateUi();
    syncLocalState();
}

void MainWindow::attackEnemyHero()
{
    QString error;
    QStringList log;

    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            showError(QStringLiteral("Сначала начните локальную игру."));
            return;
        }

        if (!m_game.attackHero(currentSelfPlayer(),
                               otherSelfPlayer(),
                               m_localBoardList->currentRow(),
                               &error,
                               &log))
        {
            showError(error);
            return;
        }

        appendLog(QStringLiteral("========== АТАКА ГЕРОЯ =========="));
        appendBattleLog(log);
        applyGameOverStatus();
        updateUi();
        return;
    }

    if (!m_network.isConnected())
    {
        showError(QStringLiteral("Сначала подключитесь к сопернику."));
        return;
    }

    if (!m_game.attackRemoteHero(m_localBoardList->currentRow(), &error, &log))
    {
        showError(error);
        return;
    }

    appendLog(QStringLiteral("========== АТАКА ГЕРОЯ =========="));
    appendBattleLog(log);

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("attackHero"));
    object.insert(QStringLiteral("attackerIndex"), m_localBoardList->currentRow());
    m_network.sendMessage(object);

    applyGameOverStatus();
    updateUi();
    syncLocalState();
}

void MainWindow::applyPlayerNames()
{
    const QString firstName = m_localNameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("Игрок 1")
        : m_localNameEdit->text().trimmed();
    const QString secondName = m_secondNameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("Игрок 2")
        : m_secondNameEdit->text().trimmed();

    if (isSelfPlayMode())
    {
        m_game.setHeroName(m_selfPlayers[0], firstName);
        m_game.setHeroName(m_selfPlayers[1], secondName);
        appendLog(QStringLiteral("Имена локальных игроков обновлены: %1 и %2.").arg(firstName, secondName));
        updateUi();
        return;
    }

    m_game.setHeroName(m_game.localPlayer(), firstName);
    m_game.setRemoteHeroName(secondName);
    appendLog(QStringLiteral("Имя вашего героя установлено: %1.").arg(firstName));

    if (m_network.isConnected())
    {
        syncLocalState();
    }

    updateUi();
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

void MainWindow::applyGameOverStatus()
{
    if (isSelfPlayMode())
    {
        if (!m_selfPlayActive)
        {
            return;
        }

        const AutoBattlerGame::PlayerState &first = m_selfPlayers[0];
        const AutoBattlerGame::PlayerState &second = m_selfPlayers[1];

        if (first.hero.health <= 0 && second.hero.health <= 0)
        {
            m_statusLabel->setText(QStringLiteral("Оба игрока довели друг друга до 0 здоровья. Ничья."));
        }
        else if (first.hero.health <= 0)
        {
            m_statusLabel->setText(QStringLiteral("%1 победил: здоровье %2 опустилось до 0.")
                                       .arg(second.hero.name, first.hero.name));
        }
        else if (second.hero.health <= 0)
        {
            m_statusLabel->setText(QStringLiteral("%1 победил: здоровье %2 опустилось до 0.")
                                       .arg(first.hero.name, second.hero.name));
        }

        return;
    }

    const AutoBattlerGame::PlayerState &local = m_game.localPlayer();
    const AutoBattlerGame::PlayerSnapshot &remote = m_game.remotePlayer();

    if (local.hero.health <= 0 && remote.heroHealth <= 0)
    {
        m_statusLabel->setText(QStringLiteral("Оба героя пали. Ничья."));
    }
    else if (local.hero.health <= 0)
    {
        m_statusLabel->setText(QStringLiteral("Поражение: здоровье вашего героя опустилось до 0."));
    }
    else if (remote.heroHealth <= 0)
    {
        m_statusLabel->setText(QStringLiteral("Победа: здоровье героя соперника опустилось до 0."));
    }
}

void MainWindow::onConnected()
{
    const QString heroName = m_localNameEdit->text().trimmed().isEmpty()
        ? (m_network.isHosting() ? QStringLiteral("Хозяин таверны") : QStringLiteral("Искатель славы"))
        : m_localNameEdit->text().trimmed();
    const QString opponentName = m_secondNameEdit->text().trimmed().isEmpty()
        ? QStringLiteral("Соперник")
        : m_secondNameEdit->text().trimmed();

    m_game.resetMatch(heroName, opponentName);
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
        return;
    }

    if (type == QLatin1String("attackMinion"))
    {
        AutoBattlerGame::PlayerState remoteState;
        remoteState.hero.name = m_game.remotePlayer().heroName;
        remoteState.hero.health = m_game.remotePlayer().heroHealth;
        remoteState.round = m_game.remotePlayer().round;
        remoteState.ready = m_game.remotePlayer().ready;
        remoteState.board = m_game.remotePlayer().board;

        QStringList log;
        if (m_game.attackMinion(remoteState,
                                m_game.localPlayer(),
                                object.value(QStringLiteral("attackerIndex")).toInt(-1),
                                object.value(QStringLiteral("targetIndex")).toInt(-1),
                                nullptr,
                                &log))
        {
            m_game.setRemotePlayer(m_game.makeSnapshot(remoteState));
            appendLog(QStringLiteral("========== АТАКА СОПЕРНИКА =========="));
            appendBattleLog(log);
            applyGameOverStatus();
            updateUi();
            syncLocalState();
        }
        return;
    }

    if (type == QLatin1String("attackHero"))
    {
        AutoBattlerGame::PlayerState remoteState;
        remoteState.hero.name = m_game.remotePlayer().heroName;
        remoteState.hero.health = m_game.remotePlayer().heroHealth;
        remoteState.round = m_game.remotePlayer().round;
        remoteState.ready = m_game.remotePlayer().ready;
        remoteState.board = m_game.remotePlayer().board;

        QStringList log;
        if (m_game.attackHero(remoteState,
                              m_game.localPlayer(),
                              object.value(QStringLiteral("attackerIndex")).toInt(-1),
                              nullptr,
                              &log))
        {
            m_game.setRemotePlayer(m_game.makeSnapshot(remoteState));
            appendLog(QStringLiteral("========== АТАКА СОПЕРНИКА ПО ГЕРОЮ =========="));
            appendBattleLog(log);
            applyGameOverStatus();
            updateUi();
            syncLocalState();
        }
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
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(20, 18, 20, 20);
    rootLayout->setSpacing(16);

    auto *headerPanel = new QWidget(central);
    headerPanel->setObjectName(QStringLiteral("headerPanel"));
    auto *headerLayout = new QHBoxLayout(headerPanel);
    headerLayout->setContentsMargins(24, 18, 24, 18);
    headerLayout->setSpacing(18);

    auto *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(4);

    auto *headerTitle = new QLabel(QStringLiteral("Qt Battlegrounds"), headerPanel);
    headerTitle->setObjectName(QStringLiteral("headerTitle"));
    auto *headerSubtitle = new QLabel(
        QStringLiteral("Таверна, герои, ручные атаки и сетевые матчи в более широком и удобном интерфейсе."),
        headerPanel);
    headerSubtitle->setObjectName(QStringLiteral("headerSubtitle"));
    headerSubtitle->setWordWrap(true);

    headerTextLayout->addWidget(headerTitle);
    headerTextLayout->addWidget(headerSubtitle);

    auto *headerActionsLayout = new QVBoxLayout();
    headerActionsLayout->setSpacing(8);

    auto *fullscreenHintLabel = new QLabel(QStringLiteral("F11 - полноэкранный режим, Esc - выход"), headerPanel);
    fullscreenHintLabel->setObjectName(QStringLiteral("fullscreenHintLabel"));
    fullscreenHintLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *fullscreenButton = new QPushButton(QStringLiteral("Полный экран"), headerPanel);
    fullscreenButton->setObjectName(QStringLiteral("fullscreenButton"));
    fullscreenButton->setMinimumWidth(190);

    headerActionsLayout->addWidget(fullscreenHintLabel);
    headerActionsLayout->addWidget(fullscreenButton, 0, Qt::AlignRight);

    headerLayout->addLayout(headerTextLayout, 1);
    headerLayout->addLayout(headerActionsLayout, 0);

    auto *contentSplitter = new QSplitter(Qt::Horizontal, central);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->setHandleWidth(10);
    contentSplitter->setObjectName(QStringLiteral("contentSplitter"));

    auto *sidebarWidget = new QWidget(contentSplitter);
    sidebarWidget->setObjectName(QStringLiteral("sidebarWidget"));
    sidebarWidget->setMinimumWidth(320);
    sidebarWidget->setMaximumWidth(430);
    auto *leftColumn = new QVBoxLayout(sidebarWidget);
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(16);

    auto *dashboardWidget = new QWidget(contentSplitter);
    dashboardWidget->setObjectName(QStringLiteral("dashboardWidget"));
    auto *dashboardLayout = new QGridLayout(dashboardWidget);
    dashboardLayout->setContentsMargins(0, 0, 0, 0);
    dashboardLayout->setHorizontalSpacing(16);
    dashboardLayout->setVerticalSpacing(16);

    auto *networkBox = new QGroupBox(QStringLiteral("Режим и сеть"), this);
    networkBox->setObjectName(QStringLiteral("networkBox"));
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

    m_localNameEdit = new QLineEdit(QStringLiteral("Игрок 1"), networkBox);
    m_secondNameEdit = new QLineEdit(QStringLiteral("Игрок 2"), networkBox);
    m_addressEdit = new QLineEdit(QStringLiteral("127.0.0.1"), networkBox);
    m_portSpin = new QSpinBox(networkBox);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(4242);
    m_hostButton = new QPushButton(QStringLiteral("Создать комнату"), networkBox);
    m_connectButton = new QPushButton(QStringLiteral("Подключиться"), networkBox);
    m_localGameButton = new QPushButton(QStringLiteral("Начать локальную игру"), networkBox);
    m_applyNamesButton = new QPushButton(QStringLiteral("Применить имена"), networkBox);
    m_connectionLabel = new QLabel(QStringLiteral("Нет подключения"), networkBox);
    m_connectionLabel->setWordWrap(true);

    networkLayout->addRow(QStringLiteral("Режим"), m_modeBox);
    networkLayout->addRow(QStringLiteral("Игрок 1"), m_localNameEdit);
    networkLayout->addRow(QStringLiteral("Игрок 2 / соперник"), m_secondNameEdit);
    networkLayout->addRow(m_applyNamesButton);
    networkLayout->addRow(QStringLiteral("Транспорт"), m_transportBox);
    networkLayout->addRow(QStringLiteral("Адрес"), m_addressEdit);
    networkLayout->addRow(QStringLiteral("Порт"), m_portSpin);
    networkLayout->addRow(m_hostButton);
    networkLayout->addRow(m_connectButton);
    networkLayout->addRow(m_localGameButton);
    networkLayout->addRow(QStringLiteral("Статус"), m_connectionLabel);

    auto *stateBox = new QGroupBox(QStringLiteral("Состояние матча"), this);
    stateBox->setObjectName(QStringLiteral("stateBox"));
    auto *stateLayout = new QFormLayout(stateBox);
    m_activePlayerLabel = new QLabel(stateBox);
    m_roundLabel = new QLabel(stateBox);
    m_goldLabel = new QLabel(stateBox);
    m_localHeroLabel = new QLabel(stateBox);
    m_remoteHeroLabel = new QLabel(stateBox);
    m_activePlayerLabel->setWordWrap(true);
    m_localHeroLabel->setWordWrap(true);
    m_remoteHeroLabel->setWordWrap(true);
    m_statusLabel = new QLabel(QStringLiteral("Выберите режим и начните матч."), stateBox);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMinimumHeight(54);

    stateLayout->addRow(QStringLiteral("Активный игрок"), m_activePlayerLabel);
    stateLayout->addRow(QStringLiteral("Раунд"), m_roundLabel);
    stateLayout->addRow(QStringLiteral("Монеты"), m_goldLabel);
    stateLayout->addRow(QStringLiteral("Текущая сторона"), m_localHeroLabel);
    stateLayout->addRow(QStringLiteral("Оппонент"), m_remoteHeroLabel);
    stateLayout->addRow(QStringLiteral("Подсказка"), m_statusLabel);

    auto *actionsBox = new QGroupBox(QStringLiteral("Действия"), this);
    actionsBox->setObjectName(QStringLiteral("actionsBox"));
    auto *actionsLayout = new QVBoxLayout(actionsBox);
    actionsLayout->setSpacing(10);
    m_refreshButton = new QPushButton(QStringLiteral("Обновить таверну (-1)"), actionsBox);
    m_buyButton = new QPushButton(QStringLiteral("Купить в руку"), actionsBox);
    m_playButton = new QPushButton(QStringLiteral("Выставить на стол"), actionsBox);
    m_sellButton = new QPushButton(QStringLiteral("Продать со стола (+1)"), actionsBox);
    m_attackMinionButton = new QPushButton(QStringLiteral("Атаковать существо"), actionsBox);
    m_attackHeroButton = new QPushButton(QStringLiteral("Атаковать героя"), actionsBox);
    m_readyButton = new QPushButton(QStringLiteral("Готов к бою"), actionsBox);
    m_switchPlayerButton = new QPushButton(QStringLiteral("Передать управление"), actionsBox);
    m_revealTurnButton = new QPushButton(QStringLiteral("Показать ход"), actionsBox);

    actionsLayout->addWidget(m_refreshButton);
    actionsLayout->addWidget(m_buyButton);
    actionsLayout->addWidget(m_playButton);
    actionsLayout->addWidget(m_sellButton);
    actionsLayout->addWidget(m_attackMinionButton);
    actionsLayout->addWidget(m_attackHeroButton);
    actionsLayout->addWidget(m_readyButton);
    actionsLayout->addWidget(m_switchPlayerButton);
    actionsLayout->addWidget(m_revealTurnButton);

    auto *tavernBox = new QGroupBox(QStringLiteral("Таверна"), this);
    tavernBox->setObjectName(QStringLiteral("tavernBox"));
    auto *tavernLayout = new QVBoxLayout(tavernBox);
    m_tavernList = new QListWidget(tavernBox);
    m_tavernList->setObjectName(QStringLiteral("tavernList"));
    m_tavernList->setAlternatingRowColors(true);
    m_tavernList->setSpacing(4);
    tavernLayout->addWidget(m_tavernList);

    auto *handBox = new QGroupBox(QStringLiteral("Рука"), this);
    handBox->setObjectName(QStringLiteral("handBox"));
    auto *handLayout = new QVBoxLayout(handBox);
    m_handList = new QListWidget(handBox);
    m_handList->setObjectName(QStringLiteral("handList"));
    m_handList->setAlternatingRowColors(true);
    m_handList->setSpacing(4);
    handLayout->addWidget(m_handList);

    auto *boardsBox = new QGroupBox(QStringLiteral("Стол"), this);
    boardsBox->setObjectName(QStringLiteral("boardsBox"));
    auto *boardsLayout = new QHBoxLayout(boardsBox);
    boardsLayout->setSpacing(14);
    auto *localBoardLayout = new QVBoxLayout();
    auto *remoteBoardLayout = new QVBoxLayout();

    m_localBoardList = new QListWidget(boardsBox);
    m_remoteBoardList = new QListWidget(boardsBox);
    m_localBoardList->setObjectName(QStringLiteral("localBoardList"));
    m_remoteBoardList->setObjectName(QStringLiteral("remoteBoardList"));
    m_localBoardList->setAlternatingRowColors(true);
    m_remoteBoardList->setAlternatingRowColors(true);
    m_localBoardList->setSpacing(4);
    m_remoteBoardList->setSpacing(4);

    localBoardLayout->addWidget(new QLabel(QStringLiteral("Текущий стол"), boardsBox));
    localBoardLayout->addWidget(m_localBoardList);
    remoteBoardLayout->addWidget(new QLabel(QStringLiteral("Стол соперника"), boardsBox));
    remoteBoardLayout->addWidget(m_remoteBoardList);

    boardsLayout->addLayout(localBoardLayout);
    boardsLayout->addLayout(remoteBoardLayout);

    auto *battleBox = new QGroupBox(QStringLiteral("Журнал боя"), this);
    battleBox->setObjectName(QStringLiteral("battleBox"));
    auto *battleLayout = new QVBoxLayout(battleBox);
    m_battleLog = new QTextEdit(battleBox);
    m_battleLog->setObjectName(QStringLiteral("battleLog"));
    m_battleLog->setReadOnly(true);
    battleLayout->addWidget(m_battleLog);

    leftColumn->addWidget(networkBox);
    leftColumn->addWidget(stateBox);
    leftColumn->addWidget(actionsBox);
    leftColumn->addStretch();

    dashboardLayout->addWidget(tavernBox, 0, 0);
    dashboardLayout->addWidget(handBox, 0, 1);
    dashboardLayout->addWidget(boardsBox, 1, 0, 1, 2);
    dashboardLayout->addWidget(battleBox, 2, 0, 1, 2);
    dashboardLayout->setColumnStretch(0, 3);
    dashboardLayout->setColumnStretch(1, 2);
    dashboardLayout->setRowStretch(0, 3);
    dashboardLayout->setRowStretch(1, 4);
    dashboardLayout->setRowStretch(2, 3);

    contentSplitter->addWidget(sidebarWidget);
    contentSplitter->addWidget(dashboardWidget);
    contentSplitter->setStretchFactor(0, 0);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setSizes({360, 1120});

    rootLayout->addWidget(headerPanel);
    rootLayout->addWidget(contentSplitter, 1);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("Qt Battlegrounds"));
    setMinimumSize(1100, 700);
    const QRect available = QGuiApplication::primaryScreen() != nullptr
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1600, 900);
    const int targetWidth = std::max(1100, static_cast<int>(available.width() * 0.95));
    const int targetHeight = std::max(700, static_cast<int>(available.height() * 0.92));
    resize(std::min(targetWidth, available.width()), std::min(targetHeight, available.height()));
    setFont(QFont(QStringLiteral("Segoe UI"), 10));

    setStyleSheet(QStringLiteral(
        "QMainWindow {"
        "  background-color: #120b08;"
        "  background-image: qradialgradient(cx:0.5, cy:0.18, radius:1.0,"
        "      fx:0.5, fy:0.18,"
        "      stop:0 #533119, stop:0.28 #2d190f, stop:0.65 #180f0b, stop:1 #0d0705);"
        "  color: #f6e4c0;"
        "}"
        "QWidget {"
        "  color: #f6e3bc;"
        "  font-size: 10pt;"
        "}"
        "QWidget#headerPanel {"
        "  background-color: rgba(63, 33, 14, 228);"
        "  border: 2px solid rgba(214, 163, 83, 210);"
        "  border-radius: 22px;"
        "}"
        "QLabel#headerTitle {"
        "  color: #ffe7ac;"
        "  font: 700 22pt 'Georgia';"
        "}"
        "QLabel#headerSubtitle {"
        "  color: rgba(255, 241, 210, 210);"
        "  font: 11pt 'Trebuchet MS';"
        "}"
        "QLabel#fullscreenHintLabel {"
        "  color: rgba(255, 231, 188, 180);"
        "  font: 9.5pt 'Segoe UI';"
        "}"
        "QSplitter::handle {"
        "  background: transparent;"
        "}"
        "QGroupBox {"
        "  background-color: rgba(34, 20, 12, 220);"
        "  border: 2px solid rgba(179, 127, 63, 195);"
        "  border-radius: 18px;"
        "  margin-top: 18px;"
        "  padding: 20px 16px 16px 16px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 16px;"
        "  padding: 3px 11px;"
        "  color: #ffdc93;"
        "  background-color: rgba(92, 54, 24, 236);"
        "  border: 1px solid rgba(234, 191, 110, 175);"
        "  border-radius: 10px;"
        "  font: bold 11pt 'Georgia';"
        "}"
        "QGroupBox#networkBox, QGroupBox#stateBox, QGroupBox#actionsBox {"
        "  background-color: rgba(41, 24, 15, 228);"
        "}"
        "QGroupBox#tavernBox, QGroupBox#handBox, QGroupBox#boardsBox, QGroupBox#battleBox {"
        "  background-color: rgba(29, 18, 11, 236);"
        "}"
        "QLabel {"
        "  color: #f8e6c5;"
        "}"
        "QLineEdit, QSpinBox, QComboBox, QListWidget, QTextEdit {"
        "  background-color: rgba(245, 230, 200, 240);"
        "  color: #2b180c;"
        "  border: 2px solid rgba(157, 109, 50, 195);"
        "  border-radius: 14px;"
        "  padding: 7px 10px;"
        "  selection-background-color: #8d4318;"
        "  selection-color: #fff2d4;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QListWidget:focus, QTextEdit:focus {"
        "  border-color: rgba(255, 200, 102, 220);"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 24px;"
        "}"
        "QListWidget#tavernList, QListWidget#handList, QListWidget#localBoardList, QListWidget#remoteBoardList {"
        "  font: 600 10pt 'Trebuchet MS';"
        "}"
        "QTextEdit#battleLog {"
        "  font: 10pt 'Consolas';"
        "  background-color: rgba(253, 240, 215, 242);"
        "}"
        "QListWidget::item {"
        "  margin: 3px 0;"
        "  padding: 10px 12px;"
        "  border-radius: 11px;"
        "}"
        "QListWidget::item:alternate {"
        "  background-color: rgba(135, 96, 56, 34);"
        "}"
        "QListWidget::item:selected {"
        "  background-color: rgba(128, 59, 20, 220);"
        "  color: #fff3d8;"
        "  border: 1px solid rgba(255, 214, 142, 160);"
        "}"
        "QPushButton {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "      stop:0 #ffd886, stop:0.45 #d79739, stop:1 #8a4f16);"
        "  color: #281208;"
        "  border: 2px solid rgba(255, 232, 180, 210);"
        "  border-radius: 15px;"
        "  padding: 10px 16px;"
        "  font: bold 10pt 'Segoe UI';"
        "}"
        "QPushButton#fullscreenButton {"
        "  min-height: 40px;"
        "}"
        "QPushButton:hover {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "      stop:0 #ffe39a, stop:0.5 #ebb152, stop:1 #9f5d1f);"
        "}"
        "QPushButton:pressed {"
        "  padding-top: 12px;"
        "  padding-bottom: 8px;"
        "  background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "      stop:0 #bc7b27, stop:1 #6e420f);"
        "}"
        "QPushButton:disabled {"
        "  background-color: rgba(95, 75, 50, 170);"
        "  color: rgba(255, 241, 216, 118);"
        "  border-color: rgba(230, 213, 178, 85);"
        "}"
        "QScrollBar:vertical {"
        "  background: rgba(56, 33, 20, 160);"
        "  width: 12px;"
        "  margin: 10px 0 10px 0;"
        "  border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: rgba(231, 183, 94, 190);"
        "  min-height: 28px;"
        "  border-radius: 6px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  border: none;"
        "  background: none;"
        "}"
    ));

    const QString heroPanelStyle = QStringLiteral(
        "padding: 10px 12px;"
        "background-color: rgba(97, 57, 25, 178);"
        "border: 1px solid rgba(250, 215, 150, 140);"
        "border-radius: 14px;"
        "color: #fff1cf;"
        "font: 600 10pt 'Trebuchet MS';");
    m_connectionLabel->setStyleSheet(heroPanelStyle);
    m_statusLabel->setStyleSheet(heroPanelStyle);
    m_localHeroLabel->setStyleSheet(heroPanelStyle);
    m_remoteHeroLabel->setStyleSheet(heroPanelStyle);
    m_activePlayerLabel->setStyleSheet(heroPanelStyle);
    m_roundLabel->setStyleSheet(heroPanelStyle);
    m_goldLabel->setStyleSheet(heroPanelStyle);

    m_privacyOverlay = new QWidget(central);
    m_privacyOverlay->setAttribute(Qt::WA_StyledBackground, true);
    m_privacyOverlay->setStyleSheet(QStringLiteral(
        "background-color: rgba(18, 9, 4, 236);"));
    m_privacyOverlay->setGeometry(central->rect());

    auto *overlayLayout = new QVBoxLayout(m_privacyOverlay);
    overlayLayout->setContentsMargins(48, 48, 48, 48);
    overlayLayout->setAlignment(Qt::AlignCenter);

    auto *overlayPanel = new QWidget(m_privacyOverlay);
    overlayPanel->setAttribute(Qt::WA_StyledBackground, true);
    overlayPanel->setStyleSheet(QStringLiteral(
        "background-color: rgba(243, 224, 187, 244);"
        "border: 2px solid rgba(190, 143, 70, 220);"
        "border-radius: 22px;"));
    overlayPanel->setMaximumWidth(520);

    auto *overlayPanelLayout = new QVBoxLayout(overlayPanel);
    overlayPanelLayout->setContentsMargins(32, 28, 32, 28);
    overlayPanelLayout->setSpacing(14);

    m_privacyTitleLabel = new QLabel(QStringLiteral("Передайте ход"), overlayPanel);
    QFont overlayTitleFont = m_privacyTitleLabel->font();
    overlayTitleFont.setPointSize(18);
    overlayTitleFont.setBold(true);
    overlayTitleFont.setFamily(QStringLiteral("Georgia"));
    m_privacyTitleLabel->setFont(overlayTitleFont);
    m_privacyTitleLabel->setAlignment(Qt::AlignCenter);
    m_privacyTitleLabel->setStyleSheet(QStringLiteral("color: #5a2f13;"));

    m_privacyTextLabel = new QLabel(overlayPanel);
    m_privacyTextLabel->setWordWrap(true);
    m_privacyTextLabel->setAlignment(Qt::AlignCenter);
    m_privacyTextLabel->setStyleSheet(QStringLiteral("color: #4a2b15; font-size: 10.5pt;"));

    m_privacyRevealButton = new QPushButton(QStringLiteral("Показать ход"), overlayPanel);
    m_privacyRevealButton->setMinimumHeight(40);

    overlayPanelLayout->addWidget(m_privacyTitleLabel);
    overlayPanelLayout->addWidget(m_privacyTextLabel);
    overlayPanelLayout->addWidget(m_privacyRevealButton);
    overlayLayout->addWidget(overlayPanel);

    m_privacyOverlay->hide();
    m_revealTurnButton->hide();

    const auto updateFullscreenButton = [this, fullscreenButton]() {
        fullscreenButton->setText(isFullScreen()
                                      ? QStringLiteral("Выйти из полного экрана")
                                      : QStringLiteral("Полный экран"));
    };

    const auto enterOrLeaveFullscreen = [this, updateFullscreenButton]() {
        if (isFullScreen())
        {
            showMaximized();
        }
        else
        {
            showFullScreen();
        }
        updateFullscreenButton();
    };

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
    connect(m_applyNamesButton, &QPushButton::clicked, this, &MainWindow::applyPlayerNames);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshTavern);
    connect(m_buyButton, &QPushButton::clicked, this, &MainWindow::buySelectedCard);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::playSelectedCard);
    connect(m_sellButton, &QPushButton::clicked, this, &MainWindow::sellSelectedMinion);
    connect(m_attackMinionButton, &QPushButton::clicked, this, &MainWindow::attackSelectedMinion);
    connect(m_attackHeroButton, &QPushButton::clicked, this, &MainWindow::attackEnemyHero);
    connect(m_readyButton, &QPushButton::clicked, this, &MainWindow::toggleReady);
    connect(m_switchPlayerButton, &QPushButton::clicked, this, &MainWindow::switchSelfPlayer);
    connect(m_revealTurnButton, &QPushButton::clicked, this, &MainWindow::revealSelfTurn);
    connect(m_privacyRevealButton, &QPushButton::clicked, this, &MainWindow::revealSelfTurn);
    connect(fullscreenButton, &QPushButton::clicked, this, enterOrLeaveFullscreen);
    connect(m_tavernList, &QListWidget::itemSelectionChanged, this, &MainWindow::updateUi);
    connect(m_handList, &QListWidget::itemSelectionChanged, this, &MainWindow::updateUi);
    connect(m_localBoardList, &QListWidget::itemSelectionChanged, this, &MainWindow::updateUi);
    connect(m_remoteBoardList, &QListWidget::itemSelectionChanged, this, &MainWindow::updateUi);

    auto *fullscreenShortcut = new QShortcut(QKeySequence(QStringLiteral("F11")), this);
    connect(fullscreenShortcut, &QShortcut::activated, this, enterOrLeaveFullscreen);

    auto *exitFullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(exitFullscreenShortcut, &QShortcut::activated, this, [this, updateFullscreenButton]() {
        if (isFullScreen())
        {
            showMaximized();
            updateFullscreenButton();
        }
    });

    updateFullscreenButton();
}

void MainWindow::updateUi()
{
    const QSignalBlocker tavernBlocker(m_tavernList);
    const QSignalBlocker handBlocker(m_handList);
    const QSignalBlocker localBoardBlocker(m_localBoardList);
    const QSignalBlocker remoteBoardBlocker(m_remoteBoardList);
    const int tavernRow = m_tavernList->currentRow();
    const int handRow = m_handList->currentRow();
    const int localBoardRow = m_localBoardList->currentRow();
    const int remoteBoardRow = m_remoteBoardList->currentRow();

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

    if (selfPlay && m_selfPlayActive && gameOver)
    {
        const AutoBattlerGame::PlayerState &first = m_selfPlayers[0];
        const AutoBattlerGame::PlayerState &second = m_selfPlayers[1];

        if (first.hero.health <= 0 && second.hero.health <= 0)
        {
            statusText = QStringLiteral("Оба игрока довели друг друга до 0 здоровья. Ничья.");
        }
        else if (first.hero.health <= 0)
        {
            statusText = QStringLiteral("%1 победил: здоровье %2 опустилось до 0.")
                             .arg(second.hero.name, first.hero.name);
        }
        else if (second.hero.health <= 0)
        {
            statusText = QStringLiteral("%1 победил: здоровье %2 опустилось до 0.")
                             .arg(first.hero.name, second.hero.name);
        }
    }
    else if (!selfPlay && gameOver)
    {
        const AutoBattlerGame::PlayerState &local = m_game.localPlayer();
        const AutoBattlerGame::PlayerSnapshot &remote = m_game.remotePlayer();

        if (local.hero.health <= 0 && remote.heroHealth <= 0)
        {
            statusText = QStringLiteral("Оба героя пали. Ничья.");
        }
        else if (local.hero.health <= 0)
        {
            statusText = QStringLiteral("Поражение: здоровье вашего героя опустилось до 0.");
        }
        else if (remote.heroHealth <= 0)
        {
            statusText = QStringLiteral("Победа: здоровье героя соперника опустилось до 0.");
        }
    }

    m_connectionLabel->setText(connectionText);
    m_activePlayerLabel->setText(activePlayerText);
    m_roundLabel->setText(roundText);
    m_goldLabel->setText(goldText);
    m_localHeroLabel->setText(currentHeroText);
    m_remoteHeroLabel->setText(opponentHeroText);
    m_statusLabel->setText(statusText);

    if (tavernRow >= 0 && tavernRow < m_tavernList->count())
    {
        m_tavernList->setCurrentRow(tavernRow);
    }
    if (handRow >= 0 && handRow < m_handList->count())
    {
        m_handList->setCurrentRow(handRow);
    }
    if (localBoardRow >= 0 && localBoardRow < m_localBoardList->count())
    {
        m_localBoardList->setCurrentRow(localBoardRow);
    }
    if (remoteBoardRow >= 0 && remoteBoardRow < m_remoteBoardList->count())
    {
        m_remoteBoardList->setCurrentRow(remoteBoardRow);
    }

    updateTransportUi();

    m_refreshButton->setEnabled(buyPhase);
    m_buyButton->setEnabled(buyPhase && m_tavernList->count() > 0);
    m_playButton->setEnabled(buyPhase && m_handList->count() > 0);
    m_sellButton->setEnabled(buyPhase && m_localBoardList->count() > 0);
    m_attackMinionButton->setEnabled(buyPhase && m_localBoardList->currentRow() >= 0 && m_remoteBoardList->currentRow() >= 0);
    m_attackHeroButton->setEnabled(buyPhase && m_localBoardList->currentRow() >= 0);

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
