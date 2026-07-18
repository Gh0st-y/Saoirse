#version 450

layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;
    vec4 lightPosFarPlane;
} pc;

layout(location = 0) in vec3 fragWorldPos;

void main() {
    float lightDistance = length(fragWorldPos - pc.lightPosFarPlane.xyz);
    gl_FragDepth = lightDistance / pc.lightPosFarPlane.w; // normalize by far plane
}