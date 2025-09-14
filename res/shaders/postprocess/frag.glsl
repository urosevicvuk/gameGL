#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D screenTexture;
uniform float gamma;
uniform float exposure;
uniform float time;

vec3 tonemap(vec3 color) {
    // Simple Reinhard tone mapping
    return color / (color + vec3(1.0));
}

void main()
{
    vec3 hdrColor = texture(screenTexture, TexCoord).rgb;
    
    // Exposure
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    
    // Tone mapping
    mapped = tonemap(mapped);
    
    // Gamma correction 
    mapped = pow(mapped, vec3(1.0/gamma));
    
    // Add slight tavern atmosphere with warm tint
    mapped.r *= 1.1;
    mapped.g *= 1.05;
    
    // Subtle vignette for atmosphere
    float dist = distance(TexCoord, vec2(0.5));
    float vignette = smoothstep(0.8, 0.2, dist);
    mapped *= mix(0.7, 1.0, vignette);
    
    FragColor = vec4(mapped, 1.0);
}