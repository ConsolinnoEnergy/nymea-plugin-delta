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

/*!
    \page ws2812fx.html
    \title WS2812FX Control
    \brief Plug-In to control WS2812FX over USB

    \ingroup plugins
    \ingroup nymea-plugins

    \chapter Plugin properties
    Following JSON file contains the definition and the description of all available \l{ThingClass}{DeviceClasses}
    and \l{Vendor}{Vendors} of this \l{DevicePlugin}.

    For more details how to read this JSON file please check out the documentation for \l{The plugin JSON File}.

    \quotefile plugins/deviceplugins/ws2812fx/devicepluginws2812fx.json
*/
#include <QColor>
#include "integrationplugindelta.h"
#include "plugininfo.h"


IntegrationPluginDelta ::IntegrationPluginDelta ()
{

}

void IntegrationPluginDelta::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    QString interface = thing->paramValue(deltainverterThingSerialPortParamTypeId).toString();


    if (m_usedInterfaces.contains(interface)) {
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("This serial port is already used."));
        return;
    }

    // setup Serial Port with all the necessary data
    QSerialPort *serialPort = new QSerialPort(interface, this);
    serialPort->setBaudRate(19200);
    serialPort->setDataBits(QSerialPort::DataBits::Data8);
    serialPort->setParity(QSerialPort::Parity::NoParity);
    serialPort->setStopBits(QSerialPort::StopBits::OneStop);
    serialPort->setFlowControl(QSerialPort::FlowControl::NoFlowControl);

    // If the serial Port doesnt open return
    if (!serialPort->open(QIODevice::ReadWrite)) {
        qCWarning(dcDelta()) << "Could not open serial port" << interface << serialPort->errorString();
        serialPort->deleteLater();
        return info->finish(Thing::ThingErrorHardwareFailure, QT_TR_NOOP("Error opening serial port."));
    }

    // if an error occurs serial Port sends a signal, which then is handeled in the onSerialError function
    connect(serialPort, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(onSerialError(QSerialPort::SerialPortError)));
    // if the serial Port sends the signal readyRead, handle it with the onReadyRead function
    connect(serialPort, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

    // set connected state to true
    qCDebug(dcDelta()) << "Setup successfully serial port" << interface;
    thing->setStateValue(deltainverterConnectedStateTypeId, true);
    m_usedInterfaces.append(interface);
    m_serialPorts.insert(thing, serialPort);

    // initialize reconnectTimer, which pings periodically and checks whether the connection is still running
    if(!m_reconnectTimer) {
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setSingleShot(true);
        m_reconnectTimer->setInterval(5000);
        // every 5seconds check if the connection is still running
        connect(m_reconnectTimer, &QTimer::timeout, this, &IntegrationPluginDelta::onReconnectTimer);
    }

    info->finish(Thing::ThingErrorNoError);
}


void IntegrationPluginDelta::discoverThings(ThingDiscoveryInfo *info)
{
    // Create the list of available serial interfaces
    Q_FOREACH(QSerialPortInfo port, QSerialPortInfo::availablePorts()) {

        qCDebug(dcDelta) << "Found serial port:" << port.portName();
        QString description = port.manufacturer() + " " + port.description();
        ThingDescriptor descriptor(info->thingClassId(), port.portName(), description);
        foreach (Thing *existingThing, myThings().filterByParam( deltainverterThingSerialPortParamTypeId , port.portName())) {
            descriptor.setThingId(existingThing->id());
        }
        ParamList parameters;
        parameters.append(Param(deltainverterThingSerialPortParamTypeId, port.portName()));
        descriptor.setParams(parameters);
        info->addThingDescriptor(descriptor);
    }
    info->finish(Thing::ThingErrorNoError);
}



void IntegrationPluginDelta::thingRemoved(Thing *thing)
{

    if (thing->thingClassId() == deltainverterThingClassId) {
        m_usedInterfaces.removeAll(thing->paramValue( deltainverterThingSerialPortParamTypeId).toString());
        QSerialPort *serialPort = m_serialPorts.take(thing);
        // clean and close the serialPort
        serialPort->flush();
        serialPort->close();
        serialPort->deleteLater();
    }
    // If the Timer isnt needed anymore close it
    if (myThings().empty()) {
        m_reconnectTimer->stop();
        m_reconnectTimer->deleteLater();
        hardwareManager()->pluginTimerManager()->unregisterTimer(m_pluginTimer);
        m_pluginTimer = nullptr;
    }
}




void IntegrationPluginDelta::update(Thing *thing)
{
      sendCommand(thing, CommandType::TotalEnergy);
      sendCommand(thing, CommandType::CurrentPower);
}


// for reusability this function needs to be changed if you want to integrate a similar thing
// The purpose of this function is to read the serialPort string and make sense out of the data
void IntegrationPluginDelta::read(Thing *thing, QByteArray data)
{
    // an receiving message always starts with: 02 06 01
    if (data.at(0)== 0x02 && data.at(1) == 0x06 && data.at(2) == 0x01 ) {
        // data.at(3) always has the length of the message in there
        // +7 because thats the number of bytes the message has, without the message payload
        qint16 totalByteCount =  static_cast<qint16>(data.at(3)) + 7;
        // length of the data payload
        qint8 lengthByte = data.at(3);
        if (totalByteCount != data.size()){
            qCDebug(dcDelta()) << "Size error" ;
            return;
        }

        QByteArray dataForCrc;
        // skip the first byte for Crc, but append the header, size and data bytes
        dataForCrc.append(data[1]);
        dataForCrc.append(data[2]);
        dataForCrc.append(data[3]);
        // add the data payload
        for(int i = 0 ; i< lengthByte; i++){
            dataForCrc.append(data[i+4]);
        }


        quint16 crcResult = m_crc->computeCrc16(dataForCrc);
        quint8 crc1 = crcResult & 0xFF;
        quint8 crc2 = crcResult >> 8;
        // check if the crc is correct
        if (!(crc1 == data[totalByteCount-3])){
            qCDebug(dcDelta()) << "Crc1 error";
            return;

        }
        if (!(crc2 == data[totalByteCount-2])){
            qCDebug(dcDelta()) << "Crc2 error";
            return;
        }

        if (lengthByte > 0x02 ) {
            // check if the commandByte is the same as the function bytes of the payload
            qint16 commandBytes = (data.at(4) << 8) + data.at(5);

            switch (commandBytes) {
            case CommandType::TotalEnergy:{
                if (lengthByte != 0x06){
                    qCDebug(dcDelta()) << "Size error Databyte" ;
                    return;
                }
                // if correct read it and set the state accordingly
                quint32 totalEnergy{0};
                totalEnergy = ((((((totalEnergy | data.at(6)) << 8) | data.at(7))<< 8) | data.at(8)) << 8) | data.at(9);
                thing->setStateValue(deltainverterTotalEnergyProducedStateTypeId , totalEnergy);
                break;
            }
            case CommandType::CurrentPower:{

                if (lengthByte != 0x04){
                    qCDebug(dcDelta()) << "Size error Databyte" ;
                    return;
                }
                // if correct read it and set the state accordingly
                quint16 currentPower{0};
                currentPower = ((currentPower | data.at(6)) << 8) | data.at(7);
                thing->setStateValue(deltainverterCurrentPowerStateTypeId , currentPower);
                break;
            }
            }

        }

    }


}

// function is always used when the QSerialPort is ready to be read
void IntegrationPluginDelta::onReadyRead()
{
    QSerialPort *serialPort =  static_cast<QSerialPort*>(sender());
    Thing *thing = m_serialPorts.key(serialPort);

    QByteArray data;
    while (serialPort->canReadLine()) {
        data = serialPort->readLine();
        qCDebug(dcDelta() ) << "Message received" << data;
        read(thing, data);
    }
}

// periodically send messages, such that we actually get the data back
void IntegrationPluginDelta::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == deltainverterThingClassId) {
        // if no pluginTimer is set yet, set it and everytime it timesout use the update function to send the get messages
        if (!m_pluginTimer) {
            qCDebug(dcDelta()) << "Starting plugin timer...";
            m_pluginTimer = hardwareManager()->pluginTimerManager()->registerTimer(5);
            connect(m_pluginTimer, &PluginTimer::timeout, this, [this, thing] {
                update(thing);
            });

            m_pluginTimer->start();
        }
    }
}


// everytime the connection stops, try to reconnect and update the Thing
void IntegrationPluginDelta::onReconnectTimer()
{
    foreach(Thing *thing, myThings()) {
        if (!thing->stateValue(deltainverterConnectedStateTypeId).toBool()) {
            QSerialPort *serialPort =  m_serialPorts.value(thing);
            if (serialPort) {
                if (serialPort->open(QSerialPort::ReadWrite)) {
                    thing->setStateValue(deltainverterConnectedStateTypeId, true);
                    update(thing);

                } else {
                    thing->setStateValue(deltainverterConnectedStateTypeId, false);
                    m_reconnectTimer->start();
                }
            }
        }
    }
}

// Error checking method
void IntegrationPluginDelta::onSerialError(QSerialPort::SerialPortError error)
{
    QSerialPort *serialPort =  static_cast<QSerialPort*>(sender());
    Thing *thing = m_serialPorts.key(serialPort);

    if (error != QSerialPort::NoError && serialPort->isOpen()) {
        qCCritical(dcDelta()) << "Serial port error:" << error << serialPort->errorString();
        m_reconnectTimer->start();
        serialPort->close();
        thing->setStateValue(deltainverterConnectedStateTypeId, false);
    }
}

// for reusability this function needs to be changed if you want to integrate a similar thing
// the purpose of this function is to build the request message so you get the data from the thing
// the request identifier bits are defined in the CommandType enum
QByteArray IntegrationPluginDelta::build(CommandType commandType)
{
    QByteArray command;
    command.append(0x05);   // Header
    command.append(0x02);   // Header
    command.append(commandType >> 8 & 0xFF);    // left 2 bytes (example enum TotalEnergy: 17)
    command.append(commandType & 0xFF);         // right 2 bytes (example enum TotalEnergy: 05)


    quint16 crcResult = m_crc->computeCrc16(command); // compute the crc

    quint8 crc1 = crcResult & 0xFF; // second crc byte
    quint8 crc2 = crcResult >> 8;   // first crc byte
    // note: little Endian thats why second byte first
    command.append(crc1); // second crc byte (example enum TotalEnergy: 05)
    command.append(crc2); // first crc byte (example enum TotalEnergy: 17)
    command.prepend(0x02); // part of the Header, but needs to be preppended later. Otherwise the crc Calculation doesnt work
    command.append(0x03); // end of the header
    return command;
}


// build the Command based ont the Commandtype defined in the CommandType enum in the .h file
void IntegrationPluginDelta::sendCommand(Thing *thing, CommandType commandType)
{

    QByteArray command = build(commandType);
    qDebug(dcDelta()) << "Sending command " << commandType << command;
    QSerialPort *serialPort = m_serialPorts.value(thing);
    if (!serialPort)
       qCWarning(dcDelta) << "Error, serial port not available";
    if (serialPort->write(command) != command.length()) {
        qCWarning(dcDelta) << "Error writing to serial port";

    }

}
