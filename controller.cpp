#include "controller.h"
#include "logger.h"

Controller::Controller(const QString &configFile) : HOMEd(SERVICE_VERSION, configFile), m_displays(new DisplayList(getConfig(), this)), m_commands(QMetaEnum::fromType <Command> ()), m_events(QMetaEnum::fromType <Event> ())
{
    m_displays->init();
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

void Controller::publishEvent(const QString &name, Event event)
{
    mqttPublish(mqttTopic("event/%1").arg(serviceTopic()), {{"device", name}, {"event", m_events.valueToKey(static_cast <int> (event))}});
}

void Controller::displayEvent(DisplayObject *display, Event event)
{
    QString topic = QString("hasp/%1/LWT").arg(display->id());

    switch (event)
    {
        case Event::aboutToRename:
        case Event::removed:
            mqttPublish(mqttTopic("device/%1/%2").arg(serviceTopic(), m_displays->names() ? display->name() : display->id()), QJsonObject(), true);
            mqttUnsubscribe(topic);
            break;

        case Event::added:
        case Event::updated:
            QTimer::singleShot(SUBSCRIPTION_DELAY, this, [this, topic] () { mqttSubscribe(topic); });
            mqttUnsubscribe(topic);
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

    for (int i = 0; i < m_displays->count(); i++)
        mqttSubscribe(QString("hasp/%1/LWT").arg(m_displays->at(i)->id()));

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
    }
}
