// piccolo's final scale, sharp bilinear (../scale-sharp-bilinear.slang in
// GLSL ES 1.00): every source pixel is a sharp block of the whole part of the
// scale; only the screen pixels where two source pixels meet are blended.
//
// SPDX-License-Identifier: MIT (piccolo's own; see LICENSE)

void main()
{
    vec2 texel = v_coord * u_source_size.xy;
    vec2 scale = max(floor(u_output_size.xy / u_source_size.xy), vec2(1.0));
    vec2 inside = fract(texel) - 0.5;
    vec2 flat_range = 0.5 - 0.5 / scale;
    vec2 ramp = (inside - clamp(inside, -flat_range, flat_range)) * scale + 0.5;
    vec2 coordinate = (floor(texel) + ramp) * u_source_size.zw;
    gl_FragColor = vec4(texture2D(u_source, coordinate).rgb, 1.0);
}
