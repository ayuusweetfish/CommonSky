#version 330 core
in vec2 fragPos;
out vec4 fragColor;

uniform float alpha;

void main() {
  fragColor = vec4(0.05, 0.05, 0.05, alpha);
}
