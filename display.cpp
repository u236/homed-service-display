#include "color.h"
#include "controller.h"
#include "display.h"
#include "expose.h"
#include "logger.h"



// NOT REVIEWED
QList <QString> DisplayObject::layout(void)
{
    QList <QString> list;
    QList <int> sizes = {150, 115, 80};
    QMap <QString, QString> units = {{"temperature", "°C"}, {"humidity", "%"}, {"co2", "ppm"}, {"power", "W"}, {"thermostat", "°C"}}, icons = {{"switch", QChar(0xE425)}, {"light", QChar(0xE335)}, {"scene", QChar(0xE40A)}, {"waterLeak", QChar(0xE58C)}, {"occupancy", QChar(0xE004)}, {"contact", QChar(0xE81B)}};
    int count = qMin(m_pages.count(), MAX_PAGES);

    m_endpoint.clear();
    m_properties.clear();
    m_items.clear();

    if (!count)
    {
        add(list, "obj", 1, 1, 0, 0, m_width, m_height, {{"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}});
        add(list, "label", 1, 2, 0, m_height / 2 - 56, m_width, 112, {{"align", "center"}, {"text", QString(QChar(0xE493))}, {"text_font", 96}, {"text_color", HEADER_COLOR}});
        add(list, "label", 1, 3, 0, m_height / 2 + 105, m_width, 30, {{"align", "center"}, {"text", "No pages configured"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
    }

    for (int i = 0; i < count; i++)
    {
        QJsonObject page = m_pages.at(i).toObject();
        QJsonArray items = page.value("items").toArray();
        QList <QString> layout = page.value("layout").toString().split('x');
        bool portrait = m_height > m_width, clock = page.value("clock").toBool();
        int header = count > 1 ? HEADER_HEIGHT : 0, bottom = m_height - (count > 1 ? NAVIGATION_HEIGHT : 0) - (clock ? CLOCK_HEIGHT : 0), rows = clock && layout.value(0) == "3" ? 2 : layout.value(portrait ? 1 : 0).toInt(), columns = layout.value(portrait ? 0 : 1).toInt(), cellWidth = (m_width - CELL_PADDING) / columns - CELL_PADDING, cellHeight = (bottom - (clock ? 0 : CELL_PADDING) - header - CELL_PADDING) / rows - CELL_PADDING, level = qMax(rows, columns - 1), labelFont = 28 - level * 4, labelGap = labelFont / 2 - 4, labelHeight = labelFont * 5 / 4 + labelGap * 2, size = qMin(qMin(cellWidth, cellHeight - labelHeight), sizes.value(level - 1)), gap = (m_width - columns * size) / (columns + 1), pitch = qMin(cellHeight + CELL_PADDING, size + labelHeight + ROW_GAP), top = header + CELL_PADDING + rows * (cellHeight + CELL_PADDING - pitch) / 2; // TODO: check rows for portrait orientation, 4 without clock and 3 with clock
        QString name = page.value("name").toString().trimmed();

        add(list, "obj", i + 1, 1, 0, 0, m_width, m_height, {{"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}});

        if (count > 1)
            add(list, "label", i + 1, 2, 0, HEADER_HEIGHT / 2 - 14, m_width, 28, {{"align", "center"}, {"text", name.isEmpty() ? QString("Page %1").arg(i + 1) : name}, {"text_font", 24}, {"text_color", HEADER_COLOR}});

        if (clock)
        {
            add(list, "label", i + 1, 3, 0, bottom - 1, m_width, 80, {{"align", "center"}, {"text", "..."}, {"template", "%H:%M:%S"}, {"text_font", 68}, {"text_color", HEADER_COLOR}});
            add(list, "label", i + 1, 4, 0, bottom + 71, m_width, 28, {{"align", "center"}, {"text", "..."}, {"template", "%d.%m.%Y"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
        }

        if (items.isEmpty())
        {
            int y = (header + bottom - (clock ? 0 : CELL_PADDING) - 196) / 2;
            add(list, "label", i + 1, 5, 0, count > 1 || clock ? y + 19 : m_height / 2 - 56, m_width, 112, {{"align", "center"}, {"text", QString(QChar(0xE493))}, {"text_font", 96}, {"text_color", HEADER_COLOR}});
            add(list, "label", i + 1, 6, 0, count > 1 || clock ? y + 158 : m_height / 2 + 105, m_width, 30, {{"align", "center"}, {"text", "No items configured"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
        }

        for (int j = 0; j < items.count() && j < rows * columns; j++)
        {
            QJsonObject item = items.at(j).toObject();
            QString endpoint = item.value("endpoint").toString(), scene = item.value("scene").toString(), expose = scene.isEmpty() ? item.value("expose").toString().split('_').value(0) : "scene", unit = units.value(expose), name = item.value("name").toString(), text = unit.isEmpty() ? icons.value(expose, expose) : "--";
            bool large = rows * columns == 1 && !unit.isEmpty() && expose != "thermostat"; // TODO: separate layout for thermostat on 1x1 page
            int id = (j + 1) * 10, height = large ? 120 : size, x = gap + j % columns * (size + gap), y = top + j / columns * pitch + (pitch - CELL_PADDING - labelHeight - height) / 2, font = units.contains(expose) ? size / 3 : icons.contains(expose) ? size / 2 : 24;

            if (large)
                add(list, "label", i + 1, id, 0, y - 20, m_width, 160, {{"align", "center"}, {"text", text}, {"text_font", 130}, {"text_color", TEXT_COLOR}});
            else
                add(list, "btn", i + 1, id, x, y, size, size, {{"toggle", expose == "switch" || expose == "light"}, {"radius", size / 2}, {"border_width", 0}, {"bg_color", BUTTON_COLOR}, {"bg_color01", ACCENT_COLOR}, {"bg_color03", ACCENT_COLOR}, {"bg_color04", BUTTON_COLOR}, {"bg_color05", BUTTON_COLOR}, {"align", "center"}, {"text", text}, {"text_font", font}, {"text_color", TEXT_COLOR}});

            if (expose == "thermostat")
                add(list, "label", i + 1, id + 2, x, y + size * 2 / 3 - 2, size, size / 4, {{"click", false}, {"align", "center"}, {"text", "--"}, {"text_font", size / 5}, {"text_color", HEADER_COLOR}});

            if (name.isEmpty())
                name = QString("Item %1").arg(j + 1);

            if (!endpoint.isEmpty() || !scene.isEmpty())
                m_items.append({static_cast <quint16> (i + 1), static_cast <quint16> (id), name, endpoint, item.value("expose").toString(), scene});

            if (!unit.isEmpty())
                name = QString("%1, %2").arg(name, unit);

            add(list, "label", i + 1, id + 1, x - gap / 2, y + height + labelGap, size + gap, labelFont * 5 / 4, {{"align", "center"}, {"text", name}, {"text_font", labelFont}, {"text_color", HEADER_COLOR}});
        }
    }

    for (int i = 0; count > 1 && i < count; i++)
    {
        int width = (m_width - (count - 1) * NAVIGATION_GAP) / count, x = i * (width + NAVIGATION_GAP);
        QString name = m_pages.at(i).toObject().value("name").toString().trimmed();
        add(list, "btn", 0, i + 1, x, m_height - NAVIGATION_HEIGHT, i < count - 1 ? width : m_width - x, NAVIGATION_HEIGHT, {{"action", QJsonObject {{"down", QString("page %1").arg(i + 1)}}}, {"radius", 0}, {"border_width", 0}, {"bg_color", ACCENT_COLOR}, {"text", name.isEmpty() ? QString("Page %1").arg(i + 1) : name}, {"text_font", 24}, {"text_color", TEXT_COLOR}});
    }

    add(list, "obj", 0, WINDOW_ID, 0, 0, m_width, m_height, {{"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}, {"hidden", true}});
    add(list, "obj", 0, WATCHDOG_ID, 0, 0, m_width, m_height, {{"radius", 0}, {"border_width", 0}, {"bg_color", BACKGROUND_COLOR}, {"hidden", true}, {"timer_action", QJsonObject {{"start", QString("p0b%1.hidden=1").arg(WATCHDOG_ID)}, {"timeout", QString("p0b%1.hidden=0").arg(WATCHDOG_ID)}}}, {"timer_interval", WATCHDOG_TIMEOUT}});
    add(list, "label", 0, WATCHDOG_ID + 1, 0, m_height / 2 - 56, m_width, 112, {{"parentid", WATCHDOG_ID}, {"click", false}, {"align", "center"}, {"text", QString(QChar(0xE026))}, {"text_font", 96}, {"text_color", ACCENT_COLOR}});
    add(list, "label", 0, WATCHDOG_ID + 2, 0, m_height / 2 + 105, m_width, 30, {{"parentid", WATCHDOG_ID}, {"click", false}, {"align", "center"}, {"text", "Display offline"}, {"text_font", 24}, {"text_color", HEADER_COLOR}});
    return list;
}
//



// NOT REVIEWED
QList <QString> DisplayObject::window(const QString &name, const QJsonObject &options)
{
    QList <QString> list;
    int sliders = 0, rows = 0, color = -1, top, row, y;

    for (int i = 0; i < m_properties.count(); i++)
    {
        QString itemName = m_properties.at(i).split('_').value(0);

        if (itemName == "color")
            color = i;
        else if (itemName == "level" || itemName == "colorTemperature")
            sliders++;
        else if (itemName == "systemMode" || itemName == "operationMode")
            rows++;
    }

    top = rows > 1 ? 80 : rows ? 98 : 148;
    row = rows > 1 ? 318 : 336;
    y = qMax(66, (m_height - sliders * 50 - (sliders > 1 ? 15 : 0) - (color < 0 ? 0 : 210 + (sliders ? 45 : 0)) + 1) / 2);

    add(list, "btn", 0, WINDOW_ID + 1, m_width - 60, 2, 56, 56, {{"parentid", WINDOW_ID}, {"action", QJsonObject {{"up", QJsonArray {QString("p0b%1.hidden 1").arg(WINDOW_ID), QString("p0b%1.hidden").arg(WINDOW_ID)}}}}, {"radius", 28}, {"border_width", 0}, {"bg_opa", 0}, {"text", QString(QChar(0xE156))}, {"text_font", 32}, {"text_color", HEADER_COLOR}});
    add(list, "label", 0, WINDOW_ID + 2, 20, 16, m_width - 80, 30, {{"parentid", WINDOW_ID}, {"align", "left"}, {"text", name}, {"text_font", 24}, {"text_color", HEADER_COLOR}});

    for (int i = 0; i < m_properties.count(); i++)
    {
        QString property = m_properties.at(i), itemName = property.split('_').value(0);
        QJsonObject data = {{"parentid", WINDOW_ID}, {"bg_color20", TEXT_COLOR}, {"bg_color22", TEXT_COLOR}, {"border_color20", BACKGROUND_COLOR}, {"border_width20", 3}};

        if (itemName == "level")
        {
            data.insert("min", 0);
            data.insert("max", 255);
            data.insert("bg_color00", BUTTON_COLOR);
            data.insert("bg_color10", ACCENT_COLOR);
        }
        else if (itemName == "colorTemperature")
        {
            QJsonObject option = options.value(options.contains(property) ? property : itemName).toObject();
            int min = option.value("min").toInt(153), max = option.value("max").toInt(500);
            Color start = Color::fromCT(min), end = Color::fromCT(max);

            data.insert("min", min);
            data.insert("max", max);
            data.insert("bg_opa10", 0);
            data.insert("bg_color00", QString::asprintf("#%02x%02x%02x", static_cast <int> (start.r() * 255), static_cast <int> (start.g() * 255), static_cast <int> (start.b() * 255)));
            data.insert("bg_grad_color00", QString::asprintf("#%02x%02x%02x", static_cast <int> (end.r() * 255), static_cast <int> (end.g() * 255), static_cast <int> (end.b() * 255)));
            data.insert("bg_grad_dir00", 2);
        }
        else if (itemName == "temperature")
        {
            add(list, "label", 0, WINDOW_ID + 3 + i * 2, (m_width - 210) / 2, top + 130, 210, 24, {{"parentid", WINDOW_ID}, {"click", false}, {"align", "center"}, {"text", "--"}, {"text_font", 20}, {"text_color", HEADER_COLOR}});
            continue;
        }
        else if (itemName == "targetTemperature")
        {
            QJsonObject option = options.value(property).toObject();
            double step = option.value("step").toDouble(1);

            if (!option.contains("min") || !option.contains("max"))
                continue;

            data.insert("min", qRound(option.value("min").toDouble() / step));
            data.insert("max", qRound(option.value("max").toDouble() / step));
            data.insert("rotation", 0);
            data.insert("start_angle", 135);
            data.insert("end_angle", 45);
            data.insert("adjustable", true);
            data.insert("border_side", 0);
            data.insert("bg_opa", 0);
            data.insert("line_width", 22);
            data.insert("line_width10", 22);
            data.insert("line_color", BUTTON_COLOR);
            data.insert("line_color10", ACCENT_COLOR);
            data.insert("pad_top20", 3);
            data.insert("pad_bottom20", 3);
            data.insert("pad_left20", 3);
            data.insert("pad_right20", 3);

            add(list, "arc", 0, WINDOW_ID + 3 + i * 2, (m_width - 210) / 2, top, 210, 210, data);
            add(list, "label", 0, WINDOW_ID + 4 + i * 2, (m_width - 210) / 2, top + 70, 210, 56, {{"parentid", WINDOW_ID}, {"click", false}, {"align", "center"}, {"text", "--"}, {"text_font", 48}, {"text_color", TEXT_COLOR}}); // TODO: draw degree sign somewhere in thermostat window
            add(list, "btn", 0, WINDOW_ID + 20 + i * 10, (m_width - 210) / 2 - 92, top + 75, 60, 60, {{"parentid", WINDOW_ID}, {"radius", 30}, {"border_width", 0}, {"bg_color", BUTTON_COLOR}, {"text", QString(QChar(0xE374))}, {"text_font", 32}, {"text_color", TEXT_COLOR}});
            add(list, "btn", 0, WINDOW_ID + 21 + i * 10, (m_width + 210) / 2 + 32, top + 75, 60, 60, {{"parentid", WINDOW_ID}, {"radius", 30}, {"border_width", 0}, {"bg_color", BUTTON_COLOR}, {"text", QString(QChar(0xE415))}, {"text_font", 32}, {"text_color", TEXT_COLOR}});
            continue;
        }
        else if (itemName == "systemMode" || itemName == "operationMode")
        {
            QJsonValue value = options.value(property).toObject().value("enum");
            QList <QVariant> items = value.isArray() ? value.toArray().toVariantList() : value.toObject().toVariantMap().values();
            int width = items.isEmpty() ? 0 : (m_width - 40 - (items.count() - 1) * 8) / items.count();

            add(list, "label", 0, WINDOW_ID + 4 + i * 2, 20, row - 30, m_width - 40, 24, {{"parentid", WINDOW_ID}, {"text", itemName == "systemMode" ? "System mode" : "Operation mode"}, {"text_font", 20}, {"text_color", HEADER_COLOR}});

            for (int j = 0; j < items.count(); j++)
                add(list, "btn", 0, WINDOW_ID + 20 + i * 10 + j, 20 + j * (width + 8), row, width, 36, {{"parentid", WINDOW_ID}, {"radius", 18}, {"border_width", 0}, {"bg_color", BUTTON_COLOR}, {"text", items.at(j).toString()}, {"text_font", 20}, {"text_color", TEXT_COLOR}});

            row += 81;
            continue;
        }
        else
            continue;

        data.insert("anim_time", 0);
        data.insert("ext_click_h", 20);

        add(list, "slider", 0, WINDOW_ID + 3 + i * 2, 20, y + 30, m_width - 40, 20, data);
        add(list, "label", 0, WINDOW_ID + 4 + i * 2, 20, y, m_width - 40, 24, {{"parentid", WINDOW_ID}, {"text", itemName == "level" ? "Level" : "Color temperature"}, {"text_font", 20}, {"text_color", HEADER_COLOR}});

        y += 65;
    }

    if (color >= 0)
        add(list, "cpicker", 0, WINDOW_ID + 3 + color * 2, (m_width - 210) / 2, sliders ? y + 30 : y, 210, 210, {{"parentid", WINDOW_ID}, {"scale_width", 22}, {"pad_inner", 22}, {"pad_top20", 3}, {"pad_bottom20", 3}, {"pad_left20", 3}, {"pad_right20", 3}, {"border_color20", BACKGROUND_COLOR}, {"border_width20", 3}, {"bg_color10", BACKGROUND_COLOR}, {"mode_fixed", true}});

    return list;
}
//



void DisplayObject::add(QList <QString> &list, const QString &type, quint16 page, quint16 id, quint16 x, quint16 y, quint16 width, quint16 height, const QJsonObject &data)
{
    QJsonObject json = {{"obj", type}, {"page", page}, {"id", id}, {"x", x}, {"y", y}, {"w", width}, {"h", height}};

    for (auto it = data.begin(); it != data.end(); it++)
        json.insert(it.key(), it.value());

    list.append(QJsonDocument(json).toJson(QJsonDocument::Compact).append('\n'));
}

DisplayList::DisplayList(QSettings *config, QObject *parent) : QObject(parent), m_timer(new QTimer(this)), m_sync(false)
{
    m_file.setFileName(config->value("device/database", "/opt/homed-display/database.json").toString());
    m_names = config->value("mqtt/names", false).toBool();

    connect(m_timer, &QTimer::timeout, this, &DisplayList::writeDatabase);
    m_timer->setSingleShot(true);
}

DisplayList::~DisplayList(void)
{
    m_sync = true;
    writeDatabase();
}

void DisplayList::init(void)
{
    QJsonObject json;

    if (!m_file.open(QFile::ReadOnly))
        return;

    json = QJsonDocument::fromJson(m_file.readAll()).object();
    unserialize(json.value("devices").toArray());
    m_file.close();
}

void DisplayList::store(bool sync)
{
    if (sync)
        m_sync = true;

    m_timer->start(STORE_DATABASE_DELAY);
}

Display DisplayList::byName(const QString &name, int *index)
{
    for (int i = 0; i < count(); i++)
    {
        if (at(i)->id() != name && at(i)->name() != name)
            continue;

        if (index)
            *index = i;

        return at(i);
    }

    return Display();
}

Display DisplayList::parse(const QJsonObject &json)
{
    QList <QString> list = {"1x1", "1x2", "1x3", "1x4", "2x2", "2x3", "2x4", "3x3", "3x4"};
    QJsonArray pages = json.value("pages").toArray();
    QString id = mqttSafe(json.value("id").toString());
    quint16 width = static_cast <quint16> (json.value("width").toInt()), height = static_cast <quint16> (json.value("height").toInt());
    Display display;

    for (auto it = pages.begin(); it != pages.end(); it++)
    {
        QJsonObject page = it->toObject();

        if (list.contains(page.value("layout").toString()))
            continue;

        return display;
    }

    if (!id.isEmpty() && width && height)
    {
        Expose light(new LightObject), idle(new SensorObject("idle")), antiburn(new ToggleObject("antiburn"));
        Endpoint endpoint;

        display = Display(new DisplayObject(id, width, height, static_cast <quint16> (json.value("home").toInt()), mqttSafe(json.value("name").toString()), pages));
        endpoint = Endpoint(new EndpointObject(DEFAULT_ENDPOINT, reinterpret_cast <const Device &> (display)));

        light->setParent(endpoint.data());
        endpoint->exposes().append(light);

        idle->setParent(endpoint.data());
        endpoint->exposes().append(idle);

        antiburn->setParent(endpoint.data());
        endpoint->exposes().append(antiburn);

        if (json.contains("active"))
            display->setActive(json.value("active").toBool());

        if (json.contains("discovery"))
            display->setDiscovery(json.value("discovery").toBool());

        if (json.contains("cloud"))
            display->setCloud(json.value("cloud").toBool());

        if (!display->active())
            display->setAvailability(Availability::Offline);

        display->setNote(json.value("note").toString());
        display->endpoints().insert(endpoint->id(), endpoint);

        display->options().insert("light",    QList <QVariant> {"level"});
        display->options().insert("idle",     QMap <QString, QVariant> {{"type", "sensor"}, {"icon", "mdi:timer-sand"}});
        display->options().insert("antiburn", QMap <QString, QVariant> {{"type", "toggle"}, {"icon", "mdi:shimmer"}});
    }

    return display;
}

void DisplayList::unserialize(const QJsonArray &displays)
{
    for (auto it = displays.begin(); it != displays.end(); it++)
    {
        QJsonObject json = it->toObject();
        Display display;

        if (!byName(json.value("id").toString()).isNull() || !byName(json.value("name").toString()).isNull())
            continue;

        display = parse(json);

        if (display.isNull())
            continue;

        append(display);
    }

    if (!count())
        return;

    logInfo << count() << "devices loaded";
}

QJsonArray DisplayList::serialize(void)
{
    QJsonArray array;

    for (int i = 0; i < count(); i++)
    {
        const Display &display = at(i);
        QJsonObject json = {{"id", display->id()}, {"width", display->width()}, {"height", display->height()}, {"active", display->active()}, {"discovery", display->discovery()}, {"cloud", display->cloud()}};

        if (display->name() != display->id())
            json.insert("name", display->name());

        if (display->home())
            json.insert("home", display->home());

        if (!display->pages().isEmpty())
            json.insert("pages", display->pages());

        if (!display->note().isEmpty())
            json.insert("note", display->note());

        array.append(json);
    }

    return array;
}

void DisplayList::writeDatabase(void)
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
