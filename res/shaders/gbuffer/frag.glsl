#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec3 FragPos;
in vec2 TexCoord;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform vec3 materialColor;
uniform float hasTexture;

void main()
{
    gPosition = FragPos;
    gNormal = normalize(Normal);
    
    if (hasTexture > 0.5) {
        gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoord).rgb;
        gAlbedoSpec.a = texture(texture_specular1, TexCoord).r;
    } else {
        gAlbedoSpec.rgb = materialColor;
        gAlbedoSpec.a = 0.3; // Default specular
    }
}