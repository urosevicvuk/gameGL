#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssaoTexture;

struct Light {
    vec3 Position;
    vec3 Color;
    float Radius;
};

uniform Light lights[8];
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;
uniform sampler2D shadowMap4;
uniform sampler2D shadowMap5;
uniform sampler2D shadowMap6;
uniform sampler2D shadowMap7;
uniform int numLights;
uniform vec3 viewPos;
uniform mat4 lightSpaceMatrix0;
uniform mat4 lightSpaceMatrix1;
uniform mat4 lightSpaceMatrix2;
uniform mat4 lightSpaceMatrix3;
uniform mat4 lightSpaceMatrix4;
uniform mat4 lightSpaceMatrix5;
uniform mat4 lightSpaceMatrix6;
uniform mat4 lightSpaceMatrix7;

float ShadowCalculation(vec3 fragPos, int lightIndex)
{
    // Support shadows for all 8 lights
    vec4 fragPosLightSpace;
    vec3 projCoords;
    float closestDepth, currentDepth;
    float bias = 0.001;
    
    if(lightIndex == 0) {
        fragPosLightSpace = lightSpaceMatrix0 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap0, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    else if(lightIndex == 1) {
        fragPosLightSpace = lightSpaceMatrix1 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap1, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    else if(lightIndex == 2) {
        fragPosLightSpace = lightSpaceMatrix2 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap2, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    else if(lightIndex == 3) {
        fragPosLightSpace = lightSpaceMatrix3 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap3, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    else if(lightIndex == 4) {
        fragPosLightSpace = lightSpaceMatrix4 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap4, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    else if(lightIndex == 5) {
        fragPosLightSpace = lightSpaceMatrix5 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap5, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    else if(lightIndex == 6) {
        fragPosLightSpace = lightSpaceMatrix6 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap6, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    else if(lightIndex == 7) {
        fragPosLightSpace = lightSpaceMatrix7 * vec4(fragPos, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) return 0.0;
        closestDepth = texture(shadowMap7, projCoords.xy).r;
        currentDepth = projCoords.z;
        return currentDepth - bias > closestDepth ? 0.95 : 0.0;
    }
    
    return 0.0;
}

void main()
{
    vec3 FragPos = texture(gPosition, TexCoord).rgb;
    vec3 Normal = texture(gNormal, TexCoord).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoord).rgb;
    float Specular = texture(gAlbedoSpec, TexCoord).a;
    
    float ssao = texture(ssaoTexture, TexCoord).r;
    vec3 lighting = Diffuse * 0.05; // Very subtle ambient to see shadows
    vec3 viewDir = normalize(viewPos - FragPos);
    
    for(int i = 0; i < numLights; ++i)
    {
        float distance = length(lights[i].Position - FragPos);
        if(distance < lights[i].Radius)
        {
            vec3 lightColor = lights[i].Color;
            vec3 lightDir = normalize(lights[i].Position - FragPos);
            vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lightColor;
            
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), 64.0);
            vec3 specular = lightColor * spec * Specular;
            
            float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
            diffuse *= attenuation;
            specular *= attenuation;
            
            // Calculate shadows for all lights (up to 8 shadow maps)
            float shadow = 0.0;
            if(i < 8 && numLights > 0) {  // Support shadows for all 8 lights
                shadow = ShadowCalculation(FragPos, i);
            }
            
            // Apply shadows with better contrast for dramatic lighting
            float shadowFactor = (1.0 - shadow);
            lighting += shadowFactor * (diffuse + specular);
        }
    }
    
    FragColor = vec4(lighting, 1.0);
}