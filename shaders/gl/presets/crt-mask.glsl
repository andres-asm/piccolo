// A simple CRT for weak GPUs: the picture times an aperture grille (a tiny
// mask texture, tiled two screen pixels across) and a scanline profile per
// source line: a texture read and a few multiplies. The idea is crt-potato's
// (a mask texture over the picture); this is piccolo's own code and mask,
// not a port.
//
// The line profile is symmetric around each line's center (bright in the
// middle, darker toward the edges between lines), so it looks the same at
// any whole factor: odd factors (x3, x5, x7) used to split a line into an
// uneven lit half and dark half. At x2 a line's two rows are alike, so there
// are no visible lines there.
//
// The mask is tiled with mod() instead of a repeating texture: GLES 2 only
// repeats power-of-two textures. Only its top row is used (the bottom one is
// the same, darker: what the old line / gap split read).
//
// SPDX-License-Identifier: MIT (piccolo's own; see LICENSE)

void main()
{
    vec3 color = texture2D(u_source, v_coord).rgb;
    // Small numbers only (the Tegra 4 computes in fp20): the screen column
    // within the mask, and how far this pixel is from its line's center
    // (0 at the center, 1 at the edge).
    float column = mod(floor(gl_FragCoord.x), u_lut_size.x);
    vec3 mask = texture2D(u_lut, vec2(column + 0.5, 0.5) * u_lut_size.zw).rgb;
    float edge = abs(fract(v_coord.y * u_source_size.y) - 0.5) * 2.0;
    float line = 1.0 - 0.5 * edge * edge;
    // The mask and the lines darken; a little gain gives some of it back.
    gl_FragColor = vec4(min(color * mask * line * 1.3, vec3(1.0)), 1.0);
}
