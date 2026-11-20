// Dot: a handheld LCD's round pixels with a little glow.
// Original by Themaister, license: public domain (libretro slang-shaders,
// handheld/shaders/dot.slang). Ported to GLSL ES 1.00 for piccolo with its
// default parameters fixed (gamma 2.4, shine 0.05, blend 0.65, soft 0). With
// soft 0 every one of the original's nine taps reads the same texel, so this
// reads it once and weighs the nine distances.

float color_bloom(vec3 color)
{
    float bright = dot(color, vec3(0.30, 0.59, 0.11));
    return mix(1.05, 0.95, bright);
}

vec3 lookup(vec2 inside, vec2 offset, vec3 color)
{
    vec2 delta = inside - (offset + vec2(0.5));
    return color * exp(-2.4 * length(delta) * color_bloom(color));
}

void main()
{
    vec3 color = texture2D(u_source, v_coord).rgb;
    vec2 inside = fract(v_coord * u_source_size.xy);
    vec3 mid_color = lookup(inside, vec2(0.0, 0.0), color);
    vec3 sum = mid_color;
    sum += lookup(inside, vec2(-1.0, -1.0), color);
    sum += lookup(inside, vec2( 0.0, -1.0), color);
    sum += lookup(inside, vec2( 1.0, -1.0), color);
    sum += lookup(inside, vec2(-1.0,  0.0), color);
    sum += lookup(inside, vec2( 1.0,  0.0), color);
    sum += lookup(inside, vec2(-1.0,  1.0), color);
    sum += lookup(inside, vec2( 0.0,  1.0), color);
    sum += lookup(inside, vec2( 1.0,  1.0), color);
    gl_FragColor = vec4(mix(1.2 * mid_color, sum, 0.65), 1.0);
}
