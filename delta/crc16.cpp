#include "crc16.h"

Crc16::Crc16(const quint16 polynomial, QObject *parent)
    : QObject{parent},
      m_polynomial{polynomial}
{
        quint16 value;
        quint16 temp;
        for (quint16 i = 0; i < 256; ++i) {
            value = 0;
            temp = i;
            for (int j = 0; j < 8; ++j) {
                if (((value ^ temp) & 0b01) != 0) {
                    value = ((value >> 1) ^ m_polynomial);
                } else {
                    value = value >> 1;
                }
                temp = temp >> 1;
            }
            m_table[i] = value;
        }
}

quint16 Crc16::computeCrc16(const QByteArray &bytes) {
    quint16 crc = 0;
    for (int i = 0; i < bytes.size(); ++i) {
        quint8 index = 0xFF & (crc ^ bytes.at(i));
        crc = ((crc >> 8) ^ m_table[index]);
    }
    return crc;
}
