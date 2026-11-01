#version 440
// One rounded panel in one pass: the blurred backdrop at the panel's region, then the fill, then the border.
// Also used with a plain picture as the backdrop and a clear fill, for pictures with rounded corners.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 region;       // the panel's part of the backdrop: x, y, width, height in 0..1
    vec2 itemSize;     // in item units
    float radius;
    float pixel;       // one device pixel in item units
    float borderWidth;
    vec4 fill;         // premultiplied: Qt hands QML colors over that way
    vec4 border;       // premultiplied
};
layout(binding = 1) uniform sampler2D backdrop;

float roundedBox(vec2 p, vec2 halfSize, float r)
{
    vec2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    vec2 halfSize = itemSize * 0.5;
    float r = min(radius, min(halfSize.x, halfSize.y));
    float d = roundedBox(qt_TexCoord0 * itemSize - halfSize, halfSize, r);
    float aa = pixel * 0.5;
    float shape = 1.0 - smoothstep(-aa, aa, d);
    float inner = 1.0 - smoothstep(-aa, aa, d + borderWidth);

    vec4 color = texture(backdrop, region.xy + qt_TexCoord0 * region.zw);
    color = fill + color * (1.0 - fill.a);
    color = border * (shape - inner) + color * (1.0 - border.a * (shape - inner));
    fragColor = color * shape * qt_Opacity;
}
