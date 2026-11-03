#ifndef CONTROLLER_H
#define CONTROLLER_H

#define SERVICE_VERSION     "1.0.0"
#define CHUNK_SIZE          1920
#define SUBSCRIPTION_DELAY  1000

#include <QMetaEnum>
#include "device.h"
#include "homed.h"

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

    DeviceList *m_devices;
    QMetaEnum m_commands, m_events;

    void publishLayout(DeviceObject *device);
    void publishEvent(const QString &name, Event event);
    void deviceEvent(DeviceObject *device, Event event);

public slots:

    void quit(void) override;

private slots:

    void mqttConnected(void) override;
    void mqttReceived(const QByteArray &message, const QMqttTopicName &topic) override;

};

#endif
