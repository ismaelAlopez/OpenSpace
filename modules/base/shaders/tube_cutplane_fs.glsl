/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2023                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include "fragment.glsl"

in float vs_depth;
in vec3 vs_normal;
in vec4 vs_positionViewSpace;
in float vs_value;
in vec2 vs_st_prev;
in vec2 vs_st_next;

uniform float opacity;
uniform vec3 color;

uniform vec4 nanColor = vec4(0.5);
uniform bool useNanColor = true;

uniform vec4 aboveRangeColor;
uniform bool useAboveRangeColor;

uniform vec4 belowRangeColor;
uniform bool useBelowRangeColor;

uniform bool useColorMap;
uniform sampler1D colorMapTexture;
uniform float cmapRangeMin;
uniform float cmapRangeMax;
uniform bool hideOutsideRange;

uniform bool performShading = true;
uniform float ambientIntensity;
uniform float diffuseIntensity;
uniform float specularIntensity;

uniform int nLightSources;
uniform vec3 lightDirectionsViewSpace[8];
uniform float lightIntensities[8];

// For interpolation of the value sent into the color map
uniform bool hasInterpolationTexture = false;
uniform bool useNearesNeighbor;
uniform float interpolationTime;
uniform sampler2DArray textures;
uniform int selectedChannel = 0;
uniform int prev_texture_index;
uniform int next_texture_index;

// Could be seperated into ambinet, diffuse and specular and passed in as uniforms
const vec3 LightColor = vec3(1.0);
const float SpecularPower = 100.0;


vec4 sampleColorMap(float dataValue) {
  if (useNanColor && isnan(dataValue)) {
    return nanColor;
  }

  bool isOutside = dataValue < cmapRangeMin || dataValue > cmapRangeMax;
  if (isnan(dataValue) || (hideOutsideRange && isOutside)) {
    discard;
  }

  if (useBelowRangeColor && dataValue < cmapRangeMin) {
    return belowRangeColor;
  }

  if (useAboveRangeColor && dataValue > cmapRangeMax) {
    return aboveRangeColor;
  }

  float t = (dataValue - cmapRangeMin) / (cmapRangeMax - cmapRangeMin);
  t = clamp(t, 0.0, 1.0);
  return texture(colorMapTexture, t);
}

Fragment getFragment() {
  if (opacity == 0.0) {
    discard;
  }

  Fragment frag;

  // Color map
  vec4 objectColor = vec4(1.0);
  if (useColorMap) {
    if (hasInterpolationTexture) {
      float t = interpolationTime;
      if (useNearesNeighbor) {
        // If t < 0.5 then t -> 0, which later become 100% prevTexture value
        // If instead t >= 0.5 then t -> 1, which later become 100% nextTexture value
        t = step(0.5, interpolationTime);
      }

      // Interpolate with linear interpolation
      float valuePrev = texture(textures, vec3(vs_st_prev, prev_texture_index))[selectedChannel];
      float valueNext = texture(textures, vec3(vs_st_next, next_texture_index))[selectedChannel];
      float value = t * valueNext + (1.0 - t) * valuePrev;

      value = clamp(value, 0.0, 1.0);
      objectColor = sampleColorMap(value);
    }
    else {
      objectColor = sampleColorMap(vs_value);
    }
  }
  else {
    objectColor.rgb = color;
  }
  //objectColor.rgb = texture(textures, vec3(vs_st_prev, prev_texture_index)).rgb;

  objectColor.a *= opacity;
  if (objectColor.a == 0.0) {
    discard;
  }

  if (performShading) {
    // Ambient light
    vec3 totalLightColor = ambientIntensity * LightColor * objectColor.rgb;
    vec3 viewDirection = normalize(vs_positionViewSpace.xyz);

    // Light sources
    for (int i = 0; i < nLightSources; ++i) {
      // Diffuse light
      vec3 lightDirection = lightDirectionsViewSpace[i];
      float diffuseFactor =  max(dot(vs_normal, lightDirection), 0.0);
      vec3 diffuseColor = diffuseIntensity * LightColor * diffuseFactor * objectColor.rgb;

      // Specular light
      vec3 reflectDirection = reflect(lightDirection, vs_normal);
      float specularFactor =
        pow(max(dot(viewDirection, reflectDirection), 0.0), SpecularPower);
      vec3 specularColor = specularIntensity * LightColor * specularFactor;

      // Total Light
      totalLightColor += lightIntensities[i] * (diffuseColor + specularColor);
    }
    frag.color.rgb = totalLightColor;
  }
  else {
    frag.color.rgb = objectColor.rgb;
  }

  frag.depth = vs_depth;
  frag.gPosition = vs_positionViewSpace;
  frag.gNormal = vec4(vs_normal, 0.0);
  frag.disableLDR2HDR = true;
  frag.color.a = opacity;

  return frag;
}
