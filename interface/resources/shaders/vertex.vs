
attribute vec3  inVertex;		
attribute vec3  inNormal;	
attribute vec3  inColor;

uniform vec3 DiffuseColor;
uniform vec3 SpecularColor;
uniform float SpecularPower;
uniform vec3 AmbientColor;
uniform vec3 LightPos;
uniform vec3 EyePos;
uniform mat4 MVPMatrix;

varying vec4 Color;

void main()
{

    vec3 lightDir = normalize(inVertex - LightPos);
    vec3 eyeDir = normalize(inVertex - EyePos);
    
    //diffuse lighting
    float NdotL = max(dot(inNormal, lightDir), 0.0);
    vec3 diffuseComponent = NdotL * DiffuseColor;
    
    //specular color
    vec3 halfVector = normalize(lightDir + eyeDir);
    float NdotH = max(dot(inNormal, halfVector),0.0);
    vec3 specularComponent = SpecularColor * pow(NdotH, SpecularPower);
    
    Color = vec4((((AmbientColor + diffuseComponent) * inColor) + specularComponent), 1.0);

    gl_Position = MVPMatrix * vec4(inVertex, 1.0);
}
