#include <QCryptographicHash>
#include "controller.h"
#include "logger.h"

QJsonObject DeviceObject::options(quint8 endpointId, const QString &name)
{
    QJsonObject options = m_exposes.value(endpointId ? QString::number(endpointId) : "common").toObject().value("options").toObject();
    return name.isEmpty() ? options : options.value(name).toObject();
}

Controller::Controller(const QString &configFile) : HOMEd(SERVICE_VERSION, configFile), m_timer(new QTimer(this)), m_displays(new DisplayList(getConfig(), this)), m_commands(QMetaEnum::fromType <Command> ()), m_events(QMetaEnum::fromType <Event> ())
{
    connect(m_timer, &QTimer::timeout, this, &Controller::update);
    m_timer->setSingleShot(true);
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

void Controller::publishCommands(DisplayObject *display, const QList <QString> &list)
{
    QString data;

    for (int i = 0; i < list.count(); i++)
    {
        data.append(list.at(i));

        if (list.count() > i + 1 && data.toUtf8().length() + list.at(i + 1).toUtf8().length() <= CHUNK_SIZE)
            continue;

        mqttPublishString(QString("hasp/%1/command/jsonl").arg(display->id()), data);
        data.clear();
    }
}

void Controller::publishEvent(const QString &name, Event event)
{
    mqttPublish(mqttTopic("event/%1").arg(serviceTopic()), {{"device", name}, {"event", m_events.valueToKey(static_cast <int> (event))}});
}

void Controller::addCommand(QList <QString> &list, quint16 page, quint16 id, const QJsonObject &data)
{
    QJsonObject json = {{"page", page}, {"id", id}};

    for (auto it = data.begin(); it != data.end(); it++)
        json.insert(it.key(), it.value());

    list.append(QJsonDocument(json).toJson(QJsonDocument::Compact).append('\n'));
}

void Controller::sendAvailability(DisplayObject *display, const QString &filter)
{
    QList <QString> list;

    for (int i = 0; i < display->items().count(); i++)
    {
        const DisplayItem &item = display->items().at(i);

        if (item.scene.isEmpty() && (filter.isEmpty() || filter == item.endpoint || item.endpoint.startsWith(QString("%1/").arg(filter))))
        {
            const Device &device = findDevice(item.endpoint);
            bool check = !device.isNull() && device->available();
            addCommand(list, item.page, item.id, {{"enabled", check}, {"text_color", check ? TEXT_COLOR : DISABLED_COLOR}});
            addCommand(list, item.page, item.id + 1, {{"text_color", check ? HEADER_COLOR : DISABLED_COLOR}});
        }
    }

    publishCommands(display, list);
}

void Controller::sendData(DisplayObject *display, const QString &filter)
{
    QList <QString> propertyList = {"level", "colorTemperature", "color", "temperature", "targetTemperature", "systemMode", "operationMode"}, exposeList = {"switch", "light", "thermostat"}, list;
    const Device &device = findDevice(display->endpoint());

    if (!device.isNull() && (filter.isEmpty() || filter == display->endpoint() || display->endpoint().startsWith(QString("%1/").arg(filter))))
    {
        quint8 endpointId = getEndpointId(display->endpoint());
        QMap <QString, QVariant> properties = device->properties().value(endpointId);

        for (int i = 0; i < display->properties().count(); i++)
        {
            QString property = display->properties().at(i), itemName = property.split('_').value(0);
            int id = WINDOW_ID + 3 + i * 2;

            if (id == display->control() || !properties.contains(property))
                continue;

            switch (propertyList.indexOf(itemName))
            {
                case 0: // level
                case 1: // colorTemperature
                {
                    addCommand(list, 0, id, {{"val", properties.value(property).toInt()}});
                    break;
                }

                case 2: // color
                {
                    QList <QVariant> color = properties.value(property).toList();
                    addCommand(list, 0, id, {{"color", QString::asprintf("#%02x%02x%02x", color.value(0).toInt(), color.value(1).toInt(), color.value(2).toInt())}});
                    break;
                }

                case 3: // temperature
                {
                    addCommand(list, 0, id, {{"text", properties.value(property).toString()}});
                    break;
                }

                case 4: // targetTemperature
                {
                    addCommand(list, 0, id, {{"val", qRound(properties.value(property).toDouble() / device->options(endpointId, property).value("step").toDouble(1))}});
                    addCommand(list, 0, id + 1, {{"text", properties.value(property).toString()}, {"text_color", properties.value("running").toBool() ? ACCENT_COLOR : TEXT_COLOR}});
                    break;
                }

                case 5: // systemMode
                case 6: // operationMode
                {
                    QJsonValue value = device->options(endpointId, property).value("enum");
                    QList <QVariant> items = value.isArray() ? value.toArray().toVariantList() : value.toObject().toVariantMap().values();

                    for (int j = 0; j < items.count(); j++)
                        addCommand(list, 0, WINDOW_ID + 20 + i * 10 + j, {{"bg_color", items.at(j) == properties.value(property) ? ACCENT_COLOR : BUTTON_COLOR}});

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
            QMap <QString, QVariant> properties = device->properties().value(getEndpointId(item.endpoint));
            QString itemName = item.expose.split('_').value(0);

            if (properties.isEmpty())
                continue;

            switch (exposeList.indexOf(itemName))
            {
                case 0: // switch
                case 1: // light
                {
                    addCommand(list, item.page, item.id, {{"val", properties.value(QString(item.expose).replace(itemName, "status")).toString() == "on" ? 1 : 0}});
                    break;
                }

                case 2: // thermostat
                {
                    addCommand(list, item.page, item.id + 2, {{"text", properties.value("temperature").toString()}});
                    addCommand(list, item.page, item.id, {{"text", properties.value("targetTemperature").toString()}, {"text_color", properties.value("running").toBool() ? ACCENT_COLOR : TEXT_COLOR}});
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
                            addCommand(list, item.page, item.id, {{"text", QString(QChar(check ? 0xE81C : 0xE81B))}});

                        addCommand(list, item.page, item.id, {{"text_color", check ? itemName == "waterLeak" ? ALARM_COLOR : ACCENT_COLOR : TEXT_COLOR}});
                        break;
                    }

                    addCommand(list, item.page, item.id, {{"text", value.toString()}});
                    break;
                }
            }
        }
    }

    publishCommands(display, list);
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

void Controller::showWindow(DisplayObject *display, const DisplayItem &item)
{
    QString itemName = item.expose.split('_').value(0);
    const Device &device = findDevice(item.endpoint);

    if (!device.isNull() && device->available())
    {
        QJsonObject options = device->options(getEndpointId(item.endpoint));
        QJsonArray controls = itemName == "light" ? options.value(item.expose).toArray() : QJsonArray {"temperature", "targetTemperature", "systemMode", "operationMode"};

        display->setEndpoint(item.endpoint);
        display->setControl(0);
        display->properties().clear();

        for (auto it = controls.begin(); it != controls.end(); it++)
        {
            QString property = QString(item.expose).replace(itemName, it->toString());

            if (property.startsWith("colorMode") || (itemName == "thermostat" && property != "temperature" && !options.contains(property)))
                continue;

            display->properties().append(property);
        }

        mqttPublishString(QString("hasp/%1/command/p0b%2.clear").arg(display->id()).arg(WINDOW_ID), QString());

        publishCommands(display, display->window(item.name, options));
        sendData(display, item.endpoint);

        mqttPublishString(QString("hasp/%1/command/p0b%2.hidden").arg(display->id()).arg(WINDOW_ID), "0");
    }
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

        sendAvailability(display.data(), device->key());
        sendData(display.data(), device->key());
    }
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
    m_automations.clear();
    m_displays->store();

    mqttPublishService();
}

void Controller::mqttReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString subTopic = topic.name().replace(0, mqttTopic().length(), QString());
    QJsonObject json = QJsonDocument::fromJson(message).object();

    if (topic.name().startsWith("hasp/"))
    {
        QList <QString> list = topic.name().split('/');

        if (list.value(2) == "LWT")
        {
            const Display &display = m_displays->byName(list.value(1));

            if (!display.isNull() && display->active())
            {
                QString topic = QString("hasp/%1/command").arg(display->id());

                display->setAvailability(message == "online" ? Availability::Online : Availability::Offline);
                mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_displays->names() ? display->name() : display->id()), {{"status", display->availability() == Availability::Online ? "online" : "offline"}}, true);

                if (display->availability() != Availability::Online)
                    return;

                mqttPublishString(QString("%1/clearpage").arg(topic), "all");
                publishCommands(display.data(), display->layout());
                mqttPublishString(QString("%1/page").arg(topic), display->home() ? QString::number(display->home()) : QString());

                sendAvailability(display.data());
                sendData(display.data());
            }
        }
        else if (list.value(2) == "state")
        {
            int page, id;
            QRegExp expression("p(\\d+)b(\\d+)");
            QString event = json.value("event").toString();
            const Display &display = m_displays->byName(list.value(1));

            if (display.isNull() || display->availability() != Availability::Online)
                return;

            if (list.value(3) == "page" && message.toInt() > qMax(qMin(display->pages().count(), MAX_PAGES), 1))
            {
                mqttPublishString(QString("hasp/%1/command/page").arg(display->id()), "1");
                return;
            }

            if (list.value(3) == "idle" && message == "short")
            {
                if (display->home())
                    mqttPublishString(QString("hasp/%1/command/page").arg(display->id()), QString::number(display->home()));

                mqttPublishString(QString("hasp/%1/command/p0b%2.hidden").arg(display->id()).arg(WINDOW_ID), "1");
                display->setEndpoint(QString());
                display->properties().clear();
                return;
            }

            if (list.value(3) == QString("p0b%1").arg(WINDOW_ID) && json.value("hidden").toBool())
            {
                display->setEndpoint(QString());
                display->properties().clear();
                return;
            }

            if (!expression.exactMatch(list.value(3)))
                return;

            page = expression.cap(1).toInt();
            id = expression.cap(2).toInt();

            if (!page)
            {
                const Device &device = findDevice(display->endpoint());
                quint8 endpointId = getEndpointId(display->endpoint());

                if (device.isNull())
                    return;

                if (event == "down")
                {
                    display->setControl(id);
                    return;
                }

                if (event == "up")
                {
                    QList <QString> list = {"level", "colorTemperature", "color", "targetTemperature"};
                    QString property = display->properties().value(id < WINDOW_ID + 20 ? (id - WINDOW_ID - 3) / 2 : (id - WINDOW_ID - 20) / 10), itemName = property.split('_').value(0);
                    QJsonValue value;

                    display->setControl(0);

                    if (property.isEmpty() || (id < WINDOW_ID + 20 && (id - WINDOW_ID - 3) % 2))
                        return;

                    switch (list.indexOf(itemName))
                    {
                        case 0: // level
                        case 1: // colorTemperature
                        {
                            value = json.value("val");
                            break;
                        }

                        case 2: // color
                        {
                            value = QJsonArray {json.value("r"), json.value("g"), json.value("b")};
                            break;
                        }

                        case 3: // targetTemperature
                        {
                            QJsonObject options = device->options(endpointId, property);
                            double step = options.value("step").toDouble(1);
                            value = id < WINDOW_ID + 20 ? json.value("val").toInt() * step : qMin(qMax(device->properties().value(endpointId).value(property).toDouble() + ((id - WINDOW_ID - 20) % 10 ? step : step * -1), options.value("min").toDouble()), options.value("max").toDouble());
                            break;
                        }

                        default:
                        {
                            if (id >= WINDOW_ID + 20)
                            {
                                QJsonValue items = device->options(endpointId, property).value("enum");
                                value = QJsonValue::fromVariant((items.isArray() ? items.toArray().toVariantList() : items.toObject().toVariantMap().values()).value((id - WINDOW_ID - 20) % 10));
                            }

                            break;
                        }
                    }

                    if (value.isNull())
                        return;

                    mqttPublish(mqttTopic("td/").append(endpointId ? QString("%1/%2").arg(device->topic()).arg(endpointId) : device->topic()), {{property, value}});
                    return;
                }

                if (event == "changed" && display->properties().value((id - WINDOW_ID - 3) / 2) == "targetTemperature")
                {
                    QList <QString> list;
                    addCommand(list, 0, id + 1, {{"text", QString::number(json.value("val").toInt() * device->options(endpointId, "targetTemperature").value("step").toDouble(1))}, {"text_color", HEADER_COLOR}});
                    publishCommands(display.data(), list);
                }

                return;
            }

            for (int i = 0; i < display->items().count(); i++)
            {
                const DisplayItem &item = display->items().at(i);

                if (item.page != page || item.id != id)
                    continue;

                if (item.scene.isEmpty())
                {
                    QString itemName = item.expose.split('_').value(0);
                    quint8 endpointId = getEndpointId(item.endpoint);
                    const Device &device = findDevice(item.endpoint);

                    if (device.isNull() || !device->available())
                        break;

                    if (event == "down" && itemName == "light")
                    {
                        qint64 now = QDateTime::currentMSecsSinceEpoch();

                        QTimer::singleShot(LONG_PRESS_DELAY, this, [this, display, item, now] ()
                        {
                            if (display->time() != now)
                                return;

                            showWindow(display.data(), item);
                            display->setTime(0);
                        });

                        display->setTime(now);
                        break;
                    }

                    if (event == "up" && (itemName == "switch" || itemName == "light"))
                    {
                        if (itemName == "switch" || display->time())
                        {
                            mqttPublish(mqttTopic("td/").append(endpointId ? QString("%1/%2").arg(device->topic()).arg(endpointId) : device->topic()), {{QString(item.expose).replace(itemName, "status"), json.value("val").toInt() ? "on" : "off"}});
                            display->setTime(0);
                        }
                        else
                            sendData(display.data(), item.endpoint);

                        break;
                    }

                    if (event == "long" && itemName == "thermostat")
                        showWindow(display.data(), item);

                    break;
                }

                if (event == "up")
                    for (int j = 0; j < m_automations.count(); j++)
                        mqttPublish(mqttTopic("command/%1").arg(m_automations.at(j)), {{"action", "callScene"}, {"scene", item.scene}});

                break;
            }
        }

        return;
    }

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

        if (type == "automation")
        {
            m_automations.removeAll(service);

            if (json.value("status").toString() == "online")
                m_automations.append(service);

            return;
        }

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

        if (!m_updates.contains(string))
            m_updates.append(string);

        if (m_timer->isActive())
            return;

        m_timer->start(UPDATE_DELAY);
    }
}

void Controller::update(void)
{
    for (int i = 0; i < m_displays->count(); i++)
    {
        const Display &display = m_displays->at(i);

        if (display->availability() != Availability::Online)
            continue;

        for (int j = 0; j < m_updates.count(); j++)
            sendData(display.data(), m_updates.at(j));
    }

    m_updates.clear();
}
