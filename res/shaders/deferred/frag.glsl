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
uniform samplerCube shadowMap0;
uniform samplerCube shadowMap1;
uniform samplerCube shadowMap2;
uniform samplerCube shadowMap3;
uniform samplerCube shadowMap4;
uniform samplerCube shadowMap5;
uniform samplerCube shadowMap6;
uniform samplerCube shadowMap7;
uniform int numLights;
uniform vec3 viewPos;
uniform float far_plane;

float ShadowCalculation(vec3 fragPos, int lightIndex)
{
    // Calculate vector from light to fragment for cube map sampling
    vec3 lightToFrag = fragPos - lights[lightIndex].Position;
    
    // Get distance to fragment (normalize to [0,1] range)
    float currentDepth = length(lightToFrag) / far_plane;
    
    // Sample cube map based on light index using light-to-fragment direction
    float closestDepth;
    if(lightIndex == 0) {
        closestDepth = texture(shadowMap0, lightToFrag).r;
    } else if(lightIndex == 1) {
        closestDepth = texture(shadowMap1, lightToFrag).r;
    } else if(lightIndex == 2) {
        closestDepth = texture(shadowMap2, lightToFrag).r;
    } else if(lightIndex == 3) {
        closestDepth = texture(shadowMap3, lightToFrag).r;
    } else if(lightIndex == 4) {
        closestDepth = texture(shadowMap4, lightToFrag).r;
    } else if(lightIndex == 5) {
        closestDepth = texture(shadowMap5, lightToFrag).r;
    } else if(lightIndex == 6) {
        closestDepth = texture(shadowMap6, lightToFrag).r;
    } else if(lightIndex == 7) {
        closestDepth = texture(shadowMap7, lightToFrag).r;
    } else {
        return 0.0;
    }
    
    // More lenient shadow test - if cube map sampling failed, don't shadow
    if(closestDepth <= 0.0) {
        return 0.0; // No shadow if invalid depth
    }
    
    // Bias to prevent shadow acne, but more generous
    float bias = 0.01;
    
    // Compare depths and return shadow value
    return (currentDepth - bias > closestDepth) ? 0.5 : 0.0; // Lighter shadows for testing
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
            // TEMPORARILY DISABLE SHADOWS FOR DEBUGGING
            // if(i < 8 && numLights > 0) {  // Support shadows for all 8 lights
            //     shadow = ShadowCalculation(FragPos, i);
            // }
            
            // Apply shadows with better contrast for dramatic lighting
            float shadowFactor = (1.0 - shadow);
            lighting += shadowFactor * (diffuse + specular);
        }
    }
    
    FragColor = vec4(lighting, 1.0);
}