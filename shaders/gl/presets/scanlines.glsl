// Scanlines Sine Absolute Value: an ultra light scanline shader.
// Original by RiskyJumps, license: public domain (libretro slang-shaders,
// scanlines/shaders/scanlines-sine-abs.slang). Ported to GLSL ES 1.00 for
// piccolo with its default parameters fixed; the angle is taken modulo one
// period first, so it stays exact with the Tegra 4's 20-bit floats.

void main()
{
    vec3 color = texture2D(u_source, v_coord).rgb;
    // freq 0.5: one sine period every two source lines; phase 0.5.
    float angle = fract(v_coord.y * u_source_size.y * 0.5) * 6.283185307 + 0.5;
    float lines = abs(sin(angle) * 1.25);   // amp 1.25, blacks 0, whites 1
    gl_FragColor = vec4(color * lines, 1.0);
}
