#version 450

// piccolo's final scale, sharp bilinear: every source pixel is a sharp block
// of the whole part of the scale; only the screen pixels where two source
// pixels meet are blended (bilinear sampling, with the texture coordinate
// pushed to the pixel centre everywhere else).
//
// SPDX-License-Identifier: MIT (piccolo's own; see LICENSE)

layout(push_constant) uniform Push
{
    vec4 SourceSize;
    vec4 OutputSize;
} params;

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

void main()
{
    vec2 texel = vTexCoord * params.SourceSize.xy;
    vec2 scale = max(floor(params.OutputSize.xy / params.SourceSize.xy), vec2(1.0));

    // Inside a pixel, within half a pixel minus half a screen pixel of its
    // centre: the centre (sharp). Beyond that: a ramp to the next pixel over
    // one screen pixel (soft edge).
    vec2 inside = fract(texel) - 0.5;
    vec2 flat_range = 0.5 - 0.5 / scale;
    vec2 ramp = (inside - clamp(inside, -flat_range, flat_range)) * scale + 0.5;
    vec2 coordinate = (floor(texel) + ramp) * params.SourceSize.zw;

    FragColor = vec4(texture(Source, coordinate).rgb, 1.0);
}
