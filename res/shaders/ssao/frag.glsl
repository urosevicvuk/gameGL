#version 330 core

out float FragColor;

in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform mat4 projection;

void main()
{
    vec3 fragPos = texture(gPosition, TexCoord).xyz;
    vec3 normal = normalize(texture(gNormal, TexCoord).rgb);
    
    float occlusion = 0.0;
    float radius = 0.3;
    int samples = 16;
    
    // Simple circular sampling around the fragment
    for(int i = 0; i < samples; ++i) {
        float angle = float(i) / float(samples) * 6.28318; // 2 * PI
        vec2 offset = vec2(cos(angle), sin(angle)) * radius;
        
        vec4 samplePos = vec4(fragPos + vec3(offset * 0.1, 0.0), 1.0);
        samplePos = projection * samplePos;
        samplePos.xyz /= samplePos.w;
        samplePos.xy = samplePos.xy * 0.5 + 0.5;
        
        float sampleDepth = texture(gPosition, samplePos.xy).z;
        
        if(sampleDepth < fragPos.z) {
            occlusion += 1.0;
        }
    }
    
    occlusion = 1.0 - (occlusion / float(samples));
    occlusion = pow(occlusion, 2.0); // Enhance contrast
    
    FragColor = occlusion;
}