// A simple CRT for weak GPUs: the picture times a tiny mask texture, tiled
// two screen pixels across (an aperture grille) and once per source line
// down (the line, then the darker gap): one texture read and a multiply.
// The idea is crt-potato's (a mask texture over the picture); this is
// piccolo's own code and mask, not a port.
//
// The mask is tiled with fract()/mod() instead of a repeating texture: GLES 2
// only repeats power-of-two textures.
//
// SPDX-License-Identifier: MIT (piccolo's own; see LICENSE)

void main()
{
    vec3 color = texture2D(u_source, v_coord).rgb;
    // Small numbers only (the Tegra 4 computes in fp20): the screen column
    // within the mask, and where in its source line this pixel is.
    float column = mod(floor(gl_FragCoord.x), u_lut_size.x);
    float row = floor(fract(v_coord.y * u_source_size.y) * u_lut_size.y);
    vec3 mask = texture2D(u_lut, (vec2(column, row) + 0.5) * u_lut_size.zw).rgb;
    // The mask darkens; a little gain gives some of it back.
    gl_FragColor = vec4(min(color * mask * 1.3, vec3(1.0)), 1.0);
}
