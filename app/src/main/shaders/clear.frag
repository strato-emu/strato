#version 460

layout (location = 0) out vec4 colour;

layout (push_constant) uniform constants {
    vec4 colour;
    bool clearDepth;
    float depth;
} PC;

void main()
{
    if (PC.clearDepth)
        gl_FragDepth = PC.depth;
    else
        colour = PC.colour;
}
