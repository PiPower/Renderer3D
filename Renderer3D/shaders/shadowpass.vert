#version 450

layout(set = 0, binding = 0) uniform Globals
{
    mat4 view;
    mat4 proj;
} global;

layout(set = 2, binding = 0) uniform  ObjectTransform
{
    mat4 model;
} objectTransform;

layout(location = 0) in vec3 inPosition;

void main() 
{
    vec4 worldPos = objectTransform.model * vec4(inPosition, 1.0);
    gl_Position = global.proj * global.view * worldPos;
}