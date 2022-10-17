#version 330 core
in vec2 fragPos;
out vec4 fragColor;

uniform sampler2D tex;

void main() {
  fragColor = texture(tex, (fragPos + 1) / 2);
}
