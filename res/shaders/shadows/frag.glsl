#version 330 core

in vec3 FragPos;

uniform vec3 lightPos;
uniform float far_plane;

void main()
{
    // Calculate distance from light to fragment
    float lightDistance = length(FragPos - lightPos);
    
    // Map to [0, 1] range by dividing by far_plane
    lightDistance = lightDistance / far_plane;
    
    // Write distance as depth value
    gl_FragDepth = lightDistance;
}