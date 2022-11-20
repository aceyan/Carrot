#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

// Per vertex
layout(location = 0) in vec3 inPosition;

// Per instance
layout(location = 1) in vec4 inInstanceColor;
layout(location = 2) in uvec4 inUUID;
layout(location = 3) in mat4 inInstanceTransform;


layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out vec3 outViewNormal;
layout(location = 5) out uvec4 outUUID;
layout(location = 6) out vec3 outViewTangent;

layout(push_constant) uniform TexRegion {
    layout(offset = 24) vec2 center;
    vec2 halfSize;
} region;

void main() {
    uv = (inPosition.xy+vec2(0.5)) * (region.halfSize * 2) + region.center - region.halfSize;
    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * vec4(inPosition, 1.0);
    gl_Position = cbo.projection * viewPosition;

    fragColor = vec4(1.0);
    instanceColor = inInstanceColor;
    outViewPos = viewPosition.xyz;

    outViewNormal = normalize(mat3(modelview) * vec3(0,0,1));
    outViewTangent = normalize(mat3(modelview) * vec3(1,0,0));

    outUUID = inUUID;
}