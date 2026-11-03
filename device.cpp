#include "controller.h"
#include "device.h"
#include "logger.h"



// NOT REVIEWED
QList <QByteArray> DeviceObject::layout(void)
{
    QList <QByteArray> list;
    QList <int> sizes = {150, 115, 80};
    QMap <QString, QString> units = {{"temperature", "°C"}, {"humidity", "%"}, {"co2", " ppm"}, {"power", " W"}}, icons = {{"switch", QChar(0xE425)}, {"light", QChar(0xE335)}, {"scene", QChar(0xE40A)}, {"waterLeak", QChar(0xE58C)}, {"occupancy", QChar(0xE004)}, {"contact", QChar(0xE81B)}};
    int count = qMin(m_pages.count(), MAX_PAGES);

    if (!count)
    {
        add(list, "obj", 1, 1, 0, 0, m_width, m_height, {{"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}});
        add(list, "label", 1, 2, 0, m_height / 2 - 84, m_width, 112, {{"align", "center"}, {"text", QString(QChar(0xE493))}, {"text_font", 96}, {"text_color", HEADER_COLOR}});
        add(list, "label", 1, 3, 0, m_height / 2 + 29, m_width, 30, {{"align", "center"}, {"text", "No pages configured"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
    }

    for (int i = 0; i < count; i++)
    {
        QJsonObject page = m_pages.at(i).toObject();
        QJsonArray items = page.value("items").toArray();
        QList <QString> layout = page.value("layout").toString().split('x');
        bool portrait = m_height > m_width, clock = page.value("clock").toBool();
        int bottom = m_height - (count > 1 ? NAVIGATION_HEIGHT : 0) - (clock ? CLOCK_HEIGHT : 0), rows = clock && layout.value(0) == "3" ? 2 : layout.value(portrait ? 1 : 0).toInt(), columns = layout.value(portrait ? 0 : 1).toInt(), cellWidth = (m_width - CELL_PADDING) / columns - CELL_PADDING, cellHeight = (bottom - (clock ? 0 : CELL_PADDING) - HEADER_HEIGHT - CELL_PADDING) / rows - CELL_PADDING, level = qMax(rows, columns - 1), labelFont = 28 - level * 4, labelGap = labelFont / 2 - 4, labelHeight = labelFont * 5 / 4 + labelGap * 2, size = qMin(qMin(cellWidth, cellHeight - labelHeight), sizes.value(level - 1)), gap = (m_width - columns * size) / (columns + 1), pitch = level > 2 ? qMin(cellHeight + CELL_PADDING, size + labelHeight + ROW_GAP) : cellHeight + CELL_PADDING, top = HEADER_HEIGHT + CELL_PADDING + rows * (cellHeight + CELL_PADDING - pitch) / 2;

        add(list, "obj", i + 1, 1, 0, 0, m_width, m_height, {{"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}});
        add(list, "label", i + 1, 2, 0, HEADER_HEIGHT / 2 - 14, m_width, 28, {{"align", "center"}, {"text", page.value("name").toString()}, {"text_font", 24}, {"text_color", HEADER_COLOR}});

        if (clock)
        {
            add(list, "label", i + 1, 3, 0, bottom - 1, m_width, 80, {{"align", "center"}, {"text", "..."}, {"template", "%H:%M:%S"}, {"text_font", 68}, {"text_color", HEADER_COLOR}});
            add(list, "label", i + 1, 4, 0, bottom + 71, m_width, 28, {{"align", "center"}, {"text", "..."}, {"template", "%d.%m.%Y"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
        }

        for (int j = 0; j < items.count() && j < rows * columns; j++)
        {
            QJsonObject item = items.at(j).toObject();
            QString expose = item.value("scene").toString().isEmpty() ? item.value("expose").toString().split('_').value(0) : "scene", name = item.value("name").toString(), text = units.contains(expose) ? QString("--%1").arg(units.value(expose)) : icons.value(expose, expose);
            int id = (j + 1) * 10, x = gap + j % columns * (size + gap), y = top + j / columns * pitch + (pitch - CELL_PADDING - labelHeight - size) / 2, font = units.contains(expose) ? size / 4 : icons.contains(expose) ? size / 2 : 24, height = font * 117 / 100;

            add(list, "btn", i + 1, id, x, y, size, size, {{"radius", size / 2}, {"border_width", 0}, {"toggle", expose == "switch" || expose == "light"}});
            add(list, "label", i + 1, id + 1, x, y + (size - height) / 2, size, height, {{"align", "center"}, {"text", text}, {"text_font", font}, {"text_color", TEXT_COLOR}});

            if (name.isEmpty())
                continue;

            add(list, "label", i + 1, id + 2, x - gap / 2, y + size + labelGap, size + gap, labelFont * 5 / 4, {{"align", "center"}, {"text", name}, {"text_font", labelFont}, {"text_color", HEADER_COLOR}});
        }
    }

    for (int i = 0; count > 1 && i < count; i++)
    {
        int width = (m_width - (count - 1) * NAVIGATION_GAP) / count, x = i * (width + NAVIGATION_GAP);
        add(list, "btn", 0, i + 1, x, m_height - NAVIGATION_HEIGHT, i < count - 1 ? width : m_width - x, NAVIGATION_HEIGHT, {{"action", QJsonObject {{"down", QString("page %1").arg(i + 1)}}}, {"radius", 0}, {"border_width", 0}, {"bg_color", ACCENT_COLOR}, {"text", m_pages.at(i).toObject().value("name").toString()}, {"text_font", 24}, {"text_color", TEXT_COLOR}});
    }

    return list;
}
//



void DeviceObject::add(QList <QByteArray> &list, const QString &type, quint16 page, quint16 id, quint16 x, quint16 y, quint16 width, quint16 height, const QJsonObject &data)
{
    QJsonObject json = {{"obj", type}, {"page", page}, {"id", id}, {"x", x}, {"y", y}, {"w", width}, {"h", height}};

    for (auto it = data.begin(); it != data.end(); it++)
        json.insert(it.key(), it.value());

    list.append(QJsonDocument(json).toJson(QJsonDocument::Compact).append('\n'));
}

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
    quint16 width = static_cast <quint16> (json.value("width").toInt()), height = static_cast <quint16> (json.value("height").toInt());
    Device device;

    for (auto it = pages.begin(); it != pages.end(); it++)
    {
        QJsonObject page = it->toObject();

        if (list.contains(page.value("layout").toString()) && !page.value("name").toString().trimmed().isEmpty())
            continue;

        return device;
    }

    if (id.isEmpty() || !width || !height)
        return device;

    device = Device(new DeviceObject(id, width, height, static_cast <quint16> (json.value("home").toInt(1)), static_cast <quint16> (json.value("timeout").toInt()), mqttSafe(json.value("name").toString()), pages)); // TODO: check home page range

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
