layout(set = 0, binding = 0) uniform texture2D currentFrame;
layout(set = 0, binding = 1) uniform texture2D previousFrame;
layout(set = 0, binding = 2) uniform texture2D currentDepth;
layout(set = 0, binding = 3) uniform texture2D previousDepth;
layout(set = 0, binding = 4) uniform sampler nearestSampler;
layout(set = 0, binding = 5) uniform sampler linearSampler;

layout(set = 1, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

layout(set = 1, binding = 1) uniform PreviousFrameCameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} previousFrameCBO;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 currentFrameColor = texture(sampler2D(currentFrame, linearSampler), uv);

    float depth = texture(sampler2D(currentDepth, linearSampler), uv).r;
    float prevDepth = texture(sampler2D(previousDepth, linearSampler), uv).r;

    vec4 viewSpacePos = cbo.inverseProjection * vec4(uv*2-1, depth, 1.0);
    vec4 hWorldSpacePos = cbo.inverseView * viewSpacePos;

    // TODO: create a velocity buffer
    vec4 previousViewSpacePos = previousFrameCBO.view * vec4(hWorldSpacePos);
    vec4 prevNDC = previousFrameCBO.projection * previousViewSpacePos;
    prevNDC.xyz /= prevNDC.w;
    vec2 reprojectedUV = (prevNDC.xy + 1.0) / 2.0;

    float reprojected = 1.0f-clamp(length(previousViewSpacePos.xyz/previousViewSpacePos.w - viewSpacePos.xyz/viewSpacePos.w)*0.5, 0, 1.0);

    reprojected *= 1.0f - float(
        reprojectedUV.x < 0 || reprojectedUV.x > 1
        || reprojectedUV.y < 0 || reprojectedUV.y > 1
    );

    vec4 previousFrameColor = texture(sampler2D(previousFrame, linearSampler), reprojectedUV);

    float alpha = 0.5;
    //if(uv.x > 0.5)
    {
        if(isnan(previousFrameColor.x))
        {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
        else
        {
            outColor = reprojected * (mix(previousFrameColor, currentFrameColor, 1-alpha)) + (1.0f-reprojected) * currentFrameColor;
        }
    }
    /*else
    {

        if(isnan(hWorldSpacePos.x))
        {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
        else
        {
            outColor = abs(vec4(reprojected));
        }
    }*/
}