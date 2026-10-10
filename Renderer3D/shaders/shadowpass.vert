#version 450

layout(set = 0, binding = 0) uniform  Light
{
    mat4 view;
    mat4 proj;
} light;

layout(set = 2, binding = 0) uniform  Object
{
    mat4 transform;
} obj;

layout(location = 0) in vec3 inPosition;

void main() 
{
    vec4 worldPos = obj.transform * vec4(inPosition, 1);
    gl_Position = light.proj * light.view * worldPos;
}