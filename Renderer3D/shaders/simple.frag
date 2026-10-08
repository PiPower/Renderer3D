#version 450
#extension GL_KHR_vulkan_glsl : enable

//push constants block
layout( push_constant ) uniform constants
{
    int textureId;
} pc;

layout(set = 0, binding = 1) uniform sampler2D shadowmap;
layout(set = 1, binding = 0) uniform sampler2DArray diffuseMaps;

layout(location = 0) in vec3 faceNormal;
layout(location = 1) in vec2 texCoord; 
layout(location = 2) in vec4 worldPos;
layout(location = 3) in vec4 worldPosLightCoord;

layout(location = 0) out vec4 outColor;
void main()
{
    float shadow = texture(shadowmap, vec2(0, 0)).r;
    outColor = texture(diffuseMaps, vec3(texCoord.x, texCoord.y, pc.textureId.x) );
    //outColor = vec4(texCoord.x, texCoord.y, 0, 1.0);
 /*
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
    outColor.rgb = pow(outColor.rgb, vec3(1.0/gamma)); */

}