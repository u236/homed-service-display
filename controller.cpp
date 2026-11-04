#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(SERVICE_VERSION, configFile), m_displays(new DisplayList(getConfig(), this)), m_commands(QMetaEnum::fromType <Command> ()), m_events(QMetaEnum::fromType <Event> ())
{
    m_displays->init();
}

Device Controller::findDevice(const QString &search)
{
    for (auto it = m_devices.begin(); it != m_devices.end(); it++)
        if (search == it.value()->key() || search.startsWith(it.value()->key().append('/')) || search == it.value()->topic() || search.startsWith(it.value()->topic().append('/')))
            return it.value();

    return Device();
}

quint8 Controller::getEndpointId(const QString &endpoint)
{
    QList <QString> list = endpoint.split('/');

    if (list.count() > 2)
        return static_cast <quint8> (list.last().toInt());

    return 0;
}

void Controller::publishLayout(DisplayObject *display) // TODO: check if used more than once
{
    QList <QByteArray> list = display->layout(); // TODO: check this
    QString topic = QString("hasp/%1/command").arg(display->id());
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

    mqttPublishString(QString("%1/page").arg(topic), QString::number(display->home()));
}

void Controller::publishAvailability(DisplayObject *display, const QString &filter)
{
    for (int i = 0; i < display->items().count(); i++)
    {
        const DisplayItem &item = display->items().at(i);
        const Device &device = findDevice(item.endpoint);

        if (filter.isEmpty() || filter == item.endpoint || item.endpoint.startsWith(QString("%1/").arg(filter)))
        {
            QString topic = QString("hasp/%1/command/p%2b").arg(display->id()).arg(item.page);
            bool check = !device.isNull() && device->available();

            mqttPublishString(QString("%1%2.enabled").arg(topic).arg(item.id), check ? "1" : "0");
            mqttPublishString(QString("%1%2.text_color").arg(topic).arg(item.id), check ? TEXT_COLOR : DISABLED_COLOR);
            mqttPublishString(QString("%1%2.text_color").arg(topic).arg(item.id + 1), check ? HEADER_COLOR : DISABLED_COLOR);
        }
    }
}

void Controller::publishData(DisplayObject *display, const QString &filter)
{
    QList <QString> list = {"switch", "light", "thermostat"};

    for (int i = 0; i < display->items().count(); i++)
    {
        const DisplayItem &item = display->items().at(i);
        const Device &device = findDevice(item.endpoint);

        if (!device.isNull() && (filter.isEmpty() || filter == item.endpoint || item.endpoint.startsWith(QString("%1/").arg(filter))))
        {
            QMap <QString, QVariant> properties = device->properties().value(getEndpointId(item.endpoint));
            QString itemName = item.expose.split('_').value(0), topic = QString("hasp/%1/command/p%2b%3").arg(display->id()).arg(item.page).arg(item.id);

            if (properties.isEmpty())
                continue;

            switch (list.indexOf(itemName))
            {
                case 0: // switch
                case 1: // light
                {
                    mqttPublishString(QString("%1.val").arg(topic), properties.value(QString(item.expose).replace(itemName, "status")).toString() == "on" ? "1" : "0");
                    break;
                }

                case 2: // thermostat
                {
                    mqttPublishString(QString("hasp/%1/command/p%2b%3.text").arg(display->id()).arg(item.page).arg(item.id + 2), properties.value("targetTemperature").toString());
                    mqttPublishString(QString("%1.text").arg(topic), properties.value("temperature").toString());
                    mqttPublishString(QString("%1.text_color").arg(topic), properties.value("running").toBool() ? ACCENT_COLOR : TEXT_COLOR);
                    break;
                }

                default:
                {
                    QVariant value = properties.value(item.expose);

                    if (!value.isValid())
                        break;

                    if (value.type() == QVariant::Bool)
                    {
                        bool check = itemName == "contact" ? !value.toBool() : value.toBool();

                        if (itemName == "contact")
                            mqttPublishString(QString("%1.text").arg(topic), QChar(check ? 0xE81C : 0xE81B));

                        mqttPublishString(QString("%1.text_color").arg(topic), check ? itemName == "waterLeak" ? ALARM_COLOR : ACCENT_COLOR : TEXT_COLOR);
                        break;
                    }

                    mqttPublishString(QString("%1.text").arg(topic), value.toString());
                    break;
                }
            }
        }
    }
}

void Controller::publishEvent(const QString &name, Event event)
{
    mqttPublish(mqttTopic("event/%1").arg(serviceTopic()), {{"device", name}, {"event", m_events.valueToKey(static_cast <int> (event))}});
}

void Controller::updateAvailability(const QString &key)
{
    for (int i = 0; i < m_displays->count(); i++)
    {
        const Display &display = m_displays->at(i);

        if (display->availability() != Availability::Online)
            continue;

        publishAvailability(display.data(), key);
        publishData(display.data(), key);
    }
}

void Controller::displayEvent(DisplayObject *display, Event event)
{
    QString topic = QString("hasp/%1").arg(display->id());

    switch (event)
    {
        case Event::aboutToRename:
        case Event::removed:
            mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_displays->names() ? display->name() : display->id()), QJsonObject(), true);
            mqttUnsubscribe(QString("%1/LWT").arg(topic));
            mqttUnsubscribe(QString("%1/state/#").arg(topic));
            break;

        case Event::added:
        case Event::updated:
            QTimer::singleShot(SUBSCRIPTION_DELAY, this, [this, topic] () { mqttSubscribe(QString("%1/LWT").arg(topic)); mqttSubscribe(QString("%1/state/#").arg(topic)); });
            mqttUnsubscribe(QString("%1/LWT").arg(topic));
            break;

        default:
            break;
    }

    publishEvent(display->name(), event);
}

void Controller::quit(void)
{
    for (int i = 0; i < m_displays->count(); i++)
    {
        const Display &display = m_displays->at(i);
        mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_displays->names() ? display->name() : display->id()), {{"status", "offline"}}, true);
    }

    delete m_displays;
    HOMEd::quit();
}

void Controller::mqttConnected(void)
{
    mqttSubscribe(mqttTopic("command/%1").arg(serviceTopic()));
    mqttSubscribe(mqttTopic("service/#"));

    for (int i = 0; i < m_displays->count(); i++)
    {
        mqttSubscribe(QString("hasp/%1/LWT").arg(m_displays->at(i)->id()));
        mqttSubscribe(QString("hasp/%1/state/#").arg(m_displays->at(i)->id()));
    }

    m_devices.clear();
    m_displays->store();

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
                Display display = m_displays->byName(json.value("device").toString(), &index), other = m_displays->byName(id);

                if (display != other && !other.isNull())
                {
                    logWarning << "Device" << id << "update failed, identifier already in use";
                    publishEvent(name, Event::idDuplicate);
                    break;
                }

                other = m_displays->byName(name);

                if (display != other && !other.isNull())
                {
                    logWarning << "Device" << name << "update failed, name already in use";
                    publishEvent(name, Event::nameDuplicate);
                    break;
                }

                if (!display.isNull() && (display->id() != id || display->name() != name))
                    displayEvent(display.data(), Event::aboutToRename);

                display = m_displays->parse(data);

                if (display.isNull())
                {
                    logWarning << "Device" << name << "update failed, data is incomplete";
                    publishEvent(name, Event::incompleteData);
                    break;
                }

                if (index >= 0)
                {
                    m_displays->replace(index, display);
                    logInfo << display << "successfully updated";
                    displayEvent(display.data(), Event::updated);
                }
                else
                {
                    m_displays->append(display);
                    logInfo << display << "successfully added";
                    displayEvent(display.data(), Event::added);
                }

                m_displays->store(true);
                break;
            }

            case Command::removeDevice:
            {
                int index = -1;
                const Display &display = m_displays->byName(json.value("device").toString(), &index);

                if (index >= 0)
                {
                    m_displays->removeAt(index);
                    logInfo << display << "removed";
                    displayEvent(display.data(), Event::removed);
                    m_displays->store(true);
                }

                break;
            }
        }
    }
    else if (subTopic.startsWith("service/"))
    {
        QString type = subTopic.split('/').value(1), service = subTopic.mid(subTopic.indexOf('/') + 1);

        if (coreServices().contains(type) || service == serviceTopic())
            return;

        if (json.value("status").toString() == "online")
        {
            mqttSubscribe(mqttTopic("status/%1").arg(service));
            return;
        }

        for (auto it = m_devices.begin(); it != m_devices.end(); it++)
        {
            const Device &device = it.value();

            if (!device->topic().startsWith(QString("%1/").arg(service)))
                continue;

            mqttUnsubscribe(mqttTopic("device/%1").arg(device->topic()));
            mqttUnsubscribe(mqttTopic("fd/%1/#").arg(device->topic()));

            device->clearTopic();
            device->setAvailable(false);

            updateAvailability(device->key());
        }

        mqttUnsubscribe(mqttTopic("status/%1").arg(service));
    }
    else if (subTopic.startsWith("status/"))
    {
        QString type = subTopic.split('/').value(1), service = subTopic.mid(subTopic.indexOf('/') + 1);
        QJsonArray devices = json.value("devices").toArray();
        bool names = json.value("names").toBool();

        if (coreServices().contains(type) || service == serviceTopic())
            return;

        for (auto it = devices.begin(); it != devices.end(); it++)
        {
            QJsonObject item = it->toObject();
            QString name = item.value("name").toString(), id = deviceId(item, type), key, topic;
            bool check = false;

            if (type == "zigbee" && (item.value("removed").toBool() || !item.value("logicalType").toInt()))
                continue;

            if (name.isEmpty())
                name = id;

            key = QString("%1/%2").arg(type, id);
            topic = QString("%1/%2").arg(service, names ? name : id);

            if (m_devices.contains(key))
            {
                const Device &device = m_devices.value(key);

                if (device->topic() != topic)
                {
                    if (!device->topic().isEmpty())
                    {
                        mqttUnsubscribe(mqttTopic("device/%1").arg(device->topic()));
                        mqttUnsubscribe(mqttTopic("fd/%1/#").arg(device->topic()));
                    }

                    device->setTopic(topic);
                    check = true;
                }
            }
            else
            {
                m_devices.insert(key, Device(new DeviceObject(key, topic)));
                updateAvailability(key);
                check = true;
            }

            if (check)
            {
                mqttSubscribe(mqttTopic("device/%1").arg(topic));
                mqttSubscribe(mqttTopic("fd/%1/#").arg(topic));
                mqttPublish(mqttTopic("command/%1").arg(service), {{"action", "getProperties"}, {"device", names ? name : id}, {"service", "display"}});
            }
        }
    }
    else if (subTopic.startsWith("device/"))
    {
        const Device &device = findDevice(subTopic.mid(subTopic.indexOf('/') + 1));
        bool status = json.value("status").toString() == "online";

        if (device.isNull() || device->available() == status)
            return;

        device->setAvailable(status);
        updateAvailability(device->key());
    }
    else if (subTopic.startsWith("fd/"))
    {
        QString string = subTopic.mid(subTopic.indexOf('/') + 1);
        const Device &device = findDevice(string);

        if (device.isNull())
            return;

        device->properties().insert(getEndpointId(string.replace(device->topic(), device->key())), json.toVariantMap());

        for (int i = 0; i < m_displays->count(); i++)
        {
            const Display &display = m_displays->at(i);

            if (display->availability() != Availability::Online)
                continue;

            publishData(display.data(), string);
        }
    }
    else if (topic.name().startsWith("hasp/") && topic.name().endsWith("/LWT"))
    {
        const Display &display = m_displays->byName(topic.name().split('/').value(1));

        if (display.isNull() || !display->active())
            return;

        display->setAvailability(message == "online" ? Availability::Online : Availability::Offline);
        mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_displays->names() ? display->name() : display->id()), {{"status", display->availability() == Availability::Online ? "online" : "offline"}}, true);

        if (display->availability() != Availability::Online)
            return;

        publishLayout(display.data());
        publishAvailability(display.data());
        publishData(display.data());
    }
    else if (topic.name().startsWith("hasp/") && topic.name().split('/').value(2) == "state")
    {
        QRegExp expression("p(\\d+)b(\\d+)");
        QList <QString> list = topic.name().split('/');
        const Display &display = m_displays->byName(list.value(1));

        if (display.isNull() || display->availability() != Availability::Online || !expression.exactMatch(list.value(3)) || json.value("event").toString() != "up")
            return;

        for (int i = 0; i < display->items().count(); i++)
        {
            const DisplayItem &item = display->items().at(i);

            if (item.page == expression.cap(1).toInt() && item.id == expression.cap(2).toInt())
            {
                QString itemName = item.expose.split('_').value(0);
                const Device &device = findDevice(item.endpoint);

                if (device.isNull() || !device->available())
                    break;

                if (itemName == "switch" || itemName == "light")
                {
                    quint8 endpointId = getEndpointId(item.endpoint);
                    mqttPublish(mqttTopic("td/").append(endpointId ? QString("%1/%2").arg(device->topic()).arg(endpointId) : device->topic()), {{QString(item.expose).replace(itemName, "status"), json.value("val").toInt() ? "on" : "off"}});
                }

                break;
            }
        }
    }
}
