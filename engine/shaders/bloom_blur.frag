#version 330 core
in vec2 vUV;
uniform sampler2D uImage;
uniform vec2 uDirection;
out vec4 FragColor;

void main() {
    const float weights[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = texture(uImage, vUV).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 off = uDirection * float(i) * 1.4;
        result += texture(uImage, vUV + off).rgb * weights[i];
        result += texture(uImage, vUV - off).rgb * weights[i];
    }
    FragColor = vec4(result, 1.0);
}
