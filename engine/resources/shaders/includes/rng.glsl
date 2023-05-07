struct RandomSampler {
    uvec2 pixelPos;
    uint frameCount;
    uint sampleIndex;
};

void initRNG(inout RandomSampler rng, vec2 uv, uint frameWidth, uint frameHeight, uint frameCount) {
    const vec2 screenSize = vec2(frameWidth, frameHeight);

    rng.pixelPos = uvec2(uv * screenSize);
    rng.frameCount = frameCount;
    rng.sampleIndex = 0;
}

// from http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
vec2 r2Sequence(uint n) {
    const double g = 1.32471795724474602596;
    double a1 = 1.0 / g;
    double a2 = 1.0 / (g*g);
    return fract(vec2((0.5+a1*n), (0.5+a2*n)));
}

float sampleNoise(inout RandomSampler rng) {
    const uint BLUE_NOISE_SIZE = 64;
    const uint NOISE_COUNT = 64;
    const vec2 blueNoiseUV = vec2(rng.pixelPos + ivec2(r2Sequence(rng.sampleIndex) * ivec2(BLUE_NOISE_SIZE)) % BLUE_NOISE_SIZE) / BLUE_NOISE_SIZE;
    rng.sampleIndex++;

    const uint components = 4;
    const uint coordinate = rng.frameCount % components;
    const uint blueNoiseIndex = (rng.frameCount / components) % NOISE_COUNT;
    const uint textureIndex = globalTextures.blueNoises[blueNoiseIndex];
    float r = texture(sampler2D(textures[textureIndex], nearestSampler), blueNoiseUV)[coordinate];
    return r;
}