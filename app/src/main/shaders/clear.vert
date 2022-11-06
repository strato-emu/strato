#version 460

void main() {
	const vec2 lut[6] = vec2[6](
        vec2(-1, -1),
        vec2(-1, 1),
        vec2(1, 1),
        vec2(1, 1),
        vec2(1, -1),
        vec2(-1, -1)
    );

    gl_Position = vec4(lut[gl_VertexIndex], 0, 1);
}
