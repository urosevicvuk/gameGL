#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

struct Light {
    vec3 Position;
    vec3 Color;
    float Radius;
};

uniform Light lights[8];
uniform samplerCube shadowMaps[4];
uniform int numLights;
uniform vec3 viewPos;
uniform float far_plane;

float ShadowCalculation(vec3 fragPos, vec3 lightPos, samplerCube shadowMap)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    float bias = 0.15;
    float closestDepth = texture(shadowMap, fragToLight).r;
    closestDepth *= far_plane;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    return shadow;
}

void main()
{
    vec3 FragPos = texture(gPosition, TexCoord).rgb;
    vec3 Normal = texture(gNormal, TexCoord).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoord).rgb;
    float Specular = texture(gAlbedoSpec, TexCoord).a;
    
    vec3 lighting = Diffuse * 0.1; // Ambient
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
            
            // float shadow = ShadowCalculation(FragPos, lights[i].Position, shadowMaps[i]);
            // lighting += (1.0 - shadow) * (diffuse + specular);
            lighting += diffuse + specular; // Skip shadows for now, focus on SSAO
        }
    }
    
    FragColor = vec4(lighting, 1.0);
}