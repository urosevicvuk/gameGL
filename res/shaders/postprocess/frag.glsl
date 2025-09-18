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
    // Simple sepia post-processing effect
    vec3 color = texture(screenTexture, TexCoord).rgb;
    
    // Convert to sepia tone for medieval atmosphere
    float grey = dot(color, vec3(0.299, 0.587, 0.114));
    color = vec3(grey * 1.2, grey * 1.0, grey * 0.8);
    
    FragColor = vec4(color, 1.0);
}