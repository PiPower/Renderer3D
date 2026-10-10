#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(set = 0, binding = 0) uniform  Camera
{
    mat4 view;
    mat4 proj;
} camera;

layout(set = 0, binding = 1) uniform  Light
{
    mat4 view;
    mat4 proj;
} light;

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

    worldPos = obj.transform * vec4(inPosition, 1);
    faceNormal =  transpose(inverse(mat3(obj.transform))) * inNormal;
    texCoord = inTex;

    worldPosLightCoord = light.proj * light.view * worldPos;
    gl_Position = camera.proj * camera.view * worldPos;
    
}