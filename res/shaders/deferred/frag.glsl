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
    // Only light 0 casts shadows for now
    if(lightIndex == 0) {
        vec4 fragPosLightSpace = lightSpaceMatrix0 * vec4(fragPos, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        
        // Check if fragment is outside light frustum
        if(projCoords.x < 0.0 || projCoords.x > 1.0 || 
           projCoords.y < 0.0 || projCoords.y > 1.0 || 
           projCoords.z > 1.0) {
            return 0.0; // No shadow outside frustum
        }
        
        float closestDepth = texture(shadowMap0, projCoords.xy).r;
        float currentDepth = projCoords.z;
        
        // Simple fixed bias to prevent shadow acne
        float bias = 0.002;
        
        // Clean shadow test - either in shadow or not
        return currentDepth - bias > closestDepth ? 0.8 : 0.0;
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
    vec3 lighting = vec3(0.0); // No ambient light - pure darkness when no lights
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
            
            // Only calculate shadows for light index 0 (flashlight) when active
            float shadow = 0.0;
            if(i == 0 && numLights > 0) {
                shadow = ShadowCalculation(FragPos, i);
            }
            lighting += (1.0 - shadow) * (diffuse + specular);
        }
    }
    
    FragColor = vec4(lighting, 1.0);
}