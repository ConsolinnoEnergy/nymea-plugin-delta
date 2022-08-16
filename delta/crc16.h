#ifndef CRC16_H
#define CRC16_H

#include <QObject>
#include <QByteArray>

class Crc16 : public QObject
{
    Q_OBJECT
public:
    explicit Crc16(const quint16 polynomial, QObject *parent = nullptr);

    quint16 computeCrc16(const QByteArray &bytes);

private:
    const quint16 m_polynomial;
    quint16 m_table[256];


};

#endif // CRC16_H
