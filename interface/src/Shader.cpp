#include "Shader.h"
#include <stdio.h>

CShader::CShader()
{
    programId = 0;
    isValid = false;
}
bool CShader::compile(std::string vertexSource, std::string fragmentSource)
{
    //create gl handles for program object and source, we only need to keep the programId
    programId = glCreateProgram();

    GLuint vertId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragId = glCreateShader(GL_FRAGMENT_SHADER);

    //compile source code
    const char* vertexSourceP = vertexSource.c_str();
    const char* fragmentSourceP = fragmentSource.c_str();
    glShaderSource(vertId, 1, &vertexSourceP, NULL);
    glShaderSource(fragId, 1, &fragmentSourceP, NULL);
    glCompileShader(vertId);
    glCompileShader(fragId);
    
    //attach the 2 source objects and attempt to link them to create shader program
    glAttachShader(programId, vertId);
    glAttachShader(programId, fragId);

    //bind vertex attributes
    glBindAttribLocation(programId, VERTEX_ATTRIB, "inVertex");
    glBindAttribLocation(programId, NORMAL_ATTRIB, "inNormal");
    glBindAttribLocation(programId, COLOR_ATTRIB, "inColor");

    glLinkProgram(programId);

    //check if we were successful
    GLint linkStatus;
    glGetProgramiv(programId, GL_LINK_STATUS, &linkStatus);
    if(!linkStatus)
    {
        //in this case the shader failed to build, get any log messages and print them out
        //to see where we may have gone wrong

        printf("Shader Failed to Compile\n");
        
        char infoLog[1024];
        GLsizei len;
        glGetShaderInfoLog(vertId, 1024, &len, infoLog);
        printf("%s\n",infoLog);

        glGetShaderInfoLog(fragId, 1024, &len, infoLog);
        printf("%s\n", infoLog);

        glGetProgramInfoLog(programId, 1024, &len, infoLog);
        printf("%s\n",infoLog);

        return false;
    }
    //if we have reached here we have a working shader program
    isValid = true;

    //find the uniform and attribute ids for the shader
    bindEntryPoints();

    glUseProgram(0);

    GLenum err = glGetError();
    if(err)
    {
        printf("GL reported a problem after shader init\n");
    }

    return true;
}
GLuint CShader::getUniformLocation(const char* name)
{
    GLuint id = glGetUniformLocation(programId, name);
    if(id == -1)
    {
        printf("Failed to find uniform '%s'' in shader\n", name);

        //might be better to treat the shader as broken, than
        //continue with minor errors
        isValid = false;

    }
    return id;
}

bool CShader::valid()
{
    return isValid;
}

void CShader::useShader()
{
    glUseProgram((GLuint)programId);
}
void CShader::cleanUp()
{
    glDisableVertexAttribArray(VERTEX_ATTRIB);
    glDisableVertexAttribArray(NORMAL_ATTRIB);
    glDisableVertexAttribArray(COLOR_ATTRIB);

    glUseProgram(0);
}

CVoxelShader::CVoxelShader() : CShader()
{

}

void CVoxelShader::bindEntryPoints()
{
    glUseProgram((GLuint)programId);


    //find uniform locations
    MVPMatrixId = getUniformLocation("MVPMatrix");
    DiffuseColorId = getUniformLocation("DiffuseColor");
    AmbientColorId = getUniformLocation("AmbientColor");
    SpecularColorId = getUniformLocation("SpecularColor");
    SpecularPowerId = getUniformLocation("SpecularPower");
    LightPosId = getUniformLocation("LightPos");
    EyePosId = getUniformLocation("EyePos");

    GLenum err = glGetError();
    if(err)
    {
        printf("GL reported a problem after shader binding\n");
    }
}

void CVoxelShader::setMVPMatrix(glm::mat4x4 m)
{
    glUniformMatrix4fv(MVPMatrixId, 1, false,glm::value_ptr(m));
}
void CVoxelShader::setLightPos(glm::vec3 c)
{
    glUniform3fv(LightPosId, 1, glm::value_ptr(c));
}
void CVoxelShader::setDiffuseColor(glm::vec3 c)
{
    glUniform3fv(DiffuseColorId, 1, glm::value_ptr(c));
}
void CVoxelShader::setAmbientColor(glm::vec3 c)
{
    glUniform3fv(AmbientColorId, 1, glm::value_ptr(c));
}
void CVoxelShader::setSpecularColor(glm::vec3 c)
{
    glUniform3fv(SpecularColorId, 1, glm::value_ptr(c));
}
void CVoxelShader::setSpecularPower(float c)
{
    glUniform1f(SpecularPowerId, c);
}
void CVoxelShader::setEyePos(glm::vec3 c)
{
    glUniform3fv(EyePosId, 1, glm::value_ptr(c));
}


