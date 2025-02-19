#ifndef RTSM_COMMON_GLSL
#define RTSM_COMMON_GLSL

#include "common.glsl"

const int RTSM_PAGE_TBL_SIZE = 32;  // small for testing, 64 can be better
const int RTSM_PAGE_MIPS     = 16;

// utility
uint floatToOrderedUint(float value) {
  uint uvalue = floatBitsToUint(value);
  uint mask = -int(uvalue >> 31) | 0x80000000;
  return uvalue ^ mask;
  }

float orderedUintToFloat(uint value) {
  uint mask = ((value >> 31) - 1) | 0x80000000;
  return uintBitsToFloat(value ^ mask);
  }

vec4 orderedUintToFloat(uvec4 value) {
  vec4 r;
  r.x = orderedUintToFloat(value.x);
  r.y = orderedUintToFloat(value.y);
  r.z = orderedUintToFloat(value.z);
  r.w = orderedUintToFloat(value.w);
  return r;
  }

bool bboxIntersect(vec4 a, vec4 b) {
  if(b.z < a.x || b.x > a.z)
    return false;
  if(b.w < a.y || b.y > a.w)
    return false;
  return true;
  }

//
bool planetOcclusion(float viewPos, vec3 sunDir) {
  const float y = RPlanet + max(viewPos*0.1, 0);
  if(rayIntersect(vec3(0,y,0), sunDir, RPlanet)>0)
    return true;
  return false;
  }

#endif
