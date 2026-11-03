#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(SERVICE_VERSION, configFile), m_devices(new DeviceList(getConfig(), this)), m_commands(QMetaEnum::fromType <Command> ()), m_events(QMetaEnum::fromType <Event> ())
{
    m_devices->init();
}

void Controller::publishLayout(DeviceObject *device) // TODO: check if used more than once
{
    QList <QByteArray> list = device->layout(); // TODO: check this
    QString topic = QString("hasp/%1/command").arg(device->id());
    QByteArray data;

    mqttPublishString(QString("%1/clearpage").arg(topic), "all");

    for (int i = 0; i < list.count(); i++)
    {
        data.append(list.at(i));

        if (list.count() > i + 1 && data.length() + list.at(i + 1).length() <= CHUNK_SIZE)
            continue;

        mqttPublishString(QString("%1/jsonl").arg(topic), QString::fromUtf8(data));
        data.clear();
    }

    mqttPublishString(QString("%1/page").arg(topic), QString::number(device->home()));
}

void Controller::publishEvent(const QString &name, Event event)
{
    mqttPublish(mqttTopic("event/%1").arg(serviceTopic()), {{"device", name}, {"event", m_events.valueToKey(static_cast <int> (event))}});
}

void Controller::deviceEvent(DeviceObject *device, Event event)
{
    QString topic = QString("hasp/%1/LWT").arg(device->id());

    switch (event)
    {
        case Event::aboutToRename:
        case Event::removed:
            mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_devices->names() ? device->name() : device->id()), QJsonObject(), true);
            mqttUnsubscribe(topic);
            break;

        case Event::added:
        case Event::updated:
            mqttUnsubscribe(topic);
            mqttSubscribe(topic);
            break;

        default:
            break;
    }

    publishEvent(device->name(), event);
}

void Controller::quit(void)
{
    for (int i = 0; i < m_devices->count(); i++)
    {
        const Device &device = m_devices->at(i);
        mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_devices->names() ? device->name() : device->id()), {{"status", "offline"}}, true);
    }

    delete m_devices;
    HOMEd::quit();
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("command/%1").arg(serviceTopic()));

    for (int i = 0; i < m_devices->count(); i++)
        mqttSubscribe(QString("hasp/%1/LWT").arg(m_devices->at(i)->id()));

    m_devices->store();
    mqttPublishService();
}

void Controller::mqttReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString subTopic = topic.name().replace(0, mqttTopic().length(), QString());
    QJsonObject json = QJsonDocument::fromJson(message).object();

    if (subTopic == QString("command/%1").arg(serviceTopic()))
    {
        switch (static_cast <Command> (m_commands.keyToValue(json.value("action").toString().toUtf8().constData())))
        {
            case Command::restartService:
            {
                logWarning << "Restart request received...";
                mqttPublish(topic.name(), QJsonObject(), true);
                QCoreApplication::exit(EXIT_RESTART);
                break;
            }

            case Command::updateDevice:
            {
                int index = -1;
                QJsonObject data = json.value("data").toObject();
                QString id = mqttSafe(data.value("id").toString()), name = mqttSafe(data.value("name").toString());
                Device device = m_devices->byName(json.value("device").toString(), &index), other = m_devices->byName(id);

                if (device != other && !other.isNull())
                {
                    logWarning << "Device" << id << "update failed, identifier already in use";
                    publishEvent(name, Event::idDuplicate);
                    break;
                }

                other = m_devices->byName(name);

                if (device != other && !other.isNull())
                {
                    logWarning << "Device" << name << "update failed, name already in use";
                    publishEvent(name, Event::nameDuplicate);
                    break;
                }

                if (!device.isNull() && (device->id() != id || device->name() != name))
                    deviceEvent(device.data(), Event::aboutToRename);

                device = m_devices->parse(data);

                if (device.isNull())
                {
                    logWarning << "Device" << name << "update failed, data is incomplete";
                    publishEvent(name, Event::incompleteData);
                    break;
                }

                if (index >= 0)
                {
                    m_devices->replace(index, device);
                    logInfo << device << "successfully updated";
                    deviceEvent(device.data(), Event::updated);
                }
                else
                {
                    m_devices->append(device);
                    logInfo << device << "successfully added";
                    deviceEvent(device.data(), Event::added);
                }

                m_devices->store(true);
                break;
            }

            case Command::removeDevice:
            {
                int index = -1;
                const Device &device = m_devices->byName(json.value("device").toString(), &index);

                if (index >= 0)
                {
                    m_devices->removeAt(index);
                    logInfo << device << "removed";
                    deviceEvent(device.data(), Event::removed);
                    m_devices->store(true);
                }

                break;
            }
        }
    }
    else if (topic.name().startsWith("hasp/") && topic.name().endsWith("/LWT"))
    {
        const Device &device = m_devices->byName(topic.name().split('/').value(1));

        if (device.isNull() || !device->active())
            return;

        device->setAvailability(message == "online" ? Availability::Online : Availability::Offline);
        mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_devices->names() ? device->name() : device->id()), {{"status", device->availability() == Availability::Online ? "online" : "offline"}}, true);

        if (device->availability() != Availability::Online)
            return;

        publishLayout(device.data());
    }
}
