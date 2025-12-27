#version 410 core

out vec4 outColor;
uniform vec3 uColor;
uniform float uAlpha;

void main()
{
    outColor = vec4(uColor, uAlpha);
}
