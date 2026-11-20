// LCD3x: a handheld LCD's sub-pixels and lines.
// Original by Gigaherz, license: public domain (libretro slang-shaders,
// handheld/shaders/lcd3x.slang). Ported to GLSL ES 1.00 for piccolo with its
// default parameters fixed; the angles are taken modulo one period first, so
// they stay exact with the Tegra 4's 20-bit floats.

void main()
{
    vec3 color = texture2D(u_source, v_coord).rgb;
    vec2 angle = fract(v_coord * u_source_size.xy) * 6.283185307;
    const vec3 offsets = vec3(3.141592654) * vec3(1.0 / 2.0, 1.0 / 2.0 - 2.0 / 3.0, 1.0 / 2.0 - 4.0 / 3.0);
    float brighten_scanlines = 16.0;
    float brighten_lcd = 4.0;
    float yfactor = (brighten_scanlines + sin(angle.y)) / (brighten_scanlines + 1.0);
    vec3 xfactors = (brighten_lcd + sin(angle.x + offsets)) / (brighten_lcd + 1.0);
    gl_FragColor = vec4(yfactor * xfactors * color, 1.0);
}
