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

layout(set = 0, binding = 2) uniform  LightProperties
{
    vec4 color; //(r, g, b, intensity coefficient)
    vec4 pos; // (x, y, z, unused)
    vec4 dir; // (x, y, z, unused)
} lightProps;

layout(set = 2, binding = 0) uniform  Object
{
    mat4 transform;
} obj;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTex;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec4 worldPos;
layout(location = 2) out vec4 worldPosLightCoord;
layout(location = 3) out vec3 tangentDirPos;

void main() 
{

    vec3 T = normalize(vec3(obj.transform * vec4(inTangent, 0.0))); // skip translation part
    vec3 B = normalize(vec3(obj.transform * vec4(inBitangent, 0.0))); // skip translation part
    vec3 N = normalize(vec3(obj.transform * vec4(inNormal, 0.0))); // skip translation part
    mat3 TBN = transpose(mat3(T, B, N));
    tangentDirPos = TBN * lightProps.dir.xyz;

    worldPos = obj.transform * vec4(inPosition, 1);
    texCoord = inTex;

    worldPosLightCoord = light.proj * light.view * worldPos;
    gl_Position = camera.proj * camera.view * worldPos;
    
}