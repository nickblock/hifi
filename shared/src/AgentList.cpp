//
//  AgentList.cpp
//  hifi
//
//  Created by Stephen Birarda on 2/15/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#include <pthread.h>
#include <cstring>
#include <stdlib.h>
#include "AgentList.h"
#include "SharedUtil.h"

#ifdef _WIN32
#include "Syssocket.h"
#else
#include <arpa/inet.h>
#endif

const char * SOLO_AGENT_TYPES_STRING = "MV";
char DOMAIN_HOSTNAME[] = "highfidelity.below92.com";
char DOMAIN_IP[100] = "";    //  IP Address will be re-set by lookup on startup
const int DOMAINSERVER_PORT = 40102;

bool silentAgentThreadStopFlag = false;
bool domainServerCheckinStopFlag = false;
pthread_mutex_t vectorChangeMutex = PTHREAD_MUTEX_INITIALIZER;

int unpackAgentId(unsigned char *packedData, uint16_t *agentId) {
    memcpy(packedData, agentId, sizeof(uint16_t));
    return sizeof(uint16_t);
}

int packAgentId(unsigned char *packStore, uint16_t agentId) {
    memcpy(&agentId, packStore, sizeof(uint16_t));
    return sizeof(uint16_t);
}

AgentList::AgentList(char newOwnerType, unsigned int newSocketListenPort) : agentSocket(newSocketListenPort) {
    ownerType = newOwnerType;
    socketListenPort = newSocketListenPort;
    lastAgentId = 0;
}

AgentList::~AgentList() {
    // stop the spawned threads, if they were started
    stopSilentAgentRemovalThread();
    stopDomainServerCheckInThread();
}

std::vector<Agent>& AgentList::getAgents() {
    return agents;
}

UDPSocket& AgentList::getAgentSocket() {
    return agentSocket;
}

char AgentList::getOwnerType() {
    return ownerType;
}

unsigned int AgentList::getSocketListenPort() {
    return socketListenPort;
}

void AgentList::processAgentData(sockaddr *senderAddress, void *packetData, size_t dataBytes) {
    switch (((char *)packetData)[0]) {
        case 'D': {
            // list of agents from domain server
            updateList((unsigned char *)packetData, dataBytes);
            break;
        }
        case 'H': {
            // head data from another agent
            updateAgentWithData(senderAddress, packetData, dataBytes);
            break;
        }
        case 'P': {
            // ping from another agent
            //std::cout << "Got ping from " << inet_ntoa(((sockaddr_in *)senderAddress)->sin_addr) << "\n";
            char reply[] = "R";
            agentSocket.send(senderAddress, reply, 1);  
            break;
        }
        case 'R': {
            // ping reply from another agent
            //std::cout << "Got ping reply from " << inet_ntoa(((sockaddr_in *)senderAddress)->sin_addr) << "\n";
            handlePingReply(senderAddress);
            break;
        }
    }
}

void AgentList::updateAgentWithData(sockaddr *senderAddress, void *packetData, size_t dataBytes) {
    // find the agent by the sockaddr
    int agentIndex = indexOfMatchingAgent(senderAddress);
    
    if (agentIndex != -1) {
        Agent *matchingAgent = &agents[agentIndex];
        
        matchingAgent->setLastRecvTimeUsecs(usecTimestampNow());
        
        if (matchingAgent->getLinkedData() == NULL) {
            if (linkedDataCreateCallback != NULL) {
                linkedDataCreateCallback(matchingAgent);
            }
        }
        
        matchingAgent->getLinkedData()->parseData(packetData, dataBytes);
    }

}

int AgentList::indexOfMatchingAgent(sockaddr *senderAddress) {
    for(std::vector<Agent>::iterator agent = agents.begin(); agent != agents.end(); agent++) {
        if (agent->getActiveSocket() != NULL && socketMatch(agent->getActiveSocket(), senderAddress)) {
            return agent - agents.begin();
        }
    }
    
    return -1;
}

uint16_t AgentList::getLastAgentId() {
    return lastAgentId;
}

void AgentList::increaseAgentId() {
    ++lastAgentId;
}

int AgentList::updateList(unsigned char *packetData, size_t dataBytes) {
    int readAgents = 0;

    char agentType;
    uint16_t agentId;
    
    // assumes only IPv4 addresses
    sockaddr_in agentPublicSocket;
    agentPublicSocket.sin_family = AF_INET;
    sockaddr_in agentLocalSocket;
    agentLocalSocket.sin_family = AF_INET;
    
    unsigned char *readPtr = packetData + 1;
    unsigned char *startPtr = packetData;
    
    while((readPtr - startPtr) < dataBytes) {
        agentType = *readPtr++;
        readPtr += unpackAgentId(readPtr, (uint16_t *)&agentId);
        readPtr += unpackSocket(readPtr, (sockaddr *)&agentPublicSocket);
        readPtr += unpackSocket(readPtr, (sockaddr *)&agentLocalSocket);
        
        addOrUpdateAgent((sockaddr *)&agentPublicSocket, (sockaddr *)&agentLocalSocket, agentType, agentId);
    }  

    return readAgents;
}

bool AgentList::addOrUpdateAgent(sockaddr *publicSocket, sockaddr *localSocket, char agentType, uint16_t agentId) {
    std::vector<Agent>::iterator agent;
    
    for (agent = agents.begin(); agent != agents.end(); agent++) {
        if (agent->matches(publicSocket, localSocket, agentType)) {
            // we already have this agent, stop checking
            break;
        }
    }
    
    if (agent == agents.end()) {
        // we didn't have this agent, so add them
        
        Agent newAgent = Agent(publicSocket, localSocket, agentType, agentId);
        
        if (socketMatch(publicSocket, localSocket)) {
            // likely debugging scenario with DS + agent on local network
            // set the agent active right away
            newAgent.activatePublicSocket();
        }
        
        if (newAgent.getType() == 'M' && audioMixerSocketUpdate != NULL) {
            // this is an audio mixer
            // for now that means we need to tell the audio class
            // to use the local socket information the domain server gave us
            sockaddr_in *publicSocketIn = (sockaddr_in *)publicSocket;
            audioMixerSocketUpdate(publicSocketIn->sin_addr.s_addr, publicSocketIn->sin_port);
        } else if (newAgent.getType() == 'V') {
            newAgent.activatePublicSocket();
        }
        
        std::cout << "Added agent - " << &newAgent << "\n";
        
        pthread_mutex_lock(&vectorChangeMutex);
        agents.push_back(newAgent);
        pthread_mutex_unlock(&vectorChangeMutex);
        
        return true;
    } else {
        
        if (agent->getType() == 'M' || agent->getType() == 'V') {
            // until the Audio class also uses our agentList, we need to update
            // the lastRecvTimeUsecs for the audio mixer so it doesn't get killed and re-added continously
            agent->setLastRecvTimeUsecs(usecTimestampNow());
        }
        
        // we had this agent already, do nothing for now
        return false;
    }    
}

void AgentList::broadcastToAgents(char *broadcastData, size_t dataBytes) {
    for(std::vector<Agent>::iterator agent = agents.begin(); agent != agents.end(); agent++) {
        // for now assume we only want to send to other interface clients
        // until the Audio class uses the AgentList
        if (agent->getActiveSocket() != NULL && (agent->getType() == 'I' || agent->getType() == 'V')) {
            // we know which socket is good for this agent, send there
            agentSocket.send(agent->getActiveSocket(), broadcastData, dataBytes);
        }
    }
}

void AgentList::pingAgents() {
    char payload[] = "P";
    
    for(std::vector<Agent>::iterator agent = agents.begin(); agent != agents.end(); agent++) {
        if (agent->getType() == 'I') {
            if (agent->getActiveSocket() != NULL) {
                // we know which socket is good for this agent, send there
                agentSocket.send(agent->getActiveSocket(), payload, 1);
            } else {
                // ping both of the sockets for the agent so we can figure out
                // which socket we can use
                agentSocket.send(agent->getPublicSocket(), payload, 1);
                agentSocket.send(agent->getLocalSocket(), payload, 1);
            }
        }
    }
}

void AgentList::handlePingReply(sockaddr *agentAddress) {
    for(std::vector<Agent>::iterator agent = agents.begin(); agent != agents.end(); agent++) {
        // check both the public and local addresses for each agent to see if we find a match
        // prioritize the private address so that we prune erroneous local matches        
        if (socketMatch(agent->getPublicSocket(), agentAddress)) {
            agent->activatePublicSocket();
            break;
        } else if (socketMatch(agent->getLocalSocket(), agentAddress)) {
            agent->activateLocalSocket();
            break;
        }
    }
}

void *removeSilentAgents(void *args) {
    std::vector<Agent> *agents = (std::vector<Agent> *)args;
    double checkTimeUSecs, sleepTime;
    
    while (!silentAgentThreadStopFlag) {
        checkTimeUSecs = usecTimestampNow();
        
        for(std::vector<Agent>::iterator agent = agents->begin(); agent != agents->end();) {
            
            pthread_mutex_t * agentDeleteMutex = &agent->deleteMutex;
            
            if ((checkTimeUSecs - agent->getLastRecvTimeUsecs()) > AGENT_SILENCE_THRESHOLD_USECS && agent->getType() != 'V'
                && pthread_mutex_trylock(agentDeleteMutex) == 0) {
                
                std::cout << "Killing agent " << &(*agent)  << "\n";
                
                // make sure the vector isn't currently adding an agent
                pthread_mutex_lock(&vectorChangeMutex);
                agent = agents->erase(agent);
                pthread_mutex_unlock(&vectorChangeMutex);
                
                // release the delete mutex and destroy it
                pthread_mutex_unlock(agentDeleteMutex);
                pthread_mutex_destroy(agentDeleteMutex);
            } else {
                agent++;
            }
        }
        
        
        sleepTime = AGENT_SILENCE_THRESHOLD_USECS - (usecTimestampNow() - checkTimeUSecs);
        #ifdef _WIN32
        Sleep( static_cast<int>(1000.0f*sleepTime) );
        #else
        usleep(sleepTime);
        #endif
    }
    
    pthread_exit(0);
    return NULL;
}

void AgentList::startSilentAgentRemovalThread() {
    pthread_create(&removeSilentAgentsThread, NULL, removeSilentAgents, (void *)&agents);
}

void AgentList::stopSilentAgentRemovalThread() {
    silentAgentThreadStopFlag = true;
    pthread_join(removeSilentAgentsThread, NULL);
}

void *checkInWithDomainServer(void *args) {
    
    AgentList *parentAgentList = (AgentList *)args;
    
    timeval lastSend;
    unsigned char output[7];
    
    in_addr_t localAddress = getLocalAddress();
    
    //  Lookup the IP address of the domain server if we need to
    if (atoi(DOMAIN_IP) == 0) {
        struct hostent* pHostInfo;
        if ((pHostInfo = gethostbyname(DOMAIN_HOSTNAME)) != NULL) {
            sockaddr_in tempAddress;
            memcpy(&tempAddress.sin_addr, pHostInfo->h_addr_list[0], pHostInfo->h_length);
            strcpy(DOMAIN_IP, inet_ntoa(tempAddress.sin_addr));
            printf("Domain server %s: %s\n", DOMAIN_HOSTNAME, DOMAIN_IP);
            
        } else {
            printf("Failed lookup domainserver\n");
        }
    } else printf("Using static domainserver IP: %s\n", DOMAIN_IP);
    
    
    while (!domainServerCheckinStopFlag) {
        gettimeofday(&lastSend, NULL);
        
        output[0] = parentAgentList->getOwnerType();
        packSocket(output + 1, localAddress, htons(parentAgentList->getSocketListenPort()));
        
        parentAgentList->getAgentSocket().send(DOMAIN_IP, DOMAINSERVER_PORT, output, 7);
        
        double usecToSleep = 1000000 - (usecTimestampNow() - usecTimestamp(&lastSend));
        
        if (usecToSleep > 0) {
            usleep(usecToSleep);
        }
    }
    
    pthread_exit(0);
    return NULL;
}

void AgentList::startDomainServerCheckInThread() {
    pthread_create(&checkInWithDomainServerThread, NULL, checkInWithDomainServer, (void *)this);
}

void AgentList::stopDomainServerCheckInThread() {
    domainServerCheckinStopFlag = true;
    pthread_join(checkInWithDomainServerThread, NULL);
}