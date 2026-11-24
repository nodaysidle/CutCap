// Fragment stage of the ClipFrameItem look shader: the same pipeline ClipFrameItem::process()
// runs on the CPU (eq -> boxblur -> vignette -> chromakey on Y'CbCr codes, then back to
// premultiplied R'G'B'A'), so the preview matches the FFmpeg filters used on export.
// Regenerate the .qsb next to this file with:
//   /usr/lib/qt6/bin/qsb --qt6 --batchable -o look.frag.qsb look.frag
//
// Uniform block (std140, after mat4 qt_ModelViewProjectionMatrix + float qt_Opacity at 64):
//   u[0] = (yr, yg, yb, 16)      forward Y row, limited range
//   u[1] = (ur, ug, ub, 128)     forward U row
//   u[2] = (vr, vg, vb, 128)     forward V row
//   u[3] = (rv, gu, gv, bu)      inverse coefficients (YUV -> RGB)
//   u[4] = (eqActive, brightness, contrast, saturation)
//   u[5] = (radiusY, radiusC, vignette, chromaKey)
//   u[6] = (keyU, keyV, similarity, blend)
//   u[7] = (texW, texH, dmax, 1.164384)
//   u[8] = (1/texW, 1/texH, 0, 0)
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_ModelViewProjectionMatrix;
    float qt_Opacity;
    vec4 u[9];
};

layout(binding = 1) uniform sampler2D qt_Texture;

// Tap budget of the blur loops; radii beyond it are rendered by the CPU path instead.
const int MAXR = 32;

void rgbToYUV(vec3 rgb, out float Y, out float U, out float V)
{
    Y = u[0].w + dot(u[0].xyz, rgb);
    U = u[1].w + dot(u[1].xyz, rgb);
    V = u[2].w + dot(u[2].xyz, rgb);
}

void applyEq(inout float Y, inout float U, inout float V)
{
    if (u[4].x > 0.5) {
        float v = u[4].z * (Y / 255.0 - 0.5) + 0.5 + u[4].y;
        Y = clamp(v * 256.0, 0.0, 255.0);
        U = clamp((U - 128.0) * u[4].w + 128.0, 0.0, 255.0);
        V = clamp((V - 128.0) * u[4].w + 128.0, 0.0, 255.0);
    }
}

void main()
{
    vec2 texSize = u[7].xy;
    vec2 texel = u[8].xy;
    vec2 uv = qt_TexCoord0;

    vec4 tex = texture(qt_Texture, uv);
    float Y, U, V;
    rgbToYUV(tex.rgb * 255.0, Y, U, V);
    applyEq(Y, U, V);
    float alpha = tex.a * 255.0;

    if (u[5].y > 0.5) {
        // boxblur: luma over a (2*rY+1)^2 window, chroma over (2*rC+1)^2, edges replicated.
        int rY = int(u[5].x + 0.5);
        int rC = int(u[5].y + 0.5);
        float sumY = 0.0;
        float sumU = 0.0;
        float sumV = 0.0;
        vec2 uvLo = texel * 0.5;
        vec2 uvHi = vec2(1.0, 1.0) - texel * 0.5;
        for (int dy = -MAXR; dy <= MAXR; ++dy) {
            if (dy < -rC)
                continue;
            if (dy > rC)
                break;
            for (int dx = -MAXR; dx <= MAXR; ++dx) {
                if (dx < -rC)
                    continue;
                if (dx > rC)
                    break;
                vec2 tuv = clamp(uv + vec2(float(dx), float(dy)) * texel, uvLo, uvHi);
                float tY, tU, tV;
                rgbToYUV(texture(qt_Texture, tuv).rgb * 255.0, tY, tU, tV);
                applyEq(tY, tU, tV);
                sumU += tU;
                sumV += tV;
                if (dx >= -rY && dx <= rY && dy >= -rY && dy <= rY)
                    sumY += tY;
            }
        }
        Y = sumY / float((2 * rY + 1) * (2 * rY + 1));
        U = sumU / float((2 * rC + 1) * (2 * rC + 1));
        V = sumV / float((2 * rC + 1) * (2 * rC + 1));
    }

    if (u[5].z > 0.5) {
        // vignette with FFmpeg's default angle PI/5, strength cos^4.
        vec2 px = uv * texSize - 0.5;
        float dnorm = length(px - texSize * 0.5) / u[7].z;
        float c = cos(0.6283185307179586 * dnorm);
        float f = dnorm > 1.0 ? 0.0 : c * c * c * c;
        Y *= f;
        U = (U - 127.0) * f + 127.0;
        V = (V - 127.0) * f + 127.0;
    }

    if (u[5].w > 0.5) {
        // chromakey: alpha from the U/V distance to the key colour (full-range conversion).
        float du = U - u[6].x;
        float dv = V - u[6].y;
        float diff = sqrt((du * du + dv * dv) / (255.0 * 255.0 * 2.0));
        float keep = u[6].w > 0.0001 ? clamp((diff - u[6].z) / u[6].w, 0.0, 1.0)
                                      : (diff > u[6].z ? 1.0 : 0.0);
        alpha *= keep;
    }

    float yy = u[7].w * (Y - 16.0);
    float uu = U - 128.0;
    float vv = V - 128.0;
    float af = clamp(alpha, 0.0, 255.0) / 255.0;
    vec3 o;
    o.r = clamp(yy + u[3].x * vv, 0.0, 255.0);
    o.g = clamp(yy + u[3].y * uu + u[3].z * vv, 0.0, 255.0);
    o.b = clamp(yy + u[3].w * uu, 0.0, 255.0);
    fragColor = vec4(o / 255.0 * af, af) * qt_Opacity;
}
