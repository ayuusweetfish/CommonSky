#version 330 core
layout (location = 0) in vec2 vertPos;

out vec2 fragPos;

uniform vec3 viewCen;

void main() {
  gl_Position = vec4(vertPos, 0, 1);
  fragPos = vertPos;
}
