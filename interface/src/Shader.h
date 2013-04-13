#ifndef SHADER_H
#define SHADER_H

#include "InterfaceConfig.h"

#include <string>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

//use these defines to pass the various vertex attributes to the shader
//with a call glVertexAttribPointer
#define VERTEX_ATTRIB 0
#define NORMAL_ATTRIB 1
#define COLOR_ATTRIB 2

//the base class shader program
//you are probably going to need a number of differing versions of the shader program
class CShader
{
public:
    CShader();

    //to create the shader program pass 2 separate strings for the vertex shader source code and fragment source
    bool compile(std::string vertexSource, std::string fragmentSource);

    //check whether the shader was successfully compiled and linked
    bool valid();

    //this needs to be called prior to any rendering in order to use the shader
    void useShader();

    void cleanUp();

protected:

    GLuint  getUniformLocation(const char*);
    // the bindentrypoints function is usedc to to find the id points for the
    // shader uniforms and vertex attributes. This will vary between differing types of shader program
    virtual void    bindEntryPoints() = 0;

    bool    isValid;
    GLuint programId;

};

//this is the particualr shader we are going to use for voxel rendering
//it is going to need to take normal and color attributes and a number of variables for lighting
class CVoxelShader : public CShader
{
public:
    CVoxelShader();

    //set the model view projection matrix
    void    setMVPMatrix(glm::mat4x4 m);
    void    setLightPos(glm::vec3 p);
    void    setDiffuseColor(glm::vec3 c);
    void    setAmbientColor(glm::vec3 c);
    void    setSpecularColor(glm::vec3 c);
    void    setSpecularPower(float p);
    void    setEyePos(glm::vec3 e);

protected:

    virtual void    bindEntryPoints();

    //the ids corresponding to uniform variables found in the shader source
    GLuint MVPMatrixId;
    GLuint LightPosId;
    GLuint DiffuseColorId;
    GLuint AmbientColorId;
    GLuint SpecularColorId;
    GLuint SpecularPowerId;
    GLuint EyePosId;
};

#endif // SHADER_H
