#include <QCryptographicHash>
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

void Controller::publishLayout(DisplayObject *display, const QList <QByteArray> &list)
{
    QByteArray data;

    for (int i = 0; i < list.count(); i++)
    {
        data.append(list.at(i));

        if (list.count() > i + 1 && data.length() + list.at(i + 1).length() <= CHUNK_SIZE)
            continue;

        mqttPublishString(QString("hasp/%1/command/jsonl").arg(display->id()), QString::fromUtf8(data));
        data.clear();
    }
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
    const Device &device = findDevice(display->endpoint());

    if (!device.isNull() && (filter.isEmpty() || filter == display->endpoint() || display->endpoint().startsWith(QString("%1/").arg(filter))))
    {
        QList <QString> list = {"level", "colorTemperature", "color", "temperature", "targetTemperature", "systemMode", "operationMode"};
        quint8 endpointId = getEndpointId(display->endpoint());
        QMap <QString, QVariant> properties = device->properties().value(endpointId);
        QJsonObject options = device->exposes().value(endpointId ? QString::number(endpointId) : "common").toObject().value("options").toObject();

        for (int i = 0; i < display->properties().count(); i++)
        {
            QString property = display->properties().at(i), itemName = property.split('_').value(0), topic = QString("hasp/%1/command/p0b").arg(display->id());
            int id = WINDOW_ID + 3 + i * 2;

            if (!properties.contains(property))
                continue;

            switch (list.indexOf(itemName))
            {
                case 0: // level
                case 1: // colorTemperature
                {
                    mqttPublishString(QString("%1%2.val").arg(topic).arg(id), QString::number(properties.value(property).toInt()));
                    break;
                }

                case 2: // color
                {
                    QList <QVariant> color = properties.value(property).toList();
                    mqttPublishString(QString("%1%2.color").arg(topic).arg(id), QString::asprintf("#%02x%02x%02x", color.value(0).toInt(), color.value(1).toInt(), color.value(2).toInt()));
                    break;
                }

                case 3: // temperature
                {
                    mqttPublishString(QString("%1%2.text").arg(topic).arg(id), properties.value(property).toString());
                    break;
                }

                case 4: // targetTemperature
                {
                    mqttPublishString(QString("%1%2.val").arg(topic).arg(id), QString::number(qRound(properties.value(property).toDouble() / options.value(property).toObject().value("step").toDouble(1))));
                    mqttPublishString(QString("%1%2.text").arg(topic).arg(id + 1), properties.value(property).toString());
                    break;
                }

                case 5: // systemMode
                case 6: // operationMode
                {
                    QJsonValue value = options.value(property).toObject().value("enum");
                    QList <QVariant> items = value.isArray() ? value.toArray().toVariantList() : value.toObject().toVariantMap().values();

                    for (int j = 0; j < items.count(); j++)
                        mqttPublishString(QString("%1%2.bg_color").arg(topic).arg(WINDOW_ID + 20 + i * 10 + j), items.at(j) == properties.value(property) ? ACCENT_COLOR : BUTTON_COLOR);

                    break;
                }
            }
        }
    }

    for (int i = 0; i < display->items().count(); i++)
    {
        const DisplayItem &item = display->items().at(i);
        const Device &device = findDevice(item.endpoint);

        if (!device.isNull() && (filter.isEmpty() || filter == item.endpoint || item.endpoint.startsWith(QString("%1/").arg(filter))))
        {
            QList <QString> list = {"switch", "light", "thermostat"};
            QMap <QString, QVariant> properties = device->properties().value(getEndpointId(item.endpoint));
            QString itemName = item.expose.split('_').value(0), topic = QString("hasp/%1/command/p%2b%3").arg(display->id()).arg(item.page).arg(item.id);

            if (properties.isEmpty())
                continue;

            switch (list.indexOf(itemName))
            {
                case 0: // switch
                case 1: // light
                {
                    mqttPublishString(QString("%1.bg_color").arg(topic), properties.value(QString(item.expose).replace(itemName, "status")).toString() == "on" ? ACCENT_COLOR : BUTTON_COLOR);
                    break;
                }

                case 2: // thermostat
                {
                    mqttPublishString(QString("hasp/%1/command/p%2b%3.text").arg(display->id()).arg(item.page).arg(item.id + 2), properties.value("temperature").toString());
                    mqttPublishString(QString("%1.text").arg(topic), properties.value("targetTemperature").toString());
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

void Controller::updateAvailability(const Device &device)
{
    for (int i = 0; i < m_displays->count(); i++)
    {
        const Display &display = m_displays->at(i);

        if (display->availability() != Availability::Online)
            continue;

        if (!device->available() && (display->endpoint() == device->key() || display->endpoint().startsWith(QString("%1/").arg(device->key()))))
        {
            mqttPublishString(QString("hasp/%1/command/p0b%2.hidden").arg(display->id()).arg(WINDOW_ID), "1");
            display->setEndpoint(QString());
            display->properties().clear();
        }

        publishAvailability(display.data(), device->key());
        publishData(display.data(), device->key());
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
            mqttUnsubscribe(mqttTopic("expose/%1").arg(device->topic()));
            mqttUnsubscribe(mqttTopic("fd/%1/#").arg(device->topic()));

            device->clearTopic();
            device->setAvailable(false);

            updateAvailability(device);
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
                        mqttUnsubscribe(mqttTopic("expose/%1").arg(device->topic()));
                        mqttUnsubscribe(mqttTopic("fd/%1/#").arg(device->topic()));
                    }

                    device->setTopic(topic);
                    check = true;
                }
            }
            else
            {
                m_devices.insert(key, Device(new DeviceObject(key, topic)));
                updateAvailability(m_devices.value(key));
                check = true;
            }

            if (check)
            {
                mqttSubscribe(mqttTopic("device/%1").arg(topic));
                mqttSubscribe(mqttTopic("expose/%1").arg(topic));
                m_devices.value(key)->setHash(QByteArray());
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
        updateAvailability(device);
    }
    else if (subTopic.startsWith("expose/"))
    {
        const Device &device = findDevice(subTopic.mid(subTopic.indexOf('/') + 1));
        QByteArray hash = QCryptographicHash::hash(message, QCryptographicHash::Md5);

        if (device.isNull() || device->hash() == hash)
            return;

        device->setExposes(json);
        device->setHash(hash);

        mqttSubscribe(mqttTopic("fd/%1/#").arg(device->topic()));
        mqttPublish(mqttTopic("command/%1").arg(device->topic().mid(0, device->topic().lastIndexOf('/'))), {{"action", "getProperties"}, {"device", device->topic().split('/').last()}, {"service", "display"}});
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

        if (!display.isNull() && display->active())
        {
            QString topic = QString("hasp/%1/command").arg(display->id());

            display->setAvailability(message == "online" ? Availability::Online : Availability::Offline);
            mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_displays->names() ? display->name() : display->id()), {{"status", display->availability() == Availability::Online ? "online" : "offline"}}, true);

            if (display->availability() != Availability::Online)
                return;

            mqttPublishString(QString("%1/clearpage").arg(topic), "all");
            publishLayout(display.data(), display->layout());
            mqttPublishString(QString("%1/page").arg(topic), QString::number(display->home()));

            publishAvailability(display.data());
            publishData(display.data());
        }
    }
    else if (topic.name().startsWith("hasp/") && topic.name().split('/').value(2) == "state")
    {
        QRegExp expression("p(\\d+)b(\\d+)");
        QList <QString> list = topic.name().split('/');
        QString event = json.value("event").toString();
        const Display &display = m_displays->byName(list.value(1));

        if (display.isNull() || display->availability() != Availability::Online || !expression.exactMatch(list.value(3)))
            return;

        if (list.value(3) == QString("p0b%1").arg(WINDOW_ID) && json.value("hidden").toBool())
        {
            display->setEndpoint(QString());
            display->properties().clear();
            return;
        }

        if (event == "changed" && !expression.cap(1).toInt() && display->properties().value((expression.cap(2).toInt() - WINDOW_ID - 3) / 2) == "targetTemperature")
        {
            const Device &device = findDevice(display->endpoint());

            if (!device.isNull())
            {
                quint8 endpointId = getEndpointId(display->endpoint());
                mqttPublishString(QString("hasp/%1/command/p0b%2.text").arg(display->id()).arg(expression.cap(2).toInt() + 1), QString::number(json.value("val").toInt() * device->exposes().value(endpointId ? QString::number(endpointId) : "common").toObject().value("options").toObject().value("targetTemperature").toObject().value("step").toDouble(1)));
            }

            return;
        }

        if (event != "long" && event != "up")
            return;

        for (int i = 0; i < display->items().count(); i++)
        {
            const DisplayItem &item = display->items().at(i);

            if (item.page == expression.cap(1).toInt() && item.id == expression.cap(2).toInt())
            {
                QString itemName = item.expose.split('_').value(0);
                quint8 endpointId = getEndpointId(item.endpoint);
                const Device &device = findDevice(item.endpoint);

                if (device.isNull() || !device->available())
                    break;

                if (event == "up" && (itemName == "switch" || itemName == "light"))
                {
                    mqttPublish(mqttTopic("td/").append(endpointId ? QString("%1/%2").arg(device->topic()).arg(endpointId) : device->topic()), {{QString(item.expose).replace(itemName, "status"), "toggle"}});
                    break;
                }

                if (itemName == "light" || itemName == "thermostat")
                {
                    QJsonObject options = device->exposes().value(endpointId ? QString::number(endpointId) : "common").toObject().value("options").toObject();
                    QJsonArray controls = itemName == "light" ? options.value(item.expose).toArray() : QJsonArray {"temperature", "targetTemperature", "systemMode", "operationMode"};

                    display->setEndpoint(item.endpoint);
                    display->properties().clear();

                    for (auto it = controls.begin(); it != controls.end(); it++)
                    {
                        QString property = QString(item.expose).replace(itemName, it->toString());

                        if (property.startsWith("colorMode") || (itemName == "thermostat" && property != "temperature" && !options.contains(property)))
                            continue;

                        display->properties().append(property);
                    }

                    mqttPublishString(QString("hasp/%1/command/p0b%2.clear").arg(display->id()).arg(WINDOW_ID), QString());
                    publishLayout(display.data(), display->window(item.name, options));
                    publishData(display.data(), item.endpoint);
                    mqttPublishString(QString("hasp/%1/command/p0b%2.hidden").arg(display->id()).arg(WINDOW_ID), "0");
                }

                break;
            }
        }
    }
}
