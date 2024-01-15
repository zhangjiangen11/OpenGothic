#ifndef MATERIALS_COMMON_GLSL
#define MATERIALS_COMMON_GLSL

#include "common.glsl"
#include "scene.glsl"

#define DEBUG_DRAW 0

#if DEBUG_DRAW
#define DEBUG_DRAW_LOC   20
#define MAX_DEBUG_COLORS 10
const vec3 debugColors[MAX_DEBUG_COLORS] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };
#endif

#define MAX_NUM_SKELETAL_NODES 96
#define MAX_MORPH_LAYERS       4
#define MaxVert                64
#define MaxPrim                64
#define MaxInd                 (MaxPrim*3)

#define T_LANDSCAPE 0
#define T_OBJ       1
#define T_SKINING   2
#define T_MORPH     3
#define T_PFX       4

#define L_Scene    0
#define L_Matrix   1
#define L_MeshDesc L_Matrix
#define L_Bucket   2
#define L_Ibo      3
#define L_Vbo      4
#define L_Diffuse  5
#define L_Shadow0  6
#define L_Shadow1  7
#define L_MorphId  8
#define L_Pfx      L_MorphId
#define L_Morph    9
#define L_SceneClr 10
#define L_GDepth   11
#define L_HiZ      12
#define L_SkyLut   13
#define L_Payload  14
#define L_Sampler  15

#define PfxOrientationNone       0
#define PfxOrientationVelocity   1
#define PfxOrientationVelocity3d 2

#if (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING || MESH_TYPE==T_MORPH)
#define LVL_OBJECT 1
#endif

#if defined(HIZ) || defined(SHADOW_MAP)
#define DEPTH_ONLY 1
#endif

#if !defined(DEPTH_ONLY) || defined(ATEST)
#define MAT_UV 1
#endif

#if !defined(DEPTH_ONLY) && (MESH_TYPE==T_PFX)
#define MAT_COLOR 1
#endif

#if defined(MAT_UV) || !defined(DEPTH_ONLY) || defined(FORWARD) || defined(MAT_COLOR)
#define MAT_VARYINGS 1
#endif

struct Varyings {
#if defined(MAT_UV)
  vec2 uv;
#endif

#if !defined(DEPTH_ONLY)
  vec3 normal;
#endif

#if defined(FORWARD) || (MESH_TYPE==T_LANDSCAPE)
  vec3 pos;
#endif

#if defined(MAT_COLOR)
  vec4 color;
#endif

#if !defined(MAT_VARYINGS)
  float dummy; // GLSL has no support for empty structs
#endif
  };

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

struct Particle {
  vec3  pos;
  uint  color;
  vec3  size;
  uint  bits0;
  vec3  dir;
  uint  colorB;
  };

struct MorphDesc {
  uint  indexOffset;
  uint  sample0;
  uint  sample1;
  uint  alpha16_intensity16;
  };

struct Instance {
  mat4x3 mat;
  float  fatness;
  uint   animPtr;
  uint   padd0;
  uint   padd1;
  };

struct Payload {
  uint  baseId;
  uint  offsets[64];
  //uvec4 offsets;
  };

struct IndirectCmd {
  uint vertexCount;
  uint instanceCount;
  uint firstVertex;
  uint firstInstance;
  uint writeOffset;
  };

struct LandscapeCluster {
  vec4  sphere;
  uint  commandId;
  uint  bucketId;
  uint  padd0;
  uint  padd1;
  };

#if defined(CLUSTER)
layout(push_constant, std430) uniform UboPush {
  uint      firstMeshlet;
  int       meshletCount;
  } push;
#elif (MESH_TYPE==T_LANDSCAPE)
layout(push_constant, std430) uniform UboPush {
  uint      firstMeshlet;
  int       meshletCount;
  } push;
#elif (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING) || (MESH_TYPE==T_MORPH)
layout(push_constant, std430) uniform UboPush {
  uint      firstMeshlet;
  int       meshletCount;
  uint      firstInstance;
  uint      instanceCount;
  } push;
#elif (MESH_TYPE==T_PFX)
// no push
#else
#error "unknown MESH_TYPE"
#endif

layout(binding = L_Scene, std140) uniform UboScene {
  SceneDesc scene;
  };

#if defined(LVL_OBJECT) && (defined(GL_VERTEX_SHADER) || defined(MESH) || defined(TASK) || defined(CLUSTER))
// NOTE: need to support binding overlap
layout(binding = L_Matrix, std430) readonly buffer InstanceMem { uint instanceMem[]; };

mat4 pullMatrix(uint i) {
  i *= 16;
  mat4 ret;
  ret[0][0] = uintBitsToFloat(instanceMem[i+0]);
  ret[0][1] = uintBitsToFloat(instanceMem[i+1]);
  ret[0][2] = uintBitsToFloat(instanceMem[i+2]);
  ret[0][3] = uintBitsToFloat(instanceMem[i+3]);
  ret[1][0] = uintBitsToFloat(instanceMem[i+4]);
  ret[1][1] = uintBitsToFloat(instanceMem[i+5]);
  ret[1][2] = uintBitsToFloat(instanceMem[i+6]);
  ret[1][3] = uintBitsToFloat(instanceMem[i+7]);
  ret[2][0] = uintBitsToFloat(instanceMem[i+8]);
  ret[2][1] = uintBitsToFloat(instanceMem[i+9]);
  ret[2][2] = uintBitsToFloat(instanceMem[i+10]);
  ret[2][3] = uintBitsToFloat(instanceMem[i+11]);
  ret[3][0] = uintBitsToFloat(instanceMem[i+12]);
  ret[3][1] = uintBitsToFloat(instanceMem[i+13]);
  ret[3][2] = uintBitsToFloat(instanceMem[i+14]);
  ret[3][3] = uintBitsToFloat(instanceMem[i+15]);
  return ret;
  }

Instance pullInstance(uint i) {
#if defined(LVL_OBJECT)
  i += push.firstInstance;
#endif
  i *= 16;
  Instance ret;
  ret.mat[0][0] = uintBitsToFloat(instanceMem[i+0]);
  ret.mat[0][1] = uintBitsToFloat(instanceMem[i+1]);
  ret.mat[0][2] = uintBitsToFloat(instanceMem[i+2]);
  ret.mat[1][0] = uintBitsToFloat(instanceMem[i+3]);
  ret.mat[1][1] = uintBitsToFloat(instanceMem[i+4]);
  ret.mat[1][2] = uintBitsToFloat(instanceMem[i+5]);
  ret.mat[2][0] = uintBitsToFloat(instanceMem[i+6]);
  ret.mat[2][1] = uintBitsToFloat(instanceMem[i+7]);
  ret.mat[2][2] = uintBitsToFloat(instanceMem[i+8]);
  ret.mat[3][0] = uintBitsToFloat(instanceMem[i+9]);
  ret.mat[3][1] = uintBitsToFloat(instanceMem[i+10]);
  ret.mat[3][2] = uintBitsToFloat(instanceMem[i+11]);
  ret.fatness   = uintBitsToFloat(instanceMem[i+12]);
  ret.animPtr   = instanceMem[i+13];
  return ret;
  }

MorphDesc pullMorphDesc(uint i) {
  i *= 4;
  MorphDesc ret;
  ret.indexOffset         = instanceMem[i+0];
  ret.sample0             = instanceMem[i+1];
  ret.sample1             = instanceMem[i+2];
  ret.alpha16_intensity16 = instanceMem[i+3];
  return ret;
  }

vec3 pullPosition(uint instanceId) {
#if defined(LVL_OBJECT)
  return pullInstance(instanceId).mat[3];
#else
  return vec3(0);
#endif
  }
#endif

#if (MESH_TYPE==T_LANDSCAPE) && (defined(TASK) || defined(CLUSTER))
layout(binding = L_MeshDesc, std430) readonly buffer Inst { LandscapeCluster clusters[]; };

LandscapeCluster pullCluster(const uint instanceId) {
  return clusters[instanceId];
  }
#endif

#if (defined(LVL_OBJECT) || defined(WATER))
layout(binding = L_Bucket, std140) uniform BucketDesc {
  vec4  bbox[2];
  ivec2 texAniMapDirPeriod;
  float bboxRadius;
  float waveMaxAmplitude;
  float alphaWeight;
  float envMapping;
  } bucket;
#elif (MESH_TYPE==T_LANDSCAPE)
layout(binding = L_Bucket, std430) buffer BucketDesc {
  IndirectCmd cmd[];
  };
#endif

#if defined(MESH) || defined(CLUSTER)
layout(binding = L_Ibo, std430) readonly buffer Ibo  { uint  indexes []; };
layout(binding = L_Vbo, std430) readonly buffer Vbo  { float vertices[]; };
#endif

#if defined(GL_FRAGMENT_SHADER) && !(defined(DEPTH_ONLY) && !defined(ATEST))
#  if defined(BINDLESS)
layout(binding = L_Diffuse) uniform texture2D textureD[];
layout(binding = L_Sampler) uniform sampler   samplerMain;
#  else
layout(binding = L_Diffuse) uniform sampler2D textureD;
  #endif
#endif

#if defined(GL_FRAGMENT_SHADER) && defined(FORWARD) && !defined(DEPTH_ONLY)
layout(binding = L_Shadow0) uniform sampler2D textureSm0;
layout(binding = L_Shadow1) uniform sampler2D textureSm1;
#endif

#if (MESH_TYPE==T_MORPH) && (defined(GL_VERTEX_SHADER) || defined(MESH))
layout(binding = L_MorphId, std430) readonly buffer SsboMorphId {
  int  index[];
  } morphId;
layout(binding = L_Morph, std430) readonly buffer SsboMorph {
  vec4 samples[];
  } morph;
#endif

#if (MESH_TYPE==T_PFX) && (defined(GL_VERTEX_SHADER) || defined(MESH))
layout(binding = L_Pfx, std430) readonly buffer SsboMorphId {
  Particle pfx[];
  };
#endif

#if defined(GL_FRAGMENT_SHADER) && (defined(WATER) || defined(GHOST))
layout(binding = L_SceneClr) uniform sampler2D sceneColor;
layout(binding = L_GDepth  ) uniform sampler2D gbufferDepth;
#endif

#if (defined(TASK) || defined(CULL)) && !defined(SHADOW_MAP)
layout(binding = L_HiZ)  uniform sampler2D hiZ;
#endif

#if defined(CULL) || defined(CLUSTER)
layout(binding = L_Payload, std430) buffer SsboGlob {
  uint meshlets[];
  } globalPayload;
#endif

#endif
