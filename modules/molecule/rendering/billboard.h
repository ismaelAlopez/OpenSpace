#pragma once

#include <glbinding/gl/types.h>
#include <glm/glm.hpp>

void billboardGlInit();
void billboardGlDeinit();

void billboardDraw(glm::mat4 const& transform, gl::GLuint colorTex, glm::vec4 const& stroke, float width, float depth);