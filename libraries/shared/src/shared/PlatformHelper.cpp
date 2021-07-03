//
//  Created by Bradley Austin Davis on 2019/08/22
//  Copyright 2013-2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "PlatformHelper.h"

void PlatformHelper::onSleep() {
    if (_awake.exchange(false)) {
        qInfo() << "Entering sleep or hibernation.";
        emit systemWillSleep();
    }
}

void PlatformHelper::onWake() {
    if (!_awake.exchange(true)) {
        qInfo() << "Waking up from sleep or hibernation.";
        emit systemWillWake();
    }
}

void PlatformHelper::shutdown() {
    DependencyManager::destroy<PlatformHelper>();
}

PlatformHelper* PlatformHelper::instance() {
    return DependencyManager::get<PlatformHelper>().get();
}

