#version 450
#ifndef TEXTURE_COUNT
    #define TEXTURE_COUNT 1
#endif

layout(binding = 0) uniform GlobalUbo
{
    mat4 view;
    mat4 proj;
    vec4 lightPos;
    vec4 lightCol;
} globalUbo;

layout(binding = 2) uniform  PerObjUbo
{
    mat4 model;
    ivec4 indexVec;
} localUbo;

layout(binding = 1) uniform sampler2D texSampler[TEXTURE_COUNT];

//push constants block
layout( push_constant ) uniform constants
{
    ivec4 index;
} PushConstants;


layout(location = 0) in vec3 faceNormal;
layout(location = 1) in vec2 texCoord; 
layout(location = 2) in vec4 worldPos;


layout(location = 0) out vec4 outColor;
void main()
{

    vec3 norm = normalize(faceNormal);
    vec3 lightDir = normalize(globalUbo.lightPos.xyz - worldPos.xyz);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * globalUbo.lightCol.xyz;
    //vec3 diffuse = vec3(0,0,0);
    vec4 texCol = texture(texSampler[PushConstants.index.x], texCoord);
    if(texCol.a == 0)
    {
        discard;
    }
    vec3 ambient =  globalUbo.lightCol.w *  globalUbo.lightCol.xyz;
    outColor = texCol * vec4(ambient + diffuse, 1.0f);

    float gamma = 2.2;
    outColor = texCol;
    outColor.rgb = pow(outColor.rgb, vec3(1.0/gamma));

}