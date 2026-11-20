// piccolo's final scale when no preset is chosen: the picture as is, sampled
// nearest or bilinear (the preset decides: scale-nearest / scale-bilinear).
//
// SPDX-License-Identifier: MIT (piccolo's own; see LICENSE)

void main()
{
    gl_FragColor = vec4(texture2D(u_source, v_coord).rgb, 1.0);
}
