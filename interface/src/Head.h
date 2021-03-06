//
//  Head.h
//  interface
//
//  Created by Philip Rosedale on 9/11/12.
//  Copyright (c) 2012 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__head__
#define __interface__head__

#include <iostream>
#include "AgentData.h"
#include "Field.h"
#include "world.h"
#include "Head.h"
#include "Hand.h"
#include "InterfaceConfig.h"
#include "SerialInterface.h"

enum eyeContactTargets {LEFT_EYE, RIGHT_EYE, MOUTH};

#define FWD 0
#define BACK 1 
#define LEFT 2 
#define RIGHT 3 
#define UP 4 
#define DOWN 5
#define ROT_LEFT 6 
#define ROT_RIGHT 7 
#define MAX_DRIVE_KEYS 8

class Head : public AgentData {
    public:
        Head();
        ~Head();
        Head(const Head &otherHead);
        Head* clone() const;
    
        void reset();
        void UpdatePos(float frametime, SerialInterface * serialInterface, int head_mirror, glm::vec3 * gravity);
        void setNoise (float mag) { noise = mag; }
        void setPitch(float p) {Pitch = p; }
        void setYaw(float y) {Yaw = y; }
        void setRoll(float r) {Roll = r; };
        void setScale(float s) {scale = s; };
        void setRenderYaw(float y) {renderYaw = y;}
        void setRenderPitch(float p) {renderPitch = p;}
        float getRenderYaw() {return renderYaw;}
        float getRenderPitch() {return renderPitch;}
        void setLeanForward(float dist);
        void setLeanSideways(float dist);
        void addPitch(float p) {Pitch -= p; }
        void addYaw(float y){Yaw -= y; }
        void addRoll(float r){Roll += r; }
        void addLean(float x, float z);
        float getPitch() {return Pitch;}
        float getRoll() {return Roll;}
        float getYaw() {return Yaw;}
        float getLastMeasuredYaw() {return YawRate;}
        
        void render(int faceToFace, int isMine);
        void simulate(float);
        
        //  Send and receive network data
        int getBroadcastData(char * data);
        void parseData(void *data, int size);
        
        float getLoudness() {return loudness;};
        float getAverageLoudness() {return averageLoudness;};
        void setAverageLoudness(float al) {averageLoudness = al;};
        void setLoudness(float l) {loudness = l;};
        
        void SetNewHeadTarget(float, float);
        glm::vec3 getPos() { return position; };
        void setPos(glm::vec3 newpos) { position = newpos; };
    
        //  Set what driving keys are being pressed to control thrust levels
        void setDriveKeys(int key, bool val) { driveKeys[key] = val; };
        bool getDriveKeys(int key) { return driveKeys[key]; };
    
        //  Set/Get update the thrust that will move the avatar around
        void setThrust(glm::vec3 newThrust) { thrust = newThrust; };
        void addThrust(glm::vec3 newThrust) { thrust += newThrust; };
        glm::vec3 getThrust() { return thrust; };
    
        Hand * hand;
    
    private:
        float noise;
        float Pitch;
        float Yaw;
        float Roll;
        float PitchRate;
        float YawRate;
        float RollRate;
        float EyeballPitch[2];
        float EyeballYaw[2];
        float EyebrowPitch[2];
        float EyebrowRoll[2];
        float EyeballScaleX, EyeballScaleY, EyeballScaleZ;
        float interPupilDistance;
        float interBrowDistance;
        float NominalPupilSize;
        float PupilSize;
        float MouthPitch;
        float MouthYaw;
        float MouthWidth;
        float MouthHeight;
        float leanForward;
        float leanSideways;
        float PitchTarget; 
        float YawTarget; 
        float NoiseEnvelope;
        float PupilConverge;
        float scale;
        
        //  Sound loudness information
        float loudness, lastLoudness;
        float averageLoudness;
        float audioAttack;
        float browAudioLift;
    
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 thrust;
    
        int driveKeys[MAX_DRIVE_KEYS];
        
        int eyeContact;
        eyeContactTargets eyeContactTarget;
    
        GLUquadric *sphere;
    
        void readSensors();
        float renderYaw, renderPitch;       //   Pitch from view frustum when this is own head.
    
};

#endif