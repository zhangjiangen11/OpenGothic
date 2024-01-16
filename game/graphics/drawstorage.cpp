#include "drawstorage.h"

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/visualobjects.h"
#include "shaders.h"

using namespace Tempest;

template<class T>
static const T& dummy() {
  static T t = {};
  return t;
  }

void DrawStorage::Item::setObjMatrix(const Tempest::Matrix4x4& pos) {
  if(owner!=nullptr) {
    owner->objects[id].pos = pos;
    owner->updateInstance(id);
    }
  }

void DrawStorage::Item::setAsGhost(bool g) {
  }

void DrawStorage::Item::setFatness(float f) {
  }

void DrawStorage::Item::setWind(phoenix::animation_mode m, float intensity) {
  }

void DrawStorage::Item::startMMAnim(std::string_view anim, float intensity, uint64_t timeUntil) {
  //assert(0);
  }

void DrawStorage::Item::setPfxData(const Tempest::StorageBuffer* ssbo, uint8_t fId) {
  assert(0);
  }

const Material& DrawStorage::Item::material() const {
  if(owner==nullptr)
    return dummy<Material>();
  auto b = owner->objects[id].bucketId;
  return owner->buckets[b].mat;
  }

const Bounds& DrawStorage::Item::bounds() const {
  if(owner==nullptr)
    return dummy<Bounds>();
  return dummy<Bounds>();
  }

Matrix4x4 DrawStorage::Item::position() const {
  if(owner!=nullptr)
    return owner->objects[id].pos;
  return dummy<Matrix4x4>();
  }

const StaticMesh* DrawStorage::Item::mesh() const {
  if(owner==nullptr)
    return nullptr;
  auto b = owner->objects[id].bucketId;
  return owner->buckets[b].staticMesh;
  }

std::pair<uint32_t, uint32_t> DrawStorage::Item::meshSlice() const {
  if(owner==nullptr)
    return std::pair<uint32_t, uint32_t>(0,0);
  auto& obj = owner->objects[id];
  return std::make_pair(obj.iboOff, obj.iboLen);
  }


void DrawStorage::InstanceDesc::setPosition(const Tempest::Matrix4x4& m) {
  for(int i=0; i<4; ++i)
    for(int r=0; r<3; ++r)
      pos[i][r] = m.at(i,r);
  }


DrawStorage::DrawStorage(VisualObjects& owner, const SceneGlobals& globals) : owner(owner), scene(globals) {
  }

DrawStorage::~DrawStorage() {
  }

DrawStorage::Item DrawStorage::alloc(const StaticMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster, Type type) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  const size_t id = implAlloc();

  Object& obj = objects[id];

  obj.type      = Type::Landscape;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketId(mat, mesh);
  obj.cmdId     = commandId(mat, type, obj.bucketId);
  obj.clusterId = clusterId(cluster, iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId, obj.cmdId);

  if(obj.isEmpty())
    return Item(); // null command
  return Item(*this, id);
  }

DrawStorage::Item DrawStorage::alloc(const StaticMesh& mesh, const Material& mat,
                                     size_t iboOff, size_t iboLen, Type type) {
  // return Item();
  // 64x64 meshlets
  assert(iboOff%PackedMesh::MaxInd==0);
  assert(iboLen%PackedMesh::MaxInd==0);

  const size_t id = implAlloc();

  Object& obj = objects[id];

  obj.type      = type;
  obj.iboOff    = uint32_t(iboOff);
  obj.iboLen    = uint32_t(iboLen);
  obj.bucketId  = bucketId(mat, mesh);
  obj.cmdId     = commandId(mat, type, obj.bucketId);
  obj.clusterId = clusterId(buckets[obj.bucketId], iboOff/PackedMesh::MaxInd, iboLen/PackedMesh::MaxInd, obj.bucketId, obj.cmdId);
  if(obj.isEmpty())
    return Item(); // null command

  obj.objInstance = owner.alloc(sizeof(InstanceDesc));
  clusters[obj.clusterId].instanceId = obj.objInstance.offsetId<InstanceDesc>();

  return Item(*this, id);
  }

DrawStorage::Item DrawStorage::alloc(const AnimMesh& mesh, const Material& mat, const InstanceStorage::Id& anim,
                                     size_t iboOff, size_t iboLen, Type bucket) {
  return Item();
  }

void DrawStorage::free(size_t id) {
  commited = false;

  Object& obj = objects[id];
  const uint32_t meshletCount = (obj.iboLen/PackedMesh::MaxInd);
  cmd[obj.cmdId].maxPayload -= meshletCount;

  const uint32_t numCluster = (obj.type==Landscape ? meshletCount : 1);
  for(size_t i=0; i<numCluster; ++i) {
    clusters[obj.clusterId + i]              = Cluster();
    clusters[obj.clusterId + i].r            = -1;
    clusters[obj.clusterId + i].meshletCount = 0;
    }

  obj = Object();
  while(objects.size()>0) {
    if(!objects.back().isEmpty())
      break;
    objects.pop_back();
    }
  }

void DrawStorage::updateInstance(size_t id) {
  auto& obj = objects[id];
  if(obj.type==Landscape)
    return;

  InstanceDesc d;
  d.setPosition(obj.pos);
  obj.objInstance.set(&d, 0, sizeof(d));

  auto cId = objects[id].clusterId;
  clusters[cId].pos = Vec3(obj.pos[3][0], obj.pos[3][1], obj.pos[3][2]);

  commited = false;
  }

void DrawStorage::commit() {
  if(commited)
    return;
  commited = true;

  if(cmd.size()==0)
    return;

  totalPayload = 0;
  for(auto& i:cmd) {
    i.firstPayload = uint32_t(totalPayload);
    totalPayload  += i.maxPayload;
    }

  std::vector<IndirectCmd> cx(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i) {
    cx[i].vertexCount = PackedMesh::MaxInd;
    cx[i].writeOffset = cmd[i].firstPayload;
    }

  ord.resize(cmd.size());
  for(size_t i=0; i<cmd.size(); ++i)
    ord[i] = &cmd[i];
  std::sort(ord.begin(), ord.end(), [](const DrawCmd* l, const DrawCmd* r){
    return l->alpha < r->alpha;
    });

  auto& device = Resources::device();
  device.waitIdle();

  clustersGpu  = device.ssbo(clusters);
  tasks.clear();
  for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
    TaskCmd cmd;
    cmd.viewport = SceneGlobals::VisCamera(v);
    tasks.emplace_back(std::move(cmd));
    }

  for(auto& v:views) {
    v.visClusters = device.ssbo(nullptr, totalPayload*sizeof(uint32_t)*4);
    v.indirectCmd = device.ssbo(cx.data(), sizeof(IndirectCmd)*cx.size());

    v.descInit = device.descriptors(Shaders::inst().clusterInit);
    v.descInit.set(T_Indirect, v.indirectCmd);
    }

  for(auto& cmd:tasks) {
    if(cmd.viewport==SceneGlobals::V_Main)
      cmd.desc = device.descriptors(Shaders::inst().clusterTaskHiZ); else
      cmd.desc = device.descriptors(Shaders::inst().clusterTask);
    cmd.desc.set(T_Clusters, clustersGpu);
    cmd.desc.set(T_Indirect, views[cmd.viewport].indirectCmd);
    cmd.desc.set(T_Payload,  views[cmd.viewport].visClusters);
    }
  }

void DrawStorage::prepareUniforms() {
  if(cmd.size()==0)
    return;

  commit();

  std::vector<BucketGpu> bucket;
  for(auto& i:buckets) {
    BucketGpu bx;
    bx.texAniMapDirPeriod = i.mat.texAniMapDirPeriod;
    bx.waveMaxAmplitude   = i.mat.waveMaxAmplitude;
    bx.alphaWeight        = i.mat.alphaWeight;
    bx.envMapping         = i.mat.envMapping;
    if(i.staticMesh!=nullptr) {
      auto& bbox    = i.staticMesh->bbox.bbox;
      bx.bboxRadius = i.staticMesh->bbox.rConservative;
      bx.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
      bx.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
      }
    else if(i.animMesh!=nullptr) {
      auto& bbox    = i.animMesh->bbox.bbox;
      bx.bboxRadius = i.animMesh->bbox.rConservative;
      bx.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
      bx.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
      }
    bucket.push_back(bx);
    }

  auto& device = Resources::device();
  device.waitIdle();
  bucketsGpu = device.ssbo(bucket);

  invalidateUbo();
  }

void DrawStorage::invalidateUbo() {
  if(owner.instanceSsbo().isEmpty())
    return;

  auto& device = Resources::device();
  device.waitIdle();

  std::vector<const Tempest::Texture2d*>     tex;
  std::vector<const Tempest::StorageBuffer*> vbo, ibo;
  for(auto& i:buckets) {
    tex.push_back(i.mat.tex);
    ibo.push_back(&i.staticMesh->ibo8);
    vbo.push_back(&i.staticMesh->vbo);
    }

  for(auto& i:tasks) {
    i.desc.set(T_Scene,    scene.uboGlobal[i.viewport]);
    i.desc.set(T_Instance, owner.instanceSsbo());
    i.desc.set(T_HiZ,      *scene.hiZ);
    }

  for(auto& i:cmd) {
    for(uint8_t v=0; v<SceneGlobals::V_Count; ++v) {
      if(i.desc[v].isEmpty())
        continue;
      auto& mem = (i.type==Type::Landscape) ? clustersGpu : owner.instanceSsbo();

      i.desc[v].set(L_Scene,    scene.uboGlobal[v]);
      i.desc[v].set(L_Instance, mem);
      i.desc[v].set(L_Ibo,      ibo);
      i.desc[v].set(L_Vbo,      vbo);
      i.desc[v].set(L_Diffuse,  tex);
      i.desc[v].set(L_Bucket,   bucketsGpu);
      i.desc[v].set(L_Payload,  views[v].visClusters);
      i.desc[v].set(L_Sampler,  Sampler::anisotrophy());
      }
    }
  }

void DrawStorage::visibilityPass(Encoder<CommandBuffer>& cmd, uint8_t frameId) {
  static bool freeze = false;
  if(freeze)
    return;

  for(auto& v:views) {
    if(this->cmd.empty())
      continue;
    cmd.setUniforms(Shaders::inst().clusterInit, v.descInit);
    cmd.dispatchThreads(this->cmd.size());
    }

  for(auto& i:tasks) {
    struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};
    push.firstMeshlet = 0;
    push.meshletCount = uint32_t(clusters.size());

    auto& pso = (i.viewport==SceneGlobals::V_Main) ? Shaders::inst().clusterTaskHiZ : Shaders::inst().clusterTask;
    cmd.setUniforms(pso, i.desc, &push, sizeof(push));
    cmd.dispatchThreads(push.meshletCount);
    }
  }

void DrawStorage::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t frameId) {
  //return;
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto  viewId = SceneGlobals::V_Main;
  auto& main   = views[viewId];
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.desc[viewId].isEmpty())
      continue;
    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    cmd.setUniforms(*cx.psoColor, cx.desc[viewId], &push, sizeof(push));
    cmd.drawIndirect(main.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

void DrawStorage::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer) {
  //return;
  struct Push { uint32_t firstMeshlet; uint32_t meshletCount; } push = {};

  auto  viewId = (SceneGlobals::V_Shadow0+layer);
  auto& view   = views[viewId];
  for(size_t i=0; i<ord.size(); ++i) {
    auto& cx = *ord[i];
    if(cx.desc[viewId].isEmpty())
      continue;
    auto id  = size_t(std::distance(this->cmd.data(), &cx));
    push.firstMeshlet = cx.firstPayload;
    push.meshletCount = cx.maxPayload;

    cmd.setUniforms(*cx.psoDepth, cx.desc[viewId], &push, sizeof(push));
    cmd.drawIndirect(view.indirectCmd, sizeof(IndirectCmd)*id);
    }
  }

const RenderPipeline* DrawStorage::pipelineColor(const Material& m, Type type) {
  auto& s = Shaders::inst();
  switch(m.alpha) {
    case Material::Solid: {
      switch(type) {
        case Landscape:
          return &s.clusterLndGBuf;
        case Static:
          return &s.clusterObjGBuf;
        case Movable:
        case Animated:
        case Pfx:
        case Morph:
          break;
        }
      }
    case Material::AlphaTest: {
      switch(type) {
        case Landscape:
          return &s.clusterLndGBufAt;
        case Static:
          return &s.clusterObjGBufAt;
        case Movable:
        case Animated:
        case Pfx:
        case Morph:
          break;
        }
      }
    case Material::Water:
    case Material::Ghost:
    case Material::Multiply:
    case Material::Multiply2:
    case Material::Transparent:
    case Material::AdditiveLight:
      break;
    }
  return nullptr;
  }

const RenderPipeline* DrawStorage::pipelineDepth(const Material& m, Type type) {
  auto& s = Shaders::inst();
  switch(m.alpha) {
    case Material::Solid:
      return &s.clusterDepth;
    case Material::AlphaTest:
      return &s.clusterDepthAt;
    case Material::Water:
    case Material::Ghost:
    case Material::Multiply:
    case Material::Multiply2:
    case Material::Transparent:
    case Material::AdditiveLight:
      break;
    }
  return nullptr;
  }

size_t DrawStorage::implAlloc() {
  commited = false;

  for(size_t i=0; i<objects.size(); ++i) {
    if(objects[i].isEmpty()) {
      return i;
      }
    }

  objects.resize(objects.size()+1);
  return objects.size()-1;
  }

uint16_t DrawStorage::bucketId(const Material& mat, const StaticMesh& mesh) {
  for(size_t i=0; i<buckets.size(); ++i) {
    auto& b = buckets[i];
    if(b.staticMesh==&mesh && b.mat==mat)
      return uint16_t(i);
    }

  Bucket bx;
  bx.staticMesh = &mesh;
  bx.mat        = mat;
  buckets.emplace_back(std::move(bx));
  return uint16_t(buckets.size()-1);
  }

uint16_t DrawStorage::bucketId(const Material& mat, const AnimMesh& mesh) {
  for(size_t i=0; i<buckets.size(); ++i) {
    auto& b = buckets[i];
    if(b.animMesh==&mesh && b.mat==mat)
      return uint16_t(i);
    }

  Bucket bx;
  bx.animMesh = &mesh;
  bx.mat      = mat;
  buckets.emplace_back(std::move(bx));
  return uint16_t(buckets.size()-1);
  }

uint16_t DrawStorage::commandId(const Material& m, Type type, uint32_t bucketId) {
  auto pMain  = pipelineColor(m, type);
  auto pDepth = pipelineDepth(m, type);
  if(pMain==nullptr && pDepth==nullptr)
    return uint16_t(-1);

  const bool bindless = true;

  for(size_t i=0; i<cmd.size(); ++i) {
    if(cmd[i].psoColor!=pMain || cmd[i].psoDepth!=pDepth)
      continue;
    if(!bindless && cmd[i].bucketId != bucketId)
      continue;
    return uint16_t(i);
    }

  auto ret = uint16_t(cmd.size());

  auto& device = Resources::device();
  DrawCmd cx;
  cx.psoColor    = pMain;
  cx.psoDepth    = pDepth;
  cx.bucketId    = bindless ? 0 : bucketId;
  cx.type        = type;
  cx.alpha       = m.alpha;
  if(cx.psoColor!=nullptr) {
    cx.desc[SceneGlobals::V_Main] = device.descriptors(*cx.psoColor);
    }
  if(cx.psoDepth!=nullptr) {
    cx.desc[SceneGlobals::V_Shadow0] = device.descriptors(*cx.psoDepth);
    cx.desc[SceneGlobals::V_Shadow1] = device.descriptors(*cx.psoDepth);
    }
  cmd.push_back(std::move(cx));
  return ret;
  }

uint32_t DrawStorage::clusterId(const PackedMesh::Cluster* cx, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId) {
  if(commandId==uint16_t(-1))
    return uint32_t(-1);

  auto ret = clusters.size();
  clusters.resize(ret + meshletCount);

  for(size_t i=0; i<meshletCount; ++i) {
    Cluster c;
    c.pos          = cx[i].pos;
    c.r            = cx[i].r;
    c.bucketId     = bucketId;
    c.commandId    = commandId;
    c.firstMeshlet = uint32_t(firstMeshlet + i);
    c.meshletCount = 1;
    c.instanceId   = uint32_t(-1);

    clusters[ret+i] = c;
    }


  cmd[commandId].maxPayload  += uint32_t(meshletCount);
  return uint32_t(ret);
  }

uint32_t DrawStorage::clusterId(const Bucket& bucket, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId) {
  if(commandId==uint16_t(-1))
    return uint32_t(-1);

  auto ret = clusters.size();
  clusters.resize(ret + 1);

  Cluster& c = clusters.back();
  c.r            = bucket.staticMesh->bbox.rConservative; // TODO: ani-mesh
  c.bucketId     = bucketId;
  c.commandId    = commandId;
  c.firstMeshlet = uint32_t(firstMeshlet);
  c.meshletCount = uint32_t(meshletCount);
  c.instanceId   = uint32_t(-1);

  cmd[commandId].maxPayload  += uint32_t(meshletCount);
  return uint32_t(ret);
  }

