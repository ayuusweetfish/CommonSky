uniform vec2 screenoffs;
uniform vec2 arcopos;
uniform float arcor;

uniform vec2 tex_dims;
uniform vec2 seed;
uniform float time;
uniform float angle_cen, angle_span;

const float pi = acos(-1);

// https://thebookofshaders.com/11/

// Some useful functions
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }

//
// Description : GLSL 2D simplex noise function
//      Author : Ian McEwan, Ashima Arts
//  Maintainer : ijm
//     Lastmod : 20110822 (ijm)
//     License :
//  Copyright (C) 2011 Ashima Arts. All rights reserved.
//  Distributed under the MIT License. See LICENSE file.
//  https://github.com/ashima/webgl-noise
//
float snoise(vec2 v) {

    // Precompute values for skewed triangular grid
    const vec4 C = vec4(0.211324865405187,
                        // (3.0-sqrt(3.0))/6.0
                        0.366025403784439,
                        // 0.5*(sqrt(3.0)-1.0)
                        -0.577350269189626,
                        // -1.0 + 2.0 * C.x
                        0.024390243902439);
                        // 1.0 / 41.0

    // First corner (x0)
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);

    // Other two corners (x1, x2)
    vec2 i1 = vec2(0.0);
    i1 = (x0.x > x0.y)? vec2(1.0, 0.0):vec2(0.0, 1.0);
    vec2 x1 = x0.xy + C.xx - i1;
    vec2 x2 = x0.xy + C.zz;

    // Do some permutations to avoid
    // truncation effects in permutation
    i = mod289(i);
    vec3 p = permute(
            permute( i.y + vec3(0.0, i1.y, 1.0))
                + i.x + vec3(0.0, i1.x, 1.0 ));

    vec3 m = max(0.5 - vec3(
                        dot(x0,x0),
                        dot(x1,x1),
                        dot(x2,x2)
                        ), 0.0);

    m = m*m ;
    m = m*m ;

    // Gradients:
    //  41 pts uniformly over a line, mapped onto a diamond
    //  The ring size 17*17 = 289 is close to a multiple
    //      of 41 (41*7 = 287)

    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;

    // Normalise gradients implicitly by scaling m
    // Approximation of: m *= inversesqrt(a0*a0 + h*h);
    m *= 1.79284291400159 - 0.85373472095314 * (a0*a0+h*h);

    // Compute final noise value at P
    vec3 g = vec3(0.0);
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * vec2(x1.x,x2.x) + h.yz * vec2(x1.y,x2.y);
    return 130.0 * dot(m, g);
}

float lerp(float x, float a, float b)
{
  return a + (b - a) * x;
}

vec4 effect(vec4 color, Image tex, vec2 texture_coords, vec2 screen_coords) {
  screen_coords -= screenoffs;
  vec4 c = Texel(tex, texture_coords) * color;
  float headroom = 0.5;
  float rate = 1;
  float noiserate = 1 + headroom;
  // Edges
  float eps = 10;
  vec2 texpixcoord = texture_coords * tex_dims;
  if (texpixcoord.x < eps) rate *= texpixcoord.x / eps;
  if (texpixcoord.x > tex_dims.x - eps) rate *= (tex_dims.x - texpixcoord.x) / eps;
  if (texpixcoord.y < eps) rate *= texpixcoord.y / eps;
  if (texpixcoord.y > tex_dims.y - eps) rate *= (tex_dims.y - texpixcoord.y) / eps;
  // Entering: Angle proximity
  float arcoangle = atan(
    (screen_coords - arcopos).y,
    (screen_coords - arcopos).x
  );
  arcoangle = mod(arcoangle + pi * 1.5, pi * 2) - pi * 1.5;
  float inanglerange = clamp(
    (angle_span * 2 + 0.2) * (1 - exp(-2.3 * time)) - 0.1,
    -0.1, pi);
  float inanglerate =
    1 - smoothstep(inanglerange, inanglerange + 0.1,
      abs(angle_cen - arcoangle)) * (1 - 0.12 * clamp((time - 2) / 3, 0, 1));
  rate *= inanglerate;
  // Erosion
  float arcodist = abs(arcor - length(screen_coords - arcopos));
  float erosion = smoothstep(0, 1, (time - 2) / 3 + arcodist / 1000);
  if (erosion > 0) {
    // Arco proximity
    float x = arcodist / 60;
    float arcorate = exp(-x * x);
    // Angle proximity
    float outanglerate = 1 - smoothstep(
      angle_span, angle_span + 0.1, abs(angle_cen - arcoangle));
    // Noise rate w.r.t. minimum
    float noiseratemin = lerp(exp(-arcodist / 300), 0.06, 0.12);
    noiserate *= lerp(erosion, 1,
      noiseratemin + min(arcorate, outanglerate) * (1 - noiseratemin));
  }
  // Clamp angle
  noiserate *= clamp(-arcoangle / 0.03 + 1, 0, 1);
  noiserate *= clamp((pi + arcoangle) / 0.03 + 1, 0, 1);
  // Final processing
  float noiseval = (snoise(seed + screen_coords / 15) + 1) / 2;
  c.a = clamp((noiserate - noiseval) / headroom, 0, 1);
  c.a *= rate;
  return c;
}
