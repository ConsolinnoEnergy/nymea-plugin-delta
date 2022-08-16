/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINDELTA
#define INTEGRATIONPLUGINDELTA

#include "integrations/integrationplugin.h"

#include <QTimer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "crc16.h"
#include "plugintimer.h"


class IntegrationPluginDelta : public IntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationplugindelta.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    enum CommandType {
        TotalEnergy = 0x1705,
        CurrentPower = 0x1009,
        TesterID = 0x0006,

    };
    Q_ENUM(CommandType)

    explicit IntegrationPluginDelta();

    void setupThing(ThingSetupInfo *info) override;
    void thingRemoved(Thing *thing) override;
    void discoverThings(ThingDiscoveryInfo *info) override;
    void postSetupThing(Thing *thing) override;


private:

    QByteArray build(CommandType commandtype);
    void read(Thing *thing, QByteArray data);
    void update(Thing *thing);
    void sendCommand(Thing *thing, CommandType commandType);
    QHash<Thing *, QSerialPort *> m_serialPorts;
    QList<QString> m_usedInterfaces;
    QHash<CommandType, ThingActionInfo*> m_pendingActions;

    QTimer *m_reconnectTimer = nullptr;
    PluginTimer *m_pluginTimer = nullptr;
    qint8 m_byteId;
    Crc16 *m_crc = new Crc16(0xA001); // 0xA001 is the CRC polynom used by the Delta Inverter protocol.



private slots:

    void onReadyRead();
    void onReconnectTimer();
    void onSerialError(QSerialPort::SerialPortError error);
    void onCurrentPower(Thing *thing);
    void onTotalEnergy(Thing *thing);

signals:

    void currentPowerChanged(Thing *thing);



};

#endif // INTEGRATIONPLUGINWS2812FX_H
