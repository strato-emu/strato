#version 460

layout (location = 0) out vec2 dstPosition;

layout (push_constant) uniform constants {
    vec2 dstOriginClipSpace;
    vec2 dstDimensionsClipSpace;
} PC;

void main() {
	const vec2 lut[6] = vec2[6](
        vec2(0, 0),
        vec2(0, 1),
        vec2(1, 1),
        vec2(1, 1),
        vec2(1, 0),
        vec2(0, 0)
    );

    dstPosition = lut[gl_VertexIndex];
    gl_Position = vec4(PC.dstOriginClipSpace + PC.dstDimensionsClipSpace * lut[gl_VertexIndex], 0, 1);
}
