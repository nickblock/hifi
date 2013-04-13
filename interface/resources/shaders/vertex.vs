
attribute vec3  inVertex;		
attribute vec3  inNormal;	
attribute vec3  inColor;

uniform mat MVPMatrix;

varying vec4 Color;

void main()
{
    Color = vec4(inColor, 1.0);

    gl_Position = MVPamtrix * vec4(inVertex, 1.0);
}
