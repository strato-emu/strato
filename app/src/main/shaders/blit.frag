#version 460

layout (binding = 0, set = 0) uniform sampler2D src;
layout (location = 0) in vec2 dstUV;
layout (location = 0) out vec4 colour;

layout (push_constant) uniform constants {
    layout (offset = 16)
    vec2 srcOriginUV;
    vec2 dstSrcScaleFactor;
} PC;

void main()
{
    vec2 srcUV = dstUV * PC.dstSrcScaleFactor + PC.srcOriginUV;
    colour.rgba = texture(src, srcUV);
}
