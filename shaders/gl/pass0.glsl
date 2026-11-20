// piccolo's pass 0 on OpenGL ES: the picture controls of a TV, at the core's
// resolution, before any preset. The same as ../pass0.slang (read that for
// the order and the why), in GLSL ES 1.00 for gl_shader_chain (frontends-common).
// piccolo sets the uniforms from IMAGE SETTINGS; every default leaves the
// picture unchanged (piccolo sets them all, the values here don't matter).
//
// SPDX-License-Identifier: MIT (piccolo's own; see LICENSE)

uniform float BRIGHTNESS;
uniform float CONTRAST;
uniform float SATURATION;
uniform float HUE;
uniform float GAMMA;
uniform float BLACK_LEVEL;
uniform float TEMPERATURE;
uniform float GAIN_R;
uniform float GAIN_G;
uniform float GAIN_B;
uniform float PROFILE_GAMMA_IN;
uniform float PROFILE_GAMMA_OUT;
uniform float PROFILE_CURVE;
uniform float PROFILE_STRENGTH;
uniform float M00; uniform float M01; uniform float M02;
uniform float M10; uniform float M11; uniform float M12;
uniform float M20; uniform float M21; uniform float M22;

void main()
{
    vec3 color = texture2D(u_source, v_coord).rgb;
    color = clamp((color + BLACK_LEVEL) / (1.0 + BLACK_LEVEL), 0.0, 1.0);

    vec3 linear = pow(color, vec3(PROFILE_GAMMA_IN));
    mat3 profile = mat3(M00, M10, M20,    // GLSL matrices are column by column
                        M01, M11, M21,
                        M02, M12, M22);
    linear = clamp(profile * linear, 0.0, 1.0);
    vec3 profiled = pow(linear, vec3(1.0 / PROFILE_GAMMA_OUT));
    if (PROFILE_CURVE > 0.5)
    {
        vec3 curve = 1.055 * pow(linear, vec3(1.0 / PROFILE_GAMMA_OUT)) - 0.055;
        profiled = mix(curve, linear * 12.92, vec3(lessThanEqual(linear, vec3(0.0031308))));
    }
    color = mix(color, profiled, PROFILE_STRENGTH);

    vec3 temperature = vec3(1.0 + 0.2 * TEMPERATURE, 1.0, 1.0 - 0.2 * TEMPERATURE);
    color *= temperature * vec3(GAIN_R, GAIN_G, GAIN_B);

    mat3 to_yiq = mat3(0.299, 0.596, 0.211,
                       0.587, -0.274, -0.523,
                       0.114, -0.322, 0.312);
    mat3 to_rgb = mat3(1.0, 1.0, 1.0,
                       0.956, -0.272, -1.106,
                       0.621, -0.647, 1.703);
    vec3 yiq = to_yiq * color;
    float angle = radians(HUE);
    float c = cos(angle);
    float s = sin(angle);
    yiq.yz = mat2(c, s, -s, c) * yiq.yz * SATURATION;
    color = to_rgb * yiq;

    color = (color - 0.5) * CONTRAST + 0.5 + BRIGHTNESS;
    color = pow(clamp(color, 0.0, 1.0), vec3(GAMMA));
    gl_FragColor = vec4(color, 1.0);
}
