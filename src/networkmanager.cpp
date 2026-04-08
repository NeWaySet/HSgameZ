#include "networkmanager.h"

#include <QHostAddress>
#include <QJsonDocument>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onTcpNewConnection);
}

NetworkManager::~NetworkManager()
{
    disconnectTransport();
}

bool NetworkManager::startTcpServer(quint16 port)
{
    disconnectTransport();
    m_hosting = false;

    if (!m_tcpServer.listen(QHostAddress::AnyIPv4, port))
    {
        emit errorOccurred(tr("Не удалось запустить TCP сервер: %1").arg(m_tcpServer.errorString()));
        return false;
    }

    m_transport = Transport::Tcp;
    m_hosting = true;
    emit info(tr("TCP сервер запущен на порту %1.").arg(port));
    return true;
}

void NetworkManager::connectTcp(const QString &address, quint16 port)
{
    disconnectTransport();
    m_hosting = false;
    m_transport = Transport::Tcp;

    auto *socket = new QTcpSocket(this);
    attachTcpSocket(socket);
    emit info(tr("Подключение по TCP к %1:%2...").arg(address).arg(port));
    socket->connectToHost(address, port);
}

bool NetworkManager::bluetoothSupported() const
{
#ifdef HAS_QT_BLUETOOTH
    return true;
#else
    return false;
#endif
}

bool NetworkManager::startBluetoothServer(const QString &serviceName)
{
#ifdef HAS_QT_BLUETOOTH
    disconnectTransport();
    m_hosting = false;

    m_btServer = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_btServer, &QBluetoothServer::newConnection, this, &NetworkManager::onBluetoothNewConnection);

    if (!m_btServer->listen(QBluetoothAddress(), 0))
    {
        emit errorOccurred(tr("Не удалось запустить Bluetooth сервер."));
        m_btServer->deleteLater();
        m_btServer = nullptr;
        return false;
    }

    m_btServiceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, serviceName);
    m_btServiceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription,
                                 QStringLiteral("Qt Autobattler over Bluetooth"));
    m_btServiceInfo.setServiceUuid(serviceUuid());
    m_btServiceInfo.registerService();

    m_transport = Transport::Bluetooth;
    m_hosting = true;
    emit info(tr("Bluetooth сервер запущен. Ожидание подключения."));
    return true;
#else
    Q_UNUSED(serviceName);
    emit errorOccurred(tr("Этот билд собран без поддержки Qt Bluetooth."));
    return false;
#endif
}

void NetworkManager::connectBluetooth(const QString &address)
{
#ifdef HAS_QT_BLUETOOTH
    disconnectTransport();
    m_hosting = false;
    m_transport = Transport::Bluetooth;

    auto *socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
    attachBluetoothSocket(socket);
    emit info(tr("Подключение по Bluetooth к %1...").arg(address));
    socket->connectToService(QBluetoothAddress(address), serviceUuid(), QIODevice::ReadWrite);
#else
    Q_UNUSED(address);
    emit errorOccurred(tr("Этот билд собран без поддержки Qt Bluetooth."));
#endif
}

void NetworkManager::disconnectTransport()
{
    clearDevice();
    resetServers();
    m_buffer.clear();
    m_hosting = false;
    m_transport = Transport::None;
}

bool NetworkManager::isConnected() const
{
    if (m_tcpSocket)
    {
        return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
    }

#ifdef HAS_QT_BLUETOOTH
    if (m_btSocket)
    {
        return m_btSocket->state() == QBluetoothSocket::ConnectedState;
    }
#endif

    return false;
}

bool NetworkManager::isHosting() const
{
    return m_hosting;
}

NetworkManager::Transport NetworkManager::transport() const
{
    return m_transport;
}

void NetworkManager::sendMessage(const QJsonObject &object)
{
    if (!m_device)
    {
        return;
    }

    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    m_device->write(payload);
}

void NetworkManager::onTcpNewConnection()
{
    QTcpSocket *incoming = m_tcpServer.nextPendingConnection();
    if (!incoming)
    {
        return;
    }

    if (m_device)
    {
        incoming->disconnectFromHost();
        incoming->deleteLater();
        emit info(tr("Подключение отклонено: уже есть активный соперник."));
        return;
    }

    attachTcpSocket(incoming);
    m_transport = Transport::Tcp;
    emit info(tr("Соперник подключился по TCP."));
    emit connected();
}

void NetworkManager::onDeviceReadyRead()
{
    if (!m_device)
    {
        return;
    }

    m_buffer.append(m_device->readAll());

    while (true)
    {
        const int lineEnd = m_buffer.indexOf('\n');
        if (lineEnd < 0)
        {
            break;
        }

        const QByteArray line = m_buffer.left(lineEnd).trimmed();
        m_buffer.remove(0, lineEnd + 1);

        if (line.isEmpty())
        {
            continue;
        }

        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject())
        {
            emit errorOccurred(tr("Получено повреждённое сетевое сообщение."));
            continue;
        }

        emit messageReceived(document.object());
    }
}

void NetworkManager::onDeviceDisconnected()
{
    clearDevice();
    emit disconnected();
    emit info(tr("Соединение закрыто."));
}

#ifdef HAS_QT_BLUETOOTH
void NetworkManager::onBluetoothNewConnection()
{
    if (!m_btServer)
    {
        return;
    }

    QBluetoothSocket *incoming = m_btServer->nextPendingConnection();
    if (!incoming)
    {
        return;
    }

    if (m_device)
    {
        incoming->close();
        incoming->deleteLater();
        emit info(tr("Bluetooth подключение отклонено: уже есть активный соперник."));
        return;
    }

    attachBluetoothSocket(incoming);
    m_transport = Transport::Bluetooth;
    emit info(tr("Соперник подключился по Bluetooth."));
    emit connected();
}
#endif

void NetworkManager::resetServers()
{
    if (m_tcpServer.isListening())
    {
        m_tcpServer.close();
    }

#ifdef HAS_QT_BLUETOOTH
    m_btServiceInfo.unregisterService();
    if (m_btServer)
    {
        m_btServer->close();
        m_btServer->deleteLater();
        m_btServer = nullptr;
    }
#endif
}

void NetworkManager::clearDevice()
{
    if (m_tcpSocket)
    {
        disconnect(m_tcpSocket, nullptr, this, nullptr);
        m_tcpSocket->disconnectFromHost();
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }

#ifdef HAS_QT_BLUETOOTH
    if (m_btSocket)
    {
        disconnect(m_btSocket, nullptr, this, nullptr);
        m_btSocket->close();
        m_btSocket->deleteLater();
        m_btSocket = nullptr;
    }
#endif

    m_device = nullptr;
}

void NetworkManager::attachTcpSocket(QTcpSocket *socket)
{
    m_tcpSocket = socket;
    m_device = socket;

    connect(socket, &QIODevice::readyRead, this, &NetworkManager::onDeviceReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &NetworkManager::onDeviceDisconnected);
    connect(socket, &QTcpSocket::connected, this, &NetworkManager::connected);
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_tcpSocket)
        {
            emit errorOccurred(m_tcpSocket->errorString());
        }
    });
}

#ifdef HAS_QT_BLUETOOTH
void NetworkManager::attachBluetoothSocket(QBluetoothSocket *socket)
{
    m_btSocket = socket;
    m_device = socket;

    connect(socket, &QIODevice::readyRead, this, &NetworkManager::onDeviceReadyRead);
    connect(socket, &QBluetoothSocket::disconnected, this, &NetworkManager::onDeviceDisconnected);
    connect(socket, &QBluetoothSocket::connected, this, &NetworkManager::connected);
}

QBluetoothUuid NetworkManager::serviceUuid()
{
    return QBluetoothUuid(QStringLiteral("4df5d1d6-34bf-4f88-af95-99e0d7f6a201"));
}
#endif
