//
//  Quat.h
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 1/29/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Scriptable Quaternion class library.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef __hifi__Quat__
#define __hifi__Quat__

#include <glm/gtc/quaternion.hpp>

#include <QObject>
#include <QString>

/// Scriptable interface a Quaternion helper class object. Used exclusively in the JavaScript API
class Quat : public QObject {
    Q_OBJECT

public slots:
    glm::quat multiply(const glm::quat& q1, const glm::quat& q2);
    glm::quat fromVec3Degrees(const glm::vec3& vec3); // degrees
    glm::quat fromVec3Radians(const glm::vec3& vec3); // radians
    glm::quat fromPitchYawRollDegrees(float pitch, float yaw, float roll); // degrees
    glm::quat fromPitchYawRollRadians(float pitch, float yaw, float roll); // radians
    glm::quat inverse(const glm::quat& q);
    glm::vec3 getFront(const glm::quat& orientation);
    glm::vec3 getRight(const glm::quat& orientation);
    glm::vec3 getUp(const glm::quat& orientation);
    glm::vec3 safeEulerAngles(const glm::quat& orientation); // degrees
    glm::quat angleAxis(float angle, const glm::vec3& v);   // degrees
    glm::quat mix(const glm::quat& q1, const glm::quat& q2, float alpha);
    void print(const QString& lable, const glm::quat& q);
};

#endif /* defined(__hifi__Quat__) */
