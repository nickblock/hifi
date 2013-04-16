//
//  
//  Interface
//   
//  Show a field of objects rendered in 3D, with yaw and pitch of scene driven 
//  by accelerometer data
//  serial port connected to Maple board/arduino. 
//
//  Keyboard Commands: 
//
//  / = toggle stats display
//  spacebar = reset gyros/head
//  h = render Head
//  l = show incoming gyro levels
//

#include "InterfaceConfig.h"
#include <iostream>
#include <fstream>
#include <math.h>
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include "Syssocket.h"
#include "Systime.h"
#else
#include <sys/time.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

#include <pthread.h>

#include <glm/glm.hpp>
#include "Field.h"
#include "world.h"
#include "Util.h"
#ifndef _WIN32
#include "Audio.h"
#endif
#include "Head.h"
#include "Hand.h"
#include "Particle.h"
#include "Texture.h"
#include "Cloud.h"
#include <AgentList.h>
#include "VoxelSystem.h"
#include "Lattice.h"
#include "Finger.h"
#include "Oscilloscope.h"
#include "UDPSocket.h"
#include "SerialInterface.h"
#include <SharedUtil.h>
#include "Shader.h"

using namespace std;

int audio_on = 1;                   //  Whether to turn on the audio support
int simulate_on = 1; 

AgentList agentList('I');
pthread_t networkReceiveThread;
bool stopNetworkReceiveThread = false;

//  For testing, add milliseconds of delay for received UDP packets
int packetcount = 0;
int packets_per_second = 0; 
int bytes_per_second = 0;
int bytescount = 0;

//  Getting a target location from other machine (or loopback) to display
int target_x, target_y; 
int target_display = 0;

int head_mirror = 1;                  //  Whether to mirror own head when viewing it
int sendToSelf = 1;

int WIDTH = 1200; 
int HEIGHT = 800; 
int fullscreen = 0;

bool wantColorRandomizer = true; // for addSphere and load file

Oscilloscope audioScope(256,200,true);

#define HAND_RADIUS 0.25            //  Radius of in-world 'hand' of you
Head myHead;                        //  The rendered head of oneself

glm::vec3 box(WORLD_SIZE,WORLD_SIZE,WORLD_SIZE);
ParticleSystem balls(0,
                     box, 
                     false,                     //  Wrap?
                     0.02f,                      //  Noise
                     0.3f,                       //  Size scale 
                     0.0                        //  Gravity
                     );

Cloud cloud(20000,                         //  Particles
            box,                           //  Bounding Box
            false                          //  Wrap
            );

VoxelSystem voxels;

Lattice lattice(160,100);
Finger myFinger(WIDTH, HEIGHT);
Field field;

#ifndef _WIN32
Audio audio(&audioScope, &myHead);
#else
#define NO_AUDIO
#endif

#define RENDER_FRAME_MSECS 8
int steps_per_frame = 0;

float yaw = 0.f;                         //  The yaw, pitch for the avatar head
float pitch = 0.f;                            
float start_yaw = 122;
float renderPitch = 0.f;
float renderYawRate = 0.f;
float renderPitchRate = 0.f; 

//  Where one's own agent begins in the world (needs to become a dynamic thing passed to the program)
glm::vec3 start_location(6.1f, 0, 1.4f);

int stats_on = 0;					//  Whether to show onscreen text overlay with stats

int noise_on = 0;					//  Whether to add random noise 
float noise = 1.0;                  //  Overall magnitude scaling for random noise levels 

int step_on = 0;                    
int display_levels = 0;
int display_head = 0;
int display_field = 0;

int display_head_mouse = 1;         //  Display sample mouse pointer controlled by head movement
int head_mouse_x, head_mouse_y; 
int head_lean_x, head_lean_y;

int mouse_x, mouse_y;				//  Where is the mouse
int mouse_start_x, mouse_start_y;   //  Mouse location at start of last down click
int mouse_pressed = 0;				//  true if mouse has been pressed (clear when finished)

int nearbyAgents = 0;               //  How many other people near you is the domain server reporting?

int speed;

//
//  Serial USB Variables
// 

SerialInterface serialPort;
int latency_display = 1;

glm::vec3 gravity;
int first_measurement = 1;
//int samplecount = 0;

//  Frame rate Measurement

int framecount = 0;                  
float FPS = 120.f;
timeval timer_start, timer_end;
timeval last_frame;
double elapsedTime;

// Particles

char texture_filename[] = "images/int-texture256-v4.png";
unsigned int texture_width = 256;
unsigned int texture_height = 256;


CVoxelShader voxelShader;

float particle_attenuation_quadratic[] =  { 0.0f, 0.0f, 2.0f }; // larger Z = smaller particles
float pointer_attenuation_quadratic[] =  { 1.0f, 0.0f, 0.0f }; // for 2D view

#ifdef MARKER_CAPTURE

    /***  Marker Capture ***/
    #define MARKER_CAPTURE_INTERVAL 1
    MarkerCapture marker_capturer(CV_CAP_ANY); // Create a new marker capturer, attached to any valid camera.
    MarkerAcquisitionView marker_acq_view(&marker_capturer);
    bool marker_capture_enabled = true;
    bool marker_capture_display = true;
    IplImage* marker_capture_frame;
    IplImage* marker_capture_blob_frame;
    pthread_mutex_t frame_lock;

#endif


//  Every second, check the frame rates and other stuff
void Timer(int extra)
{
    gettimeofday(&timer_end, NULL);
    FPS = (float)framecount / ((float)diffclock(&timer_start, &timer_end) / 1000.f);
    packets_per_second = (float)packetcount / ((float)diffclock(&timer_start, &timer_end) / 1000.f);
    bytes_per_second = (float)bytescount / ((float)diffclock(&timer_start, &timer_end) / 1000.f);
   	framecount = 0;
    packetcount = 0;
    bytescount = 0;
    
	glutTimerFunc(1000,Timer,0);
    gettimeofday(&timer_start, NULL);
    
    //  Ping the agents we can see
    agentList.pingAgents();

    if (0) {
        //  Massive send packet speed test
        timeval starttest, endtest;
        gettimeofday(&starttest, NULL);
        char junk[1000];
        junk[0] = 'J';
        for (int i = 0; i < 10000; i++)
        {
//            agentSocket.send((char *)"192.168.1.38", AGENT_UDP_PORT, junk, 1000);
        }
        gettimeofday(&endtest, NULL);
        float sendTime = static_cast<float>( diffclock(&starttest, &endtest) );
        printf("packet test = %4.1f\n", sendTime);
    }
    
    // if we haven't detected gyros, check for them now
    if (!serialPort.active) {
        serialPort.pair();
    }
}

void display_stats(void)
{
	//  bitmap chars are about 10 pels high 
    char legend[] = "/ - toggle this display, Q - exit, H - show head, M - show hand, T - test audio";
    drawtext(10, 15, 0.10f, 0, 1.0, 0, legend);
    
    char stats[200];
    sprintf(stats, "FPS = %3.0f  Pkts/s = %d  Bytes/s = %d ", 
            FPS, packets_per_second,  bytes_per_second);
    drawtext(10, 30, 0.10f, 0, 1.0, 0, stats); 
    if (serialPort.active) {
        sprintf(stats, "ADC samples = %d, LED = %d", 
                serialPort.getNumSamples(), serialPort.getLED());
        drawtext(300, 30, 0.10f, 0, 1.0, 0, stats);
    }
    
    //  Output the ping times to the various agents 
//    std::stringstream pingTimes;
//    pingTimes << "Agent Pings, msecs:";
//    for (int i = 0; i < getAgentCount(); i++) {
//        pingTimes << " " << getAgentAddress(i) << ": " << getAgentPing(i);
//    }
//    drawtext(10,50,0.10, 0, 1.0, 0, (char *)pingTimes.str().c_str());

    std::stringstream voxelStats;
    voxelStats << "Voxels Rendered: " << voxels.getVoxelsRendered();
    drawtext(10,70,0.10f, 0, 1.0, 0, (char *)voxelStats.str().c_str());

    
    /*
    std::stringstream angles;
    angles << "render_yaw: " << myHead.getRenderYaw() << ", Yaw: " << myHead.getYaw();
    drawtext(10,50,0.10, 0, 1.0, 0, (char *)angles.str().c_str());
    */
    
    /*
    char adc[200];
	sprintf(adc, "location = %3.1f,%3.1f,%3.1f, angle_to(origin) = %3.1f, head yaw = %3.1f, render_yaw = %3.1f",
            -location[0], -location[1], -location[2],
            angle_to(myHead.getPos()*-1.f, glm::vec3(0,0,0), myHead.getRenderYaw(), myHead.getYaw()),
            myHead.getYaw(), myHead.getRenderYaw());
    drawtext(10, 50, 0.10, 0, 1.0, 0, adc);
     */
    
}

void initDisplay(void)
{
    //  Set up blending function so that we can NOT clear the display
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel (GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    
//    load_png_as_texture(texture_filename);

    if (fullscreen) glutFullScreen();
}

//open shader source files, read in contents and attempt to
//compile a shader program from them
void createShader()
{
    string vertexShaderFile = "shaders/vertex.vs";
    string fragmentShaderFile = "shaders/fragment.fs";

    string vertexSourceCode, fragmentSourceCode;

    ifstream file;
    file.open(vertexShaderFile.c_str(), ios_base::in);
    if(file.is_open())
    {
        stringstream buffer;
        buffer << file.rdbuf();
        vertexSourceCode = buffer.str();

        file.close();
    }
    else
    {
        printf("Couldn't find shader source '%s'\n", vertexShaderFile.c_str());
        return;
    }

    file.open(fragmentShaderFile.c_str(), ios_base::in);
    if(file.is_open())
    {
        stringstream buffer;
        buffer << file.rdbuf();
        fragmentSourceCode = buffer.str();

        file.close();
    }
    else
    {
        printf("Couldn't find shader source '%s'\n", fragmentShaderFile.c_str());
        return;
    }

    //if we have got this far we should have our shader source code

    //try to compile it, if it fails we should get some log print out
    voxelShader.compile(vertexSourceCode, fragmentSourceCode);
}

void init(void)
{
    voxels.init();
    voxels.setViewerHead(&myHead);
    myHead.setRenderYaw(start_yaw);

    head_mouse_x = WIDTH/2;
    head_mouse_y = HEIGHT/2; 
    head_lean_x = WIDTH/2;
    head_lean_y = HEIGHT/2;
    
    //  Initialize Field values
    field = Field();
    printf( "Field Initialized.\n" );

    if (noise_on) {   
        myHead.setNoise(noise);
    }
    myHead.setPos(start_location);
    
#ifdef MARKER_CAPTURE
    if(marker_capture_enabled){
        marker_capturer.position_updated(&position_updated);
        marker_capturer.frame_updated(&marker_frame_available);
        if(!marker_capturer.init_capture()){
            printf("Camera-based marker capture initialized.\n");
        }else{
            printf("Error initializing camera-based marker capture.\n");
        }
    }
#endif
    
    
    gettimeofday(&timer_start, NULL);
    gettimeofday(&last_frame, NULL);

    createShader();
}

void terminate () {
    // Close serial port
    //close(serial_fd);

    #ifndef NO_AUDIO
    audio.terminate();
    #endif
    stopNetworkReceiveThread = true;
    pthread_join(networkReceiveThread, NULL);
    
    exit(EXIT_SUCCESS);
}

void reset_sensors()
{
    //  
    //   Reset serial I/O sensors 
    // 
    myHead.setRenderYaw(start_yaw);
    
    yaw = renderYawRate = 0; 
    pitch = renderPitch = renderPitchRate = 0;
    myHead.setPos(start_location);
    head_mouse_x = WIDTH/2;
    head_mouse_y = HEIGHT/2;
    head_lean_x = WIDTH/2;
    head_lean_y = HEIGHT/2; 
    
    myHead.reset();
    
    if (serialPort.active) {
        serialPort.resetTrailingAverages();
    }
}

void simulateHand(float deltaTime) {
    //  If mouse is being dragged, send current force to the hand controller
    if (mouse_pressed == 1)
    {
        //  Add a velocity to the hand corresponding to the detected size of the drag vector
        const float MOUSE_HAND_FORCE = 1.5;
        float dx = mouse_x - mouse_start_x;
        float dy = mouse_y - mouse_start_y;
        glm::vec3 vel(dx*MOUSE_HAND_FORCE, -dy*MOUSE_HAND_FORCE*(WIDTH/HEIGHT), 0);
        myHead.hand->addVelocity(vel*deltaTime);
    }
}

void simulateHead(float frametime)
//  Using serial data, update avatar/render position and angles
{
//    float measured_pitch_rate = serialPort.getRelativeValue(PITCH_RATE);
//    float measured_yaw_rate = serialPort.getRelativeValue(YAW_RATE);
    
    float measured_pitch_rate = 0;
    float measured_yaw_rate = 0;
    
    //float measured_lateral_accel = serialPort.getRelativeValue(ACCEL_X);
    //float measured_fwd_accel = serialPort.getRelativeValue(ACCEL_Z);
    
    myHead.UpdatePos(frametime, &serialPort, head_mirror, &gravity);
    
    //  Update head_mouse model 
    const float MIN_MOUSE_RATE = 30.0;
    const float MOUSE_SENSITIVITY = 0.1f;
    if (powf(measured_yaw_rate*measured_yaw_rate + 
             measured_pitch_rate*measured_pitch_rate, 0.5) > MIN_MOUSE_RATE)
    {
        head_mouse_x += measured_yaw_rate*MOUSE_SENSITIVITY;
        head_mouse_y += measured_pitch_rate*MOUSE_SENSITIVITY*(float)HEIGHT/(float)WIDTH; 
    }
    head_mouse_x = max(head_mouse_x, 0);
    head_mouse_x = min(head_mouse_x, WIDTH);
    head_mouse_y = max(head_mouse_y, 0);
    head_mouse_y = min(head_mouse_y, HEIGHT);
    
    //  Update render direction (pitch/yaw) based on measured gyro rates
    const int MIN_YAW_RATE = 100;
    const int MIN_PITCH_RATE = 100;
    
    const float YAW_SENSITIVITY = 0.02;
    const float PITCH_SENSITIVITY = 0.05;
    
    //  Update render pitch and yaw rates based on keyPositions
    const float KEY_YAW_SENSITIVITY = 2.0;
    if (myHead.getDriveKeys(ROT_LEFT)) renderYawRate -= KEY_YAW_SENSITIVITY*frametime;
    if (myHead.getDriveKeys(ROT_RIGHT)) renderYawRate += KEY_YAW_SENSITIVITY*frametime;
    
    if (fabs(measured_yaw_rate) > MIN_YAW_RATE)  
    {   
        if (measured_yaw_rate > 0)
            renderYawRate += (measured_yaw_rate - MIN_YAW_RATE) * YAW_SENSITIVITY * frametime;
        else 
            renderYawRate += (measured_yaw_rate + MIN_YAW_RATE) * YAW_SENSITIVITY * frametime;
    }
    if (fabs(measured_pitch_rate) > MIN_PITCH_RATE) 
    {
        if (measured_pitch_rate > 0)
            renderPitchRate += (measured_pitch_rate - MIN_PITCH_RATE) * PITCH_SENSITIVITY * frametime;
        else 
            renderPitchRate += (measured_pitch_rate + MIN_PITCH_RATE) * PITCH_SENSITIVITY * frametime;
    }
         
    renderPitch += renderPitchRate;
    
    // Decay renderPitch toward zero because we never look constantly up/down 
    renderPitch *= (1.f - 2.0*frametime);

    //  Decay angular rates toward zero 
    renderPitchRate *= (1.f - 5.0*frametime);
    renderYawRate *= (1.f - 7.0*frametime);
    
    //  Update own head data
    myHead.setRenderYaw(myHead.getRenderYaw() + renderYawRate);
    myHead.setRenderPitch(renderPitch);
    
    //  Get audio loudness data from audio input device
    float loudness, averageLoudness;
    #ifndef NO_AUDIO
    audio.getInputLoudness(&loudness, &averageLoudness);
    myHead.setLoudness(loudness);
    myHead.setAverageLoudness(averageLoudness);
    #endif

    //  Send my streaming head data to agents that are nearby and need to see it!
    const int MAX_BROADCAST_STRING = 200;
    char broadcast_string[MAX_BROADCAST_STRING];
    int broadcast_bytes = myHead.getBroadcastData(broadcast_string);
    agentList.broadcastToAgents(broadcast_string, broadcast_bytes);
}

int render_test_spot = WIDTH/2;
int render_test_direction = 1; 

void display(void)
{

    glEnable (GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_LIGHTING);
    glEnable(GL_LINE_SMOOTH);
    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();  {
        glLoadIdentity();

        //  Setup 3D lights
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        
        GLfloat light_position0[] = { 0.0, 0.0, 0.0, 1.0 };
        glLightfv(GL_LIGHT0, GL_POSITION, light_position0);
        GLfloat ambient_color[] = { 0.125, 0.305, 0.5 };
        glLightfv(GL_LIGHT0, GL_AMBIENT, ambient_color);
        GLfloat diffuse_color[] = { 0.5, 0.42, 0.33 };
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse_color);
        GLfloat specular_color[] = { 0.0, 1.0, 0.0, 1.0};
        glLightfv(GL_LIGHT0, GL_SPECULAR, specular_color);
        
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular_color);
        glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 96);
           
        //  Rotate, translate to camera location
        glRotatef(myHead.getRenderPitch(), 1, 0, 0);
        glRotatef(myHead.getRenderYaw(), 0, 1, 0);
        glTranslatef(myHead.getPos().x, myHead.getPos().y, myHead.getPos().z);

        //if we have the voxel shader working, use it
        if(voxelShader.valid())
        {
            //create the projection matrix, this should be based on the screen ratio
            glm::mat4 projectionMatrix = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.f);

            //the view matrix is a product of the 'head' camera
            glm::mat4 viewMatrix;
            viewMatrix = glm::rotate(glm::mat4(1.0), myHead.getRenderPitch(), glm::vec3(1, 0, 0));
            viewMatrix = glm::rotate(viewMatrix, myHead.getRenderYaw(), glm::vec3(0, 1, 0));
            viewMatrix = glm::translate(viewMatrix, myHead.getPos());

            //the model matrix for the voxels
            glm::mat4 modelMatrix = glm::scale(glm::vec3(10, 10, 10));
            glm::mat4 inverseModelMatrix = glm::inverse(modelMatrix);

            //the resultant MVP matrix required by the shader
            glm::mat4 MVPMatrix = projectionMatrix * viewMatrix * modelMatrix;

            //put the light and eye positions in model space
            glm::vec4 LightPos(light_position0[0], light_position0[1], light_position0[2], 1.0);
      //      LightPos = LightPos * rotate;
            glm::vec3 LightPosModel( inverseModelMatrix * LightPos);
            glm::vec3 eyePos = myHead.getPos();
            glm::vec3 EyePosModel(inverseModelMatrix  * glm::vec4(eyePos.x, eyePos.y, eyePos.z, 1.0));

            voxelShader.useShader();

            //set shader state
            voxelShader.setMVPMatrix(MVPMatrix);
            voxelShader.setLightPos(LightPosModel);
            voxelShader.setDiffuseColor(glm::vec3(diffuse_color[0], diffuse_color[1], diffuse_color[2]));
            voxelShader.setAmbientColor(glm::vec3(ambient_color[0], ambient_color[1], ambient_color[2]));
            voxelShader.setSpecularColor(glm::vec3(specular_color[0], specular_color[1], specular_color[2]));
            voxelShader.setSpecularPower(96);
            voxelShader.setEyePos(EyePosModel);

            //  Draw voxels
            voxels.render();

            voxelShader.cleanUp();
        }

        glColor3f(1,0.0,0.0);
        glutSolidSphere(0.25, 15, 15);

        //  Draw cloud of dots
        //glDisable( GL_POINT_SPRITE_ARB );
        //glDisable( GL_TEXTURE_2D );
//        if (!display_head) cloud.render();

    
        //  Draw field vectors
        if (display_field) field.render();
            
        //  Render heads of other agents
        for(std::vector<Agent>::iterator agent = agentList.getAgents().begin(); agent != agentList.getAgents().end(); agent++) {
            if (agent->getLinkedData() != NULL) {
                Head *agentHead = (Head *)agent->getLinkedData();
                glPushMatrix();
                glm::vec3 pos = agentHead->getPos();
                glTranslatef(-pos.x, -pos.y, -pos.z);
                agentHead->render(0, 0);
                glPopMatrix();
            }
        }



        if (!display_head) balls.render();
    
        //  Render the world box
        if (!display_head && stats_on) render_world_box();
    
        //  Render my own head
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(0.f, 0.f, -7.f);
        myHead.render(display_head, 1);
        glPopMatrix();






    }
    
    glPopMatrix();

    //  Render 2D overlay:  I/O level bar graphs and text  
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity(); 
        gluOrtho2D(0, WIDTH, HEIGHT, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);

        //lattice.render(WIDTH, HEIGHT);
        //myFinger.render();
        #ifndef NO_AUDIO
        audio.render(WIDTH, HEIGHT);
        if (audioScope.getState()) audioScope.render();
        #endif


        //drawvec3(100, 100, 0.15, 0, 1.0, 0, myHead.getPos(), 0, 1, 0);
        glPointParameterfvARB( GL_POINT_DISTANCE_ATTENUATION_ARB, pointer_attenuation_quadratic );

        if (mouse_pressed == 1)
        {
            glPointSize( 10.0f );
            glColor3f(1,1,1);
            //glEnable(GL_POINT_SMOOTH);
            glBegin(GL_POINTS);
            glVertex2f(target_x, target_y);
            glEnd();
            char val[20];
            sprintf(val, "%d,%d", target_x, target_y); 
            drawtext(target_x, target_y-20, 0.08, 0, 1.0, 0, val, 0, 1, 0);
        }
        if (display_head_mouse && !display_head && stats_on)
        {
            glPointSize(10.0f);
            glColor4f(1.0, 1.0, 0.0, 0.8);
            glEnable(GL_POINT_SMOOTH);
            glBegin(GL_POINTS);
            glVertex2f(head_mouse_x, head_mouse_y);
            glEnd();
        }
        //  Spot bouncing back and forth on bottom of screen
        if (0)
        {
            glPointSize(50.0f);
            glColor4f(1.0, 1.0, 1.0, 1.0);
            glEnable(GL_POINT_SMOOTH);
            glBegin(GL_POINTS);
            glVertex2f(render_test_spot, HEIGHT-100);
            glEnd(); 
            render_test_spot += render_test_direction*50; 
            if ((render_test_spot > WIDTH-100) || (render_test_spot < 100)) render_test_direction *= -1.0;
            
        }

        
    //  Show detected levels from the serial I/O ADC channel sensors
    if (display_levels) serialPort.renderLevels(WIDTH,HEIGHT);
    
    //  Display miscellaneous text stats onscreen
    if (stats_on) display_stats();
    
    //  Draw number of nearby people always
    char agents[100];
    sprintf(agents, "Agents nearby: %ld\n", agentList.getAgents().size());
    drawtext(WIDTH-200,20, 0.10, 0, 1.0, 0, agents, 1, 1, 0);
    
    glPopMatrix();


    glutSwapBuffers();
    framecount++;
}

void testPointToVoxel()
{
	float y=0;
	float z=0;
	float s=0.1;
	for (float x=0; x<=1; x+= 0.05)
	{
		std::cout << " x=" << x << " ";

		unsigned char red   = 200; //randomColorValue(65);
		unsigned char green = 200; //randomColorValue(65);
		unsigned char blue  = 200; //randomColorValue(65);
	
		unsigned char* voxelCode = pointToVoxel(x, y, z, s,red,green,blue);
		printVoxelCode(voxelCode);
		delete voxelCode;
		std::cout << std::endl;
	}
}

void addRandomSphere(bool wantColorRandomizer)
{
	float r = randFloatInRange(0.05,0.1);
	float xc = randFloatInRange(r,(1-r));
	float yc = randFloatInRange(r,(1-r));
	float zc = randFloatInRange(r,(1-r));
	float s = 0.001; // size of voxels to make up surface of sphere
	bool solid = false;

	printf("random sphere\n");
	printf("radius=%f\n",r);
	printf("xc=%f\n",xc);
	printf("yc=%f\n",yc);
	printf("zc=%f\n",zc);

	voxels.createSphere(r,xc,yc,zc,s,solid,wantColorRandomizer);
}


const float KEYBOARD_YAW_RATE = 0.8;
const float KEYBOARD_STRAFE_RATE = 0.03;
const float KEYBOARD_FLY_RATE = 0.08;

void specialkeyUp(int k, int x, int y) {
    if (k == GLUT_KEY_UP) {
        myHead.setDriveKeys(FWD, 0);
        myHead.setDriveKeys(UP, 0);
    }
    if (k == GLUT_KEY_DOWN) {
        myHead.setDriveKeys(BACK, 0);
        myHead.setDriveKeys(DOWN, 0);
    }
    if (k == GLUT_KEY_LEFT) {
        myHead.setDriveKeys(LEFT, 0);
        myHead.setDriveKeys(ROT_LEFT, 0);
    }
    if (k == GLUT_KEY_RIGHT) {
        myHead.setDriveKeys(RIGHT, 0);
        myHead.setDriveKeys(ROT_RIGHT, 0);
    }
    
}

void specialkey(int k, int x, int y)
{
    if (k == GLUT_KEY_UP || k == GLUT_KEY_DOWN || k == GLUT_KEY_LEFT || k == GLUT_KEY_RIGHT) {
        if (k == GLUT_KEY_UP) {
            if (glutGetModifiers() == GLUT_ACTIVE_SHIFT) myHead.setDriveKeys(UP, 1);
            else myHead.setDriveKeys(FWD, 1);
        }
        if (k == GLUT_KEY_DOWN) {
            if (glutGetModifiers() == GLUT_ACTIVE_SHIFT) myHead.setDriveKeys(DOWN, 1);
            else myHead.setDriveKeys(BACK, 1);
        }
        if (k == GLUT_KEY_LEFT) {
            if (glutGetModifiers() == GLUT_ACTIVE_SHIFT) myHead.setDriveKeys(LEFT, 1);
            else myHead.setDriveKeys(ROT_LEFT, 1);  
        }
        if (k == GLUT_KEY_RIGHT) {
            if (glutGetModifiers() == GLUT_ACTIVE_SHIFT) myHead.setDriveKeys(RIGHT, 1);
            else myHead.setDriveKeys(ROT_RIGHT, 1);   
        }
        #ifndef NO_AUDIO
        audio.setWalkingState(true);
        #endif
    }    
}


void keyUp(unsigned char k, int x, int y) {
    if (k == 'e') myHead.setDriveKeys(UP, 0);
    if (k == 'c') myHead.setDriveKeys(DOWN, 0);
    if (k == 'w') myHead.setDriveKeys(FWD, 0);
    if (k == 's') myHead.setDriveKeys(BACK, 0);
    if (k == 'a') myHead.setDriveKeys(ROT_LEFT, 0);
    if (k == 'd') myHead.setDriveKeys(ROT_RIGHT, 0);

}

void key(unsigned char k, int x, int y)
{
    
	//  Process keypresses 
 	if (k == 'q')  ::terminate();
    
    if (k == '/')  stats_on = !stats_on;		// toggle stats
	if (k == 'n') 
    {
        noise_on = !noise_on;                   // Toggle noise 
        if (noise_on)
        {
            myHead.setNoise(noise);
        }
        else 
        {
            myHead.setNoise(0);
        }

    }
    
    if (k == 'h') {
        display_head = !display_head;
        #ifndef NO_AUDIO
        audio.setMixerLoopbackFlag(display_head);
        #endif
    }
    
    if (k == 'm') head_mirror = !head_mirror;
    
    if (k == 'f') display_field = !display_field;
    if (k == 'l') display_levels = !display_levels;
    if (k == 'e') myHead.setDriveKeys(UP, 1);
    if (k == 'c') myHead.setDriveKeys(DOWN, 1);
    if (k == 'w') myHead.setDriveKeys(FWD, 1);
    if (k == 's') myHead.setDriveKeys(BACK, 1);
    if (k == ' ') reset_sensors();
    if (k == 'a') myHead.setDriveKeys(ROT_LEFT, 1); 
    if (k == 'd') myHead.setDriveKeys(ROT_RIGHT, 1);
    if (k == 'o') simulate_on = !simulate_on;
    if (k == 'p') 
    {
        // Add to field vector 
        float pos[] = {5,5,5};
        float add[] = {0.001, 0.001, 0.001};
        field.add(add, pos);
    }
    if (k == '1')
    {
        myHead.SetNewHeadTarget((randFloat()-0.5)*20.0, (randFloat()-0.5)*20.0);
    }

	// press the . key to get a new random sphere of voxels added 
    if (k == '.')
    {
        addRandomSphere(wantColorRandomizer);
        //testPointToVoxel();
    }
}

//
//  Receive packets from other agents/servers and decide what to do with them!
//
void *networkReceive(void *args)
{    
    sockaddr senderAddress;
    ssize_t bytesReceived;
    char *incomingPacket = new char[MAX_PACKET_SIZE];

    while (!stopNetworkReceiveThread) {
        if (agentList.getAgentSocket().receive(&senderAddress, incomingPacket, &bytesReceived)) {
            packetcount++;
            bytescount += bytesReceived;
            
            if (incomingPacket[0] == 't') {
                //  Pass everything but transmitter data to the agent list
                 myHead.hand->processTransmitterData(incomingPacket, bytesReceived);            
            } else if (incomingPacket[0] == 'V') {
                voxels.parseData(incomingPacket, bytesReceived);
            } else {
               agentList.processAgentData(&senderAddress, incomingPacket, bytesReceived);
            }
        }
    }
    
    pthread_exit(0); 
    return NULL;
}

void idle(void)
{
    timeval check;
    gettimeofday(&check, NULL);
    
    //  Check and render display frame 
    if (diffclock(&last_frame, &check) > RENDER_FRAME_MSECS)
    {
        steps_per_frame++;
        //  Simulation
        simulateHead(1.f/FPS);
        simulateHand(1.f/FPS);
        
        if (simulate_on) {
            field.simulate(1.f/FPS);
            myHead.simulate(1.f/FPS);
            balls.simulate(1.f/FPS);
            cloud.simulate(1.f/FPS);
            lattice.simulate(1.f/FPS);
            myFinger.simulate(1.f/FPS);
        }

        if (!step_on) glutPostRedisplay();
        last_frame = check;
        
    }
    
    //  Read serial data 
    if (serialPort.active) {
        serialPort.readData();
    }
}

void reshape(int width, int height)
{
    WIDTH = width;
    HEIGHT = height; 
    
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION); //hello
    glLoadIdentity();
    gluPerspective(45, //view angle
                   1.0, //aspect ratio
                   0.1, //near clip
                   500.0);//far clip
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

     
}

void mouseFunc( int button, int state, int x, int y ) 
{
    if( button == GLUT_LEFT_BUTTON && state == GLUT_DOWN )
    {
		mouse_x = x;
		mouse_y = y;
		mouse_pressed = 1;
        lattice.mouseClick((float)x/(float)WIDTH,(float)y/(float)HEIGHT);
        mouse_start_x = x;
        mouse_start_y = y;
    }
	if( button == GLUT_LEFT_BUTTON && state == GLUT_UP )
    {
		mouse_x = x;
		mouse_y = y;
		mouse_pressed = 0;
    }
	
}

void motionFunc( int x, int y)
{
	mouse_x = x;
	mouse_y = y;
    
    lattice.mouseClick((float)x/(float)WIDTH,(float)y/(float)HEIGHT);
}

void mouseoverFunc( int x, int y)
{
	mouse_x = x;
	mouse_y = y;
    if (mouse_pressed == 0)
    {
//        lattice.mouseOver((float)x/(float)WIDTH,(float)y/(float)HEIGHT);
//        myFinger.setTarget(mouse_x, mouse_y);
    }
}

void attachNewHeadToAgent(Agent *newAgent) {
    if (newAgent->getLinkedData() == NULL) {
        newAgent->setLinkedData(new Head());
    }
}

#ifndef NO_AUDIO
void audioMixerUpdate(in_addr_t newMixerAddress, in_port_t newMixerPort) {
    audio.updateMixerParams(newMixerAddress, newMixerPort);
}
#endif

int main(int argc, const char * argv[])
{
    const char* domainIP = getCmdOption(argc, argv, "--domain");
    if (domainIP) {
		strcpy(DOMAIN_IP,domainIP);
	}

    // Handle Local Domain testing with the --local command line
    if (cmdOptionExists(argc, argv, "--local")) {
    	printf("Local Domain MODE!\n");
		int ip = getLocalAddress();
		sprintf(DOMAIN_IP,"%d.%d.%d.%d", (ip & 0xFF), ((ip >> 8) & 0xFF),((ip >> 16) & 0xFF), ((ip >> 24) & 0xFF));
    }

    // the callback for our instance of AgentList is attachNewHeadToAgent
    agentList.linkedDataCreateCallback = &attachNewHeadToAgent;
    
    #ifndef NO_AUDIO
    agentList.audioMixerSocketUpdate = &audioMixerUpdate;
    #endif

    // start the thread which checks for silent agents
    agentList.startSilentAgentRemovalThread();
    agentList.startDomainServerCheckInThread();
    
#ifdef _WIN32
    WSADATA WsaData;
    int wsaresult = WSAStartup( MAKEWORD(2,2), &WsaData );
#endif

    glutInit(&argc, (char**)argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Interface");
    
    #ifdef _WIN32
    glewInit();
    #endif

    printf( "Created Display Window.\n" );
    
    initDisplay();
    
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
	glutKeyboardFunc(key);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialkey);
    glutSpecialUpFunc(specialkeyUp);
	glutMotionFunc(motionFunc);
    glutPassiveMotionFunc(mouseoverFunc);
	glutMouseFunc(mouseFunc);
    glutIdleFunc(idle);
	
    printf( "Initialized Display.\n" );

    init();

	// Check to see if the user passed in a command line option for randomizing colors
	if (cmdOptionExists(argc, argv, "--NoColorRandomizer")) {
		wantColorRandomizer = false;
	}
	
	// Check to see if the user passed in a command line option for loading a local
	// Voxel File. If so, load it now.
    const char* voxelsFilename = getCmdOption(argc, argv, "-i");
    if (voxelsFilename) {
	    voxels.loadVoxelsFile(voxelsFilename,wantColorRandomizer);
	}
    
    // create thread for receipt of data via UDP
    pthread_create(&networkReceiveThread, NULL, networkReceive, NULL);
    
    printf( "Init() complete.\n" );
    
    glutTimerFunc(1000, Timer, 0);
    glutMainLoop();

    ::terminate();
    return EXIT_SUCCESS;
}   

