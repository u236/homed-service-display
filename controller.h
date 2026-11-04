#ifndef CONTROLLER_H
#define CONTROLLER_H

#define SERVICE_VERSION     "1.0.0"
#define CHUNK_SIZE          1920
#define SUBSCRIPTION_DELAY  1000

#include <QMetaEnum>
#include "display.h"
#include "homed.h"

class DeviceObject
{

public:

    DeviceObject(const QString &key, const QString &topic) :
        m_key(key), m_topic(topic), m_available(true) {}

    inline QString key(void) { return m_key; }

    inline QString topic(void) { return m_topic; }
    inline void setTopic(const QString &value) { m_topic = value; }
    inline void clearTopic(void) { m_topic.clear(); }

    inline bool available(void) { return m_available; }
    inline void setAvailable(bool value) { m_available = value; }

    inline QJsonObject exposes(void) { return m_exposes; }
    inline void setExposes(const QJsonObject &value) { m_exposes = value; }

    inline QByteArray hash(void) { return m_hash; }
    inline void setHash(const QByteArray &value) { m_hash = value; }

    inline QMap <quint8, QMap <QString, QVariant>> &properties(void) { return m_properties; }

private:

    QString m_key, m_topic;
    bool m_available;

    QJsonObject m_exposes;
    QByteArray m_hash;

    QMap <quint8, QMap <QString, QVariant>> m_properties;

};

class Controller : public HOMEd
{
    Q_OBJECT

public:

    enum class Command
    {
        restartService,
        updateDevice,
        removeDevice
    };

    enum class Event
    {
        idDuplicate,
        nameDuplicate,
        incompleteData,
        aboutToRename,
        added,
        updated,
        removed
    };

    Controller(const QString &configFile);

    Q_ENUM(Command)
    Q_ENUM(Event)

private:

    DisplayList *m_displays;
    QMetaEnum m_commands, m_events;

    QMap <QString, Device> m_devices;

    Device findDevice(const QString &search);
    quint8 getEndpointId(const QString &endpoint);

    void publishLayout(DisplayObject *display, const QList <QByteArray> &list);
    void publishAvailability(DisplayObject *display, const QString &filter = QString());
    void publishData(DisplayObject *display, const QString &filter = QString());
    void publishEvent(const QString &name, Event event);

    void updateAvailability(const QString &key);
    void displayEvent(DisplayObject *display, Event event);

public slots:

    void quit(void) override;

private slots:

    void mqttConnected(void) override;
    void mqttReceived(const QByteArray &message, const QMqttTopicName &topic) override;

};

#endif
