#ifndef CONTROLLER_H
#define CONTROLLER_H

#define SERVICE_VERSION             "1.0.0"
#define CHUNK_SIZE                  1920
#define LONG_PRESS_DELAY            400
#define SUBSCRIPTION_DELAY          1000
#define UPDATE_PROPERTIES_DELAY     1000
#define UPDATE_DATA_DELAY           100
#define UPDATE_WATCHDOG_INTERVAL    10000

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

    QJsonObject options(quint8 endpointId, const QString &name = QString());

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
        removeDevice,
        getProperties
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

    QTimer *m_propertiesTimer, *m_dataTimer, *m_watchdogTimer;
    DisplayList *m_displays;

    QMetaEnum m_commands, m_events;
    QString m_haPrefix, m_haStatus;
    bool m_haEnabled, m_haUpdate;

    QMap <QString, Device> m_devices;
    QList <QString> m_automations, m_updates;

    QString valueString(const QVariant &value);

    Device findDevice(const QString &search);
    quint8 getEndpointId(const QString &endpoint);

    void publishCommands(DisplayObject *display, const QList <QString> &list);
    void publishExposes(DisplayObject *display, bool remove = false);
    void publishProperties(DisplayObject *display);
    void publishEvent(const QString &name, Event event);

    void addCommand(QList <QString> &list, quint16 page, quint16 id, const QJsonObject &data);

    void sendAvailability(DisplayObject *display, const QString &filter = QString());
    void sendData(DisplayObject *display, const QString &filter = QString());

    void displayEvent(DisplayObject *display, Event event);
    void showWindow(DisplayObject *display, const DisplayItem &item);
    void updateAvailability(const Device &device);

public slots:

    void quit(void) override;

private slots:

    void mqttConnected(void) override;
    void mqttReceived(const QByteArray &message, const QMqttTopicName &topic) override;

    void updateProperties(void);
    void updateData(void);
    void updateWatchdog(void);

};

#endif
