#version 450
#extension GL_ARB_separate_shader_objects : enable

// Per vertex
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 uv;

layout(set = 2, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

void main() {
    mat3 rot = mat3(
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 0.0, -1.0),
        vec3(0.0, 1.0, 0.0)
    );
    uv = rot * inPosition;
    mat4 viewNoTranslation = mat4(mat3(cbo.view));
    gl_Position = cbo.projection * viewNoTranslation * vec4(inPosition, 1);
}