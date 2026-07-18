#version 450

layout(location = 0) out vec2 fragUV;

void main() {
    // Hardcoded positions forming one triangle that covers the whole screen
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragUV = (positions[gl_VertexIndex] + 1.0) * 0.5;   // maps clip-space to 0-1 UV range
}