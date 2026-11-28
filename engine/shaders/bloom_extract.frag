#version 330 core
in vec2 vUV;
uniform sampler2D uHdrTex;
out vec4 FragColor;

void main() {
    vec3 col = texture(uHdrTex, vUV).rgb;
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    float soft = smoothstep(0.85, 1.65, lum);
    FragColor = vec4(col * soft, 1.0);
}
