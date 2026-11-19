#version 450

// piccolo's pass 0: the picture controls of a TV, at the core's resolution,
// before any preset. Every parameter's default leaves the picture unchanged.
// piccolo sets them from its settings (IMAGE SETTINGS); the names are the
// contract with piccolo_settings.cpp.
//
// Order: black level, color profile (to linear with PROFILE_GAMMA_IN, 3x3
// matrix, back with PROFILE_GAMMA_OUT as a plain power or the sRGB curve,
// mixed with the uncorrected color by PROFILE_STRENGTH), temperature and RGB
// gain, saturation and hue (YIQ), contrast and brightness, gamma.
//
// SPDX-License-Identifier: MIT (piccolo's own; see LICENSE)

layout(push_constant) uniform Push
{
    vec4 SourceSize;
    vec4 OutputSize;
} params;

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4  MVP;
    float BRIGHTNESS;
    float CONTRAST;
    float SATURATION;
    float HUE;
    float GAMMA;
    float BLACK_LEVEL;
    float TEMPERATURE;
    float GAIN_R;
    float GAIN_G;
    float GAIN_B;
    float PROFILE_GAMMA_IN;
    float PROFILE_GAMMA_OUT;
    float PROFILE_CURVE;
    float PROFILE_STRENGTH;
    float M00; float M01; float M02;
    float M10; float M11; float M12;
    float M20; float M21; float M22;
} global;

#pragma parameter BRIGHTNESS "Brightness" 0.0 -1.0 1.0 0.01
#pragma parameter CONTRAST "Contrast" 1.0 0.0 2.0 0.01
#pragma parameter SATURATION "Saturation" 1.0 0.0 2.0 0.01
#pragma parameter HUE "Hue (degrees)" 0.0 -180.0 180.0 1.0
#pragma parameter GAMMA "Gamma" 1.0 0.5 2.0 0.01
#pragma parameter BLACK_LEVEL "Black level" 0.0 -0.5 0.5 0.01
#pragma parameter TEMPERATURE "Temperature" 0.0 -1.0 1.0 0.01
#pragma parameter GAIN_R "Red gain" 1.0 0.0 2.0 0.01
#pragma parameter GAIN_G "Green gain" 1.0 0.0 2.0 0.01
#pragma parameter GAIN_B "Blue gain" 1.0 0.0 2.0 0.01
#pragma parameter PROFILE_GAMMA_IN "Profile gamma in" 1.0 0.5 4.0 0.01
#pragma parameter PROFILE_GAMMA_OUT "Profile gamma out" 1.0 0.5 4.0 0.01
#pragma parameter PROFILE_CURVE "Profile curve (0 = power, 1 = sRGB)" 0.0 0.0 1.0 1.0
#pragma parameter PROFILE_STRENGTH "Profile strength" 1.0 0.0 1.0 0.01
#pragma parameter M00 "Matrix R<-R" 1.0 -2.0 2.0 0.001
#pragma parameter M01 "Matrix R<-G" 0.0 -2.0 2.0 0.001
#pragma parameter M02 "Matrix R<-B" 0.0 -2.0 2.0 0.001
#pragma parameter M10 "Matrix G<-R" 0.0 -2.0 2.0 0.001
#pragma parameter M11 "Matrix G<-G" 1.0 -2.0 2.0 0.001
#pragma parameter M12 "Matrix G<-B" 0.0 -2.0 2.0 0.001
#pragma parameter M20 "Matrix B<-R" 0.0 -2.0 2.0 0.001
#pragma parameter M21 "Matrix B<-G" 0.0 -2.0 2.0 0.001
#pragma parameter M22 "Matrix B<-B" 1.0 -2.0 2.0 0.001

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
    vec3 color = texture(Source, vTexCoord).rgb;

    // Black level: lifts (or crushes) the blacks, whites stay white.
    color = clamp((color + global.BLACK_LEVEL) / (1.0 + global.BLACK_LEVEL), 0.0, 1.0);

    // Color profile: a screen's colors as a matrix in linear light. Back
    // with a plain power, or the sRGB curve (straight near black, then the
    // power with 1.055 / 0.055) for profiles that target sRGB exactly (NTSC).
    vec3 linear = pow(color, vec3(global.PROFILE_GAMMA_IN));
    mat3 profile = mat3(global.M00, global.M10, global.M20,    // GLSL matrices are column by column
                        global.M01, global.M11, global.M21,
                        global.M02, global.M12, global.M22);
    linear = clamp(profile * linear, 0.0, 1.0);
    vec3 profiled = pow(linear, vec3(1.0 / global.PROFILE_GAMMA_OUT));
    if (global.PROFILE_CURVE > 0.5)
    {
        vec3 curve = 1.055 * pow(linear, vec3(1.0 / global.PROFILE_GAMMA_OUT)) - 0.055;
        profiled = mix(curve, linear * 12.92, vec3(lessThanEqual(linear, vec3(0.0031308))));
    }
    color = mix(color, profiled, global.PROFILE_STRENGTH);

    // Temperature: warmer (+) adds red and takes blue, cooler (-) the other way.
    vec3 temperature = vec3(1.0 + 0.2 * global.TEMPERATURE, 1.0, 1.0 - 0.2 * global.TEMPERATURE);
    color *= temperature * vec3(global.GAIN_R, global.GAIN_G, global.GAIN_B);

    // Saturation and hue in YIQ (NTSC's own space): Y is untouched, the
    // chroma (I, Q) is scaled and rotated.
    const mat3 to_yiq = mat3(0.299, 0.596, 0.211,
                             0.587, -0.274, -0.523,
                             0.114, -0.322, 0.312);
    const mat3 to_rgb = mat3(1.0, 1.0, 1.0,
                             0.956, -0.272, -1.106,
                             0.621, -0.647, 1.703);
    vec3 yiq = to_yiq * color;
    float angle = radians(global.HUE);
    float c = cos(angle);
    float s = sin(angle);
    yiq.yz = mat2(c, s, -s, c) * yiq.yz * global.SATURATION;
    color = to_rgb * yiq;

    // Contrast around mid grey, then brightness.
    color = (color - 0.5) * global.CONTRAST + 0.5 + global.BRIGHTNESS;

    // Gamma: above 1 darkens the mid tones.
    color = pow(clamp(color, 0.0, 1.0), vec3(global.GAMMA));

    FragColor = vec4(color, 1.0);
}
