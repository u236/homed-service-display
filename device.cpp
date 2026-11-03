#include "controller.h"
#include "device.h"
#include "logger.h"



// NOT REVIEWED
QList <QByteArray> DeviceObject::layout(void)
{
    int count = qMin(m_pages.count(), MAX_PAGES);
    QList <int> sizes = {150, 115, 80};
    QMap <QString, QString> units = {{"temperature", "°C"}, {"humidity", "%"}, {"co2", " ppm"}, {"power", " W"}}, icons = {{"switch", QChar(0xE425)}, {"light", QChar(0xE335)}, {"scene", QChar(0xE40A)}, {"waterLeak", QChar(0xE58C)}, {"occupancy", QChar(0xE004)}, {"contact", QChar(0xE81B)}};
    QList <QJsonObject> objects;
    QList <QByteArray> list;

    for (int i = 0; count > 1 && i < count; i++)
    {
        int width = (m_width - (count - 1) * NAVIGATION_GAP) / count, x = i * (width + NAVIGATION_GAP);
        objects.append({{"page", 0}, {"id", i + 1}, {"obj", "btn"}, {"action", QJsonObject {{"down", QString("page %1").arg(i + 1)}}}, {"x", x}, {"y", m_height - NAVIGATION_HEIGHT}, {"w", i < count - 1 ? width : m_width - x}, {"h", NAVIGATION_HEIGHT}, {"radius", 0}, {"border_width", 0}, {"bg_color", ACCENT_COLOR}, {"text", m_pages.at(i).toObject().value("name").toString()}, {"text_font", 24}, {"text_color", TEXT_COLOR}});
    }

    if (!count)
    {
        objects.append({{"page", 1}, {"id", 1}, {"obj", "obj"}, {"x", 0}, {"y", 0}, {"w", m_width}, {"h", m_height}, {"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}});
        objects.append({{"page", 1}, {"id", 2}, {"obj", "label"}, {"x", 0}, {"y", m_height / 2 - 84}, {"w", m_width}, {"h", 112}, {"align", "center"}, {"text", QString(QChar(0xE493))}, {"text_font", 96}, {"text_color", HEADER_COLOR}});
        objects.append({{"page", 1}, {"id", 3}, {"obj", "label"}, {"x", 0}, {"y", m_height / 2 + 29}, {"w", m_width}, {"h", 30}, {"align", "center"}, {"text", "No pages configured"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
    }

    for (int i = 0; i < count; i++)
    {
        QJsonObject page = m_pages.at(i).toObject();
        QJsonArray items = page.value("items").toArray();
        QList <QString> layout = page.value("layout").toString().split('x');
        bool portrait = m_height > m_width, clock = page.value("clock").toBool();
        int bottom = m_height - (count > 1 ? NAVIGATION_HEIGHT : 0) - (clock ? CLOCK_HEIGHT : 0), rows = clock && layout.value(0) == "3" ? 2 : layout.value(portrait ? 1 : 0).toInt(), columns = layout.value(portrait ? 0 : 1).toInt(), cellWidth = (m_width - CELL_PADDING) / columns - CELL_PADDING, cellHeight = (bottom - (clock ? 0 : CELL_PADDING) - HEADER_HEIGHT - CELL_PADDING) / rows - CELL_PADDING, level = qMax(rows, columns - 1), labelFont = 28 - level * 4, labelGap = labelFont / 2 - 4, labelHeight = labelFont * 5 / 4 + labelGap * 2, size = qMin(qMin(cellWidth, cellHeight - labelHeight), sizes.value(level - 1)), gap = (m_width - columns * size) / (columns + 1), pitch = level > 2 ? qMin(cellHeight + CELL_PADDING, size + labelHeight + ROW_GAP) : cellHeight + CELL_PADDING, top = HEADER_HEIGHT + CELL_PADDING + rows * (cellHeight + CELL_PADDING - pitch) / 2;

        objects.append({{"page", i + 1}, {"id", 1}, {"obj", "obj"}, {"x", 0}, {"y", 0}, {"w", m_width}, {"h", m_height}, {"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}});
        objects.append({{"page", i + 1}, {"id", 2}, {"obj", "label"}, {"x", 0}, {"y", HEADER_HEIGHT / 2 - 14}, {"w", m_width}, {"h", 28}, {"align", "center"}, {"text", page.value("name").toString()}, {"text_font", 24}, {"text_color", HEADER_COLOR}});

        if (clock)
        {
            objects.append({{"page", i + 1}, {"id", 3}, {"obj", "label"}, {"x", 0}, {"y", bottom - 1}, {"w", m_width}, {"h", 80}, {"align", "center"}, {"text", "..."}, {"template", "%H:%M:%S"}, {"text_font", 68}, {"text_color", HEADER_COLOR}});
            objects.append({{"page", i + 1}, {"id", 4}, {"obj", "label"}, {"x", 0}, {"y", bottom + 71}, {"w", m_width}, {"h", 28}, {"align", "center"}, {"text", "..."}, {"template", "%d.%m.%Y"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
        }

        for (int j = 0; j < items.count() && j < rows * columns; j++)
        {
            QJsonObject item = items.at(j).toObject();
            QString expose = item.value("scene").toString().isEmpty() ? item.value("expose").toString().split('_').value(0) : "scene", name = item.value("name").toString(), text = units.contains(expose) ? QString("--%1").arg(units.value(expose)) : icons.value(expose, expose);
            int id = (j + 1) * 10, x = gap + j % columns * (size + gap), y = top + j / columns * pitch + (pitch - CELL_PADDING - labelHeight - size) / 2, font = units.contains(expose) ? size / 4 : icons.contains(expose) ? size / 2 : 24, height = font * 117 / 100;

            objects.append({{"page", i + 1}, {"id", id}, {"obj", "btn"}, {"x", x}, {"y", y}, {"w", size}, {"h", size}, {"radius", size / 2}, {"border_width", 0}, {"toggle", expose == "switch" || expose == "light"}});
            objects.append({{"page", i + 1}, {"id", id + 1}, {"obj", "label"}, {"x", x}, {"y", y + (size - height) / 2}, {"w", size}, {"h", height}, {"align", "center"}, {"text", text}, {"text_font", font}, {"text_color", TEXT_COLOR}});

            if (name.isEmpty())
                continue;

            objects.append({{"page", i + 1}, {"id", id + 2}, {"obj", "label"}, {"x", x - gap / 2}, {"y", y + size + labelGap}, {"w", size + gap}, {"h", labelFont * 5 / 4}, {"align", "center"}, {"text", name}, {"text_font", labelFont}, {"text_color", HEADER_COLOR}});
        }
    }

    for (int i = 0; i < objects.count(); i++)
        list.append(QJsonDocument(objects.at(i)).toJson(QJsonDocument::Compact).append('\n'));

    return list;
}
//



DeviceList::DeviceList(QSettings *config, QObject *parent) : QObject(parent), m_timer(new QTimer(this)), m_sync(false)
{
    m_file.setFileName(config->value("device/database", "/opt/homed-display/database.json").toString());
    m_names = config->value("mqtt/names", false).toBool();

    connect(m_timer, &QTimer::timeout, this, &DeviceList::writeDatabase);
    m_timer->setSingleShot(true);
}

DeviceList::~DeviceList(void)
{
    m_sync = true;
    writeDatabase();
}

void DeviceList::init(void)
{
    QJsonObject json;

    if (!m_file.open(QFile::ReadOnly))
        return;

    json = QJsonDocument::fromJson(m_file.readAll()).object();
    unserialize(json.value("devices").toArray());
    m_file.close();
}

void DeviceList::store(bool sync)
{
    if (sync)
        m_sync = true;

    m_timer->start(STORE_DATABASE_DELAY);
}

Device DeviceList::byName(const QString &name, int *index)
{
    for (int i = 0; i < count(); i++)
    {
        if (at(i)->id() != name && at(i)->name() != name)
            continue;

        if (index)
            *index = i;

        return at(i);
    }

    return Device();
}

Device DeviceList::parse(const QJsonObject &json)
{
    QList <QString> list = {"1x1", "1x2", "1x3", "1x4", "2x2", "2x3", "2x4", "3x3", "3x4"};
    QJsonArray pages = json.value("pages").toArray();
    QString id = mqttSafe(json.value("id").toString());
    qint16 width = static_cast <qint16> (json.value("width").toInt()), height = static_cast <qint16> (json.value("height").toInt());
    Device device;

    for (auto it = pages.begin(); it != pages.end(); it++)
    {
        QJsonObject page = it->toObject();

        if (list.contains(page.value("layout").toString()) && !page.value("name").toString().trimmed().isEmpty())
            continue;

        return device;
    }

    if (id.isEmpty() || width <= 0 || height <= 0)
        return device;

    device = Device(new DeviceObject(id, width, height, static_cast <qint16> (json.value("home").toInt(1)), static_cast <qint16> (json.value("timeout").toInt()), mqttSafe(json.value("name").toString()), pages)); // TODO: check home page range

    if (json.contains("active"))
        device->setActive(json.value("active").toBool());

    return device;
}

void DeviceList::unserialize(const QJsonArray &devices)
{
    for (auto it = devices.begin(); it != devices.end(); it++)
    {
        QJsonObject json = it->toObject();
        Device device;

        if (!byName(json.value("id").toString()).isNull() || !byName(json.value("name").toString()).isNull())
            continue;

        device = parse(json);

        if (device.isNull())
            continue;

        append(device);
    }

    if (!count())
        return;

    logInfo << count() << "devices loaded";
}

QJsonArray DeviceList::serialize(void)
{
    QJsonArray array;

    for (int i = 0; i < count(); i++)
    {
        const Device &device = at(i);
        QJsonObject json = {{"id", device->id()}, {"width", device->width()}, {"height", device->height()}, {"home", device->home()}, {"active", device->active()}};

        if (device->name() != device->id())
            json.insert("name", device->name());

        if (device->timeout())
            json.insert("timeout", device->timeout());

        if (!device->pages().isEmpty())
            json.insert("pages", device->pages());

        array.append(json);
    }

    return array;
}

void DeviceList::writeDatabase(void)
{
    HOMEd *homed = reinterpret_cast <HOMEd*> (parent());
    QJsonObject json = {{"devices", serialize()}, {"names", m_names}, {"timestamp", QDateTime::currentSecsSinceEpoch()}, {"version", SERVICE_VERSION}};

    homed->mqttPublishStatus(json);

    if (!m_sync)
        return;

    json.remove("names");
    m_sync = false;

    if (homed->writeFile(m_file, QJsonDocument(json).toJson(QJsonDocument::Compact)))
        return;

    logWarning << "Database not stored";
}
