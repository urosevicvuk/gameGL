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
uniform int numLights;
uniform vec3 viewPos;
uniform mat4 lightSpaceMatrix0;
uniform mat4 lightSpaceMatrix1;
uniform mat4 lightSpaceMatrix2;
uniform mat4 lightSpaceMatrix3;

float ShadowCalculation(vec3 fragPos, int lightIndex)
{
    if(lightIndex == 0) {
        vec4 fragPosLightSpace = lightSpaceMatrix0 * vec4(fragPos, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        
        float closestDepth = texture(shadowMap0, projCoords.xy).r;
        float currentDepth = projCoords.z;
        
        float bias = 0.005;
        float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
        return shadow;
    }
    
    return 0.0; // No shadows for other lights yet
}

void main()
{
    vec3 FragPos = texture(gPosition, TexCoord).rgb;
    vec3 Normal = texture(gNormal, TexCoord).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoord).rgb;
    float Specular = texture(gAlbedoSpec, TexCoord).a;
    
    float ssao = texture(ssaoTexture, TexCoord).r;
    vec3 lighting = Diffuse * 0.1 * ssao; // Ambient with SSAO
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
            
            float shadow = ShadowCalculation(FragPos, i);
            lighting += (1.0 - shadow) * (diffuse + specular);
        }
    }
    
    FragColor = vec4(lighting, 1.0);
}