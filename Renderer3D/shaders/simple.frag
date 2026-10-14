#version 450
#extension GL_KHR_vulkan_glsl : enable

//push constants block
layout( push_constant ) uniform constants
{
    int textureId;
} pc;

layout(set = 0, binding = 2) uniform  LightProperties
{
    vec4 color; //(r, g, b, intensity coefficient)
    vec4 pos; // (x, y, z, unused)
    vec4 dir; // (x, y, z, unused)
} lightProps;

layout(set = 0, binding = 3) uniform sampler2D shadowmap;
layout(set = 0, binding = 4) uniform sampler2DArray normalMap;
layout(set = 1, binding = 0) uniform sampler2DArray diffuseMaps;

layout(location = 0) in vec2 texCoord; 
layout(location = 1) in vec4 worldPos;
layout(location = 2) in vec4 worldPosLightCoord;
layout(location = 3) in vec3 tangentDirPos;

layout(location = 0) out vec4 outColor;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if(projCoords.z > 1.0)
    { 
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float bias = 0.001;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowmap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowmap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;

    return shadow;
}


void main()
{
    vec3 normalVec = texture(normalMap, vec3(texCoord.x, texCoord.y, pc.textureId.x)).rgb;
    normalVec = normalize(normalVec * 2.0 - 1.0);

    vec4 albedo = texture(diffuseMaps, vec3(texCoord.x, texCoord.y, pc.textureId.x));
    vec3 color = albedo.rgb;
    float shadow = ShadowCalculation(worldPosLightCoord);
    float visibile = 1.0 - shadow;

    vec3 ambient = lightProps.color.rgb * lightProps.color.w;
    vec3 lightDirTS = normalize(tangentDirPos);
    float diffCoeff = max(dot(normalVec, -lightDirTS), 0.0);

    vec3 diffuse = diffCoeff * lightProps.color.rgb;

    vec3 lighting = ambient + visibile * diffuse;
    outColor = vec4(lighting * color, albedo.a);

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