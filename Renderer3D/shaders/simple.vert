#version 450
#extension GL_KHR_vulkan_glsl : enable
#ifndef LIGHT_COUNT
    #define LIGHT_COUNT
#endif
/*
struct Light
{
    vec4 color; // (R, G, B, Intensity)
    vec4 pos;   // (x, y, z, unused)
};

layout(set = 0, binding = 0) uniform Globals
{
    mat4 view;
    mat4 proj;
    Light lights[LIGHT_COUNT]; // (R, G, B, Intensity);
    mat4 lightViewProj[LIGHT_COUNT];
} global;
*/

layout(set = 0, binding = 0) uniform  Camera
{
    mat4 proj;
    mat4 view;
} camera;

layout(set = 2, binding = 0) uniform  Object
{
    mat4 transform;
} obj;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTex;


layout(location = 0) out vec3 faceNormal;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec4 worldPos;
layout(location = 3) out vec4 worldPosLightCoord;

void main() 
{

    worldPos = obj.transform * vec4(inPosition, 1.0);
    worldPosLightCoord = vec4(0.5, 0.7, 0.5, 1.0);
    faceNormal =  transpose(inverse(mat3(obj.transform))) * inNormal;
    texCoord = inTex;

    gl_Position = camera.proj * camera.view * worldPos;
    
}