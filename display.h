#ifndef DISPLAY_H
#define DISPLAY_H



// NOT REVIEWED
#define MAX_PAGES                   4
#define STORE_DATABASE_DELAY        20

#define HEADER_HEIGHT               60
#define NAVIGATION_HEIGHT           40
#define NAVIGATION_GAP              2
#define CLOCK_HEIGHT                120
#define CELL_PADDING                10
#define ROW_GAP                     12
#define WINDOW_ID                   10 // TODO: refactor magic numbers

#define BACKGROUND_COLOR            "#000000"
#define HEADER_COLOR                "#888888"
#define BUTTON_COLOR                "#616161"
#define DISABLED_COLOR              "#555555"
#define ACCENT_COLOR                "#F6AA39"
#define TEXT_COLOR                  "#FFFFFF"
#define ALARM_COLOR                 "#F44336"
//



#include "endpoint.h"

struct DisplayItem
{
    quint16 page, id;
    QString name, endpoint, expose, scene;
};

class DisplayObject;
typedef QSharedPointer <DisplayObject> Display;

class DisplayObject : public AbstractDeviceObject
{

public:

    DisplayObject(const QString &id, quint16 width, quint16 height, quint16 home, const QString &name, const QJsonArray &pages) :
        AbstractDeviceObject(name.isEmpty() ? id : name), m_id(id), m_width(width), m_height(height), m_home(home), m_control(0), m_time(0), m_pages(pages) {}

    inline QString id(void) { return m_id; }

    inline QString endpoint(void) { return m_endpoint; }
    inline void setEndpoint(const QString &value) { m_endpoint = value; }

    inline quint16 width(void) { return m_width; }
    inline quint16 height(void) { return m_height; }
    inline quint16 home(void) { return m_home; }

    inline quint16 control(void) { return m_control; }
    inline void setControl(quint16 value) { m_control = value; }

    inline qint64 time(void) { return m_time; }
    inline void setTime(qint64 value) { m_time = value; }

    inline QJsonArray pages(void) { return m_pages; }

    inline QList <QString> &properties(void) { return m_properties; }
    inline QList <DisplayItem> &items(void) { return m_items; }

    QList <QString> layout(void);
    QList <QString> window(const QString &name, const QJsonObject &options);

private:

    QString m_id, m_endpoint;

    quint16 m_width, m_height, m_home, m_control;
    qint64 m_time;

    QJsonArray m_pages;

    QList <QString> m_properties;
    QList <DisplayItem> m_items;

    void add(QList <QString> &list, const QString &type, quint16 page, quint16 id, quint16 x, quint16 y, quint16 width, quint16 height, const QJsonObject &data = QJsonObject());

};

class DisplayList : public QObject, public QList <Display>
{
    Q_OBJECT

public:

    DisplayList(QSettings *config, QObject *parent);
    ~DisplayList(void);

    inline bool names(void) { return m_names; }

    void init(void);
    void store(bool sync = false);

    Display byName(const QString &name, int *index = nullptr);
    Display parse(const QJsonObject &json);

private:

    QTimer *m_timer;

    QFile m_file;
    bool m_names, m_sync;

    void unserialize(const QJsonArray &displays);
    QJsonArray serialize(void);

private slots:

    void writeDatabase(void);

};

inline QDebug operator << (QDebug debug, const Display &display) { return debug << "device" << display->name(); }

#endif
