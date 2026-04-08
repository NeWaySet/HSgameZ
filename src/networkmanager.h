#pragma once

#include <QIODevice>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

#ifdef HAS_QT_BLUETOOTH
#include <QBluetoothServiceInfo>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#endif

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    enum class Transport
    {
        None,
        Tcp,
        Bluetooth
    };

    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager() override;

    bool startTcpServer(quint16 port);
    void connectTcp(const QString &address, quint16 port);

    bool bluetoothSupported() const;
    bool startBluetoothServer(const QString &serviceName);
    void connectBluetooth(const QString &address);

    void disconnectTransport();
    bool isConnected() const;
    bool isHosting() const;
    Transport transport() const;
    void sendMessage(const QJsonObject &object);

signals:
    void connected();
    void disconnected();
    void messageReceived(const QJsonObject &object);
    void errorOccurred(const QString &message);
    void info(const QString &message);

private slots:
    void onTcpNewConnection();
    void onDeviceReadyRead();
    void onDeviceDisconnected();

#ifdef HAS_QT_BLUETOOTH
    void onBluetoothNewConnection();
#endif

private:
    void resetServers();
    void clearDevice();
    void attachTcpSocket(QTcpSocket *socket);

#ifdef HAS_QT_BLUETOOTH
    void attachBluetoothSocket(QBluetoothSocket *socket);
    static QBluetoothUuid serviceUuid();
#endif

    QTcpServer m_tcpServer;
    QTcpSocket *m_tcpSocket = nullptr;
    QIODevice *m_device = nullptr;
    QByteArray m_buffer;
    bool m_hosting = false;
    Transport m_transport = Transport::None;

#ifdef HAS_QT_BLUETOOTH
    QBluetoothServer *m_btServer = nullptr;
    QBluetoothSocket *m_btSocket = nullptr;
    QBluetoothServiceInfo m_btServiceInfo;
#endif
};
