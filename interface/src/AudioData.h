//
//  AudioData.h
//  interface
//
//  Created by Stephen Birarda on 1/29/13.
//  Copyright (c) 2013 HighFidelity, Inc. All rights reserved.
//

#ifndef __interface__AudioData__
#define __interface__AudioData__

#include <iostream>
#include <stdint.h>
#include <glm/glm.hpp>
#include "AudioRingBuffer.h"
#include "UDPSocket.h"
#include "Head.h"

class AudioData {
    public:
        AudioData();
        ~AudioData();
        AudioRingBuffer *ringBuffer;

        UDPSocket *audioSocket;
    
        Head *linkedHead;
    
        // store current mixer address and port
        in_addr_t mixerAddress;
        in_port_t mixerPort;
    
        timeval lastCallback;
        float averagedLatency;
        float measuredJitter;
        float jitterBuffer;
        int wasStarved;
        
        float lastInputLoudness;
        float averagedInputLoudness;
    
        bool mixerLoopbackFlag;
        bool playWalkSound;
};

#endif /* defined(__interface__AudioData__) */
