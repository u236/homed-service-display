#ifndef DEVICE_H
#define DEVICE_H



// NOT REVIEWED
#define MAX_PAGES                   4
#define STORE_DATABASE_DELAY        20

#define HEADER_HEIGHT               60
#define NAVIGATION_HEIGHT           40
#define NAVIGATION_GAP              2
#define CLOCK_HEIGHT                120
#define CELL_PADDING                10
#define ROW_GAP                     12

#define BACKGROUND_COLOR            "#000000"
#define HEADER_COLOR                "#888888"
#define ACCENT_COLOR                "#f6aa39"
#define TEXT_COLOR                  "#ffffff"
//



#include "endpoint.h"

class DeviceObject : public AbstractDeviceObject
{

public:

    DeviceObject(const QString &id, qint16 width, qint16 height, qint16 home, qint16 timeout, const QString &name, const QJsonArray &pages) :
        AbstractDeviceObject(name.isEmpty() ? id : name), m_id(id), m_pages(pages), m_width(width), m_height(height), m_home(home), m_timeout(timeout) {}

    inline QString id(void) { return m_id; }
    inline QJsonArray pages(void) { return m_pages; }

    inline qint16 width(void) { return m_width; }
    inline qint16 height(void) { return m_height; }
    inline qint16 home(void) { return m_home; }
    inline qint16 timeout(void) { return m_timeout; } // TODO: implement home page timeout

    QList <QByteArray> layout(void);

private:

    QString m_id;
    QJsonArray m_pages;
    qint16 m_width, m_height, m_home, m_timeout;

};

class DeviceList : public QObject, public QList <Device>
{
    Q_OBJECT

public:

    DeviceList(QSettings *config, QObject *parent);
    ~DeviceList(void);

    inline bool names(void) { return m_names; }

    void init(void);
    void store(bool sync = false);

    Device byName(const QString &name, int *index = nullptr);
    Device parse(const QJsonObject &json);

private:

    QTimer *m_timer;

    QFile m_file;
    bool m_names, m_sync;

    void unserialize(const QJsonArray &devices);
    QJsonArray serialize(void);

private slots:

    void writeDatabase(void);

};

inline QDebug operator << (QDebug debug, const Device &device) { return debug << "device" << device->name(); }

#endif
