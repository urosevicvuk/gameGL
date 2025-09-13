#version 330 core

void main()
{
    // OpenGL automatically writes gl_FragCoord.z to depth buffer
    // We can leave this empty, but explicit depth write for clarity:
    gl_FragDepth = gl_FragCoord.z;
}