/*
 * File:   SatViewer.cpp
 * Author: Ivan Ryazanov
 *
 * Created on April 13, 2019, 9:45 PM
 */

#include "SatViewer.h"
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
#include <float.h>

SatViewer::SatViewer() {
    m_host = QHostAddress::LocalHost;
    m_port = DEFAULT_PORT;

    m_satellites.clear();
    m_locations.clear();
    m_currentSatellite = nullptr;
    m_currentLocation = nullptr;
    m_time = 0.0;

    connect(this, SIGNAL(readyRead()), this, SLOT(readyReadSlot()));
    connect(this, SIGNAL(connected()), this, SLOT(connectedSlot()));

    m_timerSlowId = startTimer(5000);
    m_timerFastId = startTimer(1000);
    loadSatellitesJson();
}

SatViewer::~SatViewer() {
    saveSatellitesJson();
    // TODO
    //    for (const auto& sat : m_satellites) {
    //        if (sat != nullptr) {
    //            delete sat;
    //        }
    //    }
    //    m_satellites.clear();

    //    for (const auto& loc : m_locations) {
    //        if (loc != nullptr) {
    //            delete loc;
    //        }
    //    }
    //    m_locations.clear();
}

void SatViewer::readyReadSlot() {
    QJsonParseError parseError;

    if (m_byteArray.size() > 0) { // TODO: check warning
        qWarning() << "m_byteArray isn't null:" << m_byteArray.size();
    }

    while (canReadLine()) {
        m_byteArray.append(readLine());
        QJsonDocument jsonDoc = QJsonDocument::fromJson(m_byteArray, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            break;
        }
        m_byteArray.clear();

        QJsonObject jsonObject = jsonDoc.object();
        if (jsonObject.contains("SatMap")) {
            for (const auto &item : jsonObject.value("SatMap").toObject()) {
                QJsonObject satelliteObject = item.toObject();
                QJsonArray jsonArray = satelliteObject.value("Coords").toArray();
                QJsonValue name = satelliteObject.value("Title");
                QJsonValue satnum = satelliteObject.value("Satnum");
                if (m_satellites.contains(name.toString())) {
                    // nothing
                } else {
                    Satellite *sat = new Satellite();
                    sat->setSatnum(satnum.toInt());
                    sat->setName(name.toString());
                    appendSatellite(sat);
                }
                Satellite *sat = m_satellites[name.toString()];
                sat->setAbsoluteCoords(
                    jsonArray[0].toDouble() * 1000.0, jsonArray[1].toDouble() * 1000.0,
                    jsonArray[2].toDouble() * 1000.0, jsonArray[3].toDouble() * 1000.0,
                    jsonArray[4].toDouble() * 1000.0, jsonArray[5].toDouble() * 1000.0);
            }
        }
        if (jsonObject.contains("TleMap")) {
            QJsonObject tleMap = jsonObject.value("TleMap").toObject();
            // qWarning() << tleMap.keys();
        }
    }
}

void SatViewer::connectedSlot() {
    write(QJsonDocument(jsonIdList()).toJson());
    write("\n");
    write("{\"Names\": true}\n");
}

void SatViewer::reconnect() {
    if (state() == QTcpSocket::ConnectedState || state() == QTcpSocket::ConnectingState) {
    } else {
        connectToHost(m_host, m_port);
    }
}

void SatViewer::requestGosat() {
    if (state() == QTcpSocket::ConnectedState) {
        // write("{\n    \"idList\": [\n        25544,\n        43467,\n        43548,\n        "
        //       "43556\n    ]\n}\n");
        write("{}\n");
    }
}

void SatViewer::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_timerSlowId) {
        reconnect();
    } else if (event->timerId() == m_timerFastId) {
        requestGosat();
    }
}

void SatViewer::appendSatellite(Satellite *sat) {
    m_satellites[sat->name()] = sat;
}

void SatViewer::appendLocation(Location *loc) {
    m_locations.push_back(loc);
}

void SatViewer::removeSatellite(Satellite *sat) {
    // m_satellites.remove(sat);
    //    int pos = satList.indexOf(sat);
    //    if (pos == -1) return;
    //    ioList.deleteSat(satList.at(pos));
    //    satList.removeAt(pos);
    //    delete sat;
    //    setIndexSat(pos - 1);
}

void SatViewer::removeLocation(Location *loc) {
    m_locations.remove(loc);
    //    int pos = locList.indexOf(loc);
    //    if (pos == -1) return;
    //    ioList.deleteLoc(locList.at(pos));
    //    locList.removeAt(pos);
    //    delete loc;
    //    setIndexLoc(pos - 1);
}

void SatViewer::clearSatellites() {
    m_satellites.clear();
}

void SatViewer::clearLocations() {
    m_locations.clear();
}

void SatViewer::loadSatellitesJson() {
    QString path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QDir dir = QDir(path);
    dir.mkpath(dir.absolutePath());

    QFile file(dir.absoluteFilePath("satellites.json"));
    file.open(QFile::ReadOnly | QFile::Text);
    QString text = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(text.toUtf8());
    QJsonObject jsonObject = jsonDoc.object();
    QJsonValue value = jsonObject.value("idList");
    QJsonArray jsonArray = value.toArray();
    for (const auto &id : jsonArray) {
        m_satellites[id.toString()] = new Satellite();
    }
}

QJsonObject SatViewer::jsonIdList() {
    QJsonArray jsonArray;
    for (const auto &id : m_satellites.keys()) {
        jsonArray.append(id);
    }
    QJsonObject jsonObject;
    jsonObject["idList"] = jsonArray;
    return jsonObject;
}

void SatViewer::saveSatellitesJson() {
    QString path = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    QDir dir = QDir(path);
    dir.mkpath(dir.absolutePath());

    QFile file(dir.absoluteFilePath("satellites.json"));
    file.open(QFile::WriteOnly | QFile::Text);

    file.write(QJsonDocument(jsonIdList()).toJson());
    file.close();
}

Satellite *SatViewer::currentSatellite() {
    return m_currentSatellite;
    //    if ((m_currentSatellite >= 0) &&
    //            ((long unsigned int)m_currentSatellite < m_satellites.size())) {
    //        auto item = m_satellites.begin();
    //        std::advance(item, m_currentSatellite);
    //        return (*item);
    //    }
    //    else {
    //        return nullptr;
    //    }
}

Location *SatViewer::currentLocation() {
    return m_currentLocation;
}

void SatViewer::setCurrentSatelliteIndex(long unsigned int index) {
    if (index < m_satellites.size()) {
        auto it = m_satellites.begin();
        std::advance(it, index);
        m_currentSatellite = (*it);
    } else {
        m_currentSatellite = nullptr;
    }
    emit currentChanged(m_currentSatellite, m_currentLocation, &m_time);
}

void SatViewer::setCurrentLocationIndex(long unsigned int index) {
    if (index < m_locations.size()) {
        auto it = m_locations.begin();
        std::advance(it, index);
        m_currentLocation = (*it);
    } else {
        m_currentLocation = nullptr;
    }
    emit currentChanged(m_currentSatellite, m_currentLocation, &m_time);
}

void SatViewer::setCurrentSatellite(Satellite *sat) {
    m_currentSatellite = sat;
    emit currentChanged(m_currentSatellite, m_currentLocation, &m_time);
}

void SatViewer::setCurrentLocation(Location *loc) {
    m_currentLocation = loc;
    emit currentChanged(m_currentSatellite, m_currentLocation, &m_time);
}

void SatViewer::setTime(double value) {
    m_time = value;
    emit timeChanged();
}

void SatViewer::aerv(const double loc_rg[], const double sat_rg[], double aerv[]) {
    double m[3][3];

    double d[3];
    double p = sqrt(loc_rg[0] * loc_rg[0] + loc_rg[1] * loc_rg[1]);
    double r = sqrt(loc_rg[0] * loc_rg[0] + loc_rg[1] * loc_rg[1] + loc_rg[2] * loc_rg[2]);

    double dd[] = {loc_rg[0] - sat_rg[0], loc_rg[1] - sat_rg[1], loc_rg[2] - sat_rg[2],
                   loc_rg[3] - sat_rg[3], loc_rg[4] - sat_rg[4], loc_rg[5] - sat_rg[5]};

    aerv[2] = sqrt(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]);

    if ((p < DBL_EPSILON) || (r < DBL_EPSILON)) {
        aerv[0] = 0.0;
        aerv[1] = 0.0;
        aerv[2] = 0.0;
        aerv[3] = 0.0;
        aerv[4] = 0.0;
        aerv[5] = 0.0;
        return;
    }

    m[0][0] = -loc_rg[1] / p;
    m[0][1] = loc_rg[0] / p;
    m[0][2] = 0.0;
    m[1][0] = -(loc_rg[0] * loc_rg[2] / (p * r));
    m[1][1] = -(loc_rg[1] * loc_rg[2] / (p * r));
    m[1][2] = p / r;
    m[2][0] = loc_rg[0] / r;
    m[2][1] = loc_rg[1] / r;
    m[2][2] = loc_rg[2] / r;

    for (size_t j = 0; j < 3; j++) {
        d[j] = 0.0;
        for (size_t i = 0; i < 3; i++) {
            d[j] += (sat_rg[i] - loc_rg[i]) * m[j][i];
        }
    }

    double s = d[2] / sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);

    aerv[0] = atan2(d[0], d[1]);
    aerv[1] = atan2(s, sqrt(1.0 - s * s));
    aerv[3] = (dd[0] * dd[3] + dd[1] * dd[4] + dd[2] * dd[5]) / aerv[2];
}
