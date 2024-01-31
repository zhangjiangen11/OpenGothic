#pragma once

#include <Tempest/DescriptorSet>
#include <Tempest/RenderPipeline>
#include <Tempest/StorageBuffer>
#include <Tempest/Texture2d>

#include <cstddef>
#include <utility>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/bounds.h"
#include "graphics/material.h"
#include "graphics/sceneglobals.h"
#include "graphics/instancestorage.h"
#include "graphics/drawbuckets.h"
#include "graphics/drawclusters.h"

class StaticMesh;
class AnimMesh;
class VisualObjects;
class Camera;
class RtScene;

class DrawStorage {
  public:
    enum Type : uint8_t {
      Landscape,
      Static,
      Movable,
      Animated,
      Pfx,
      Morph,
      };

    class Item final {
      public:
        Item()=default;
        Item(DrawStorage& owner,size_t id)
            :owner(&owner),id(id){}
        Item(Item&& obj):owner(obj.owner),id(obj.id){
          obj.owner=nullptr;
          obj.id   =0;
          }
        Item& operator=(Item&& obj) {
          std::swap(obj.owner,owner);
          std::swap(obj.id,   id);
          return *this;
          }
        ~Item() {
          if(owner)
            owner->free(this->id);
          }

        bool     isEmpty() const { return owner==nullptr; }

        void     setObjMatrix(const Tempest::Matrix4x4& mt);
        void     setAsGhost  (bool g);
        void     setFatness  (float f);
        void     setWind     (phoenix::animation_mode m, float intensity);
        void     startMMAnim (std::string_view anim, float intensity, uint64_t timeUntil);

        const Material&    material() const;
        const Bounds&      bounds()   const;
        Tempest::Matrix4x4 position() const;
        const StaticMesh*  mesh()     const;
        std::pair<uint32_t,uint32_t> meshSlice() const;

      private:
        DrawStorage* owner = nullptr;
        size_t       id    = 0;
      };

    DrawStorage(VisualObjects& owner, const SceneGlobals& globals);
    ~DrawStorage();

    Item alloc(const AnimMesh& mesh, const Material& mat, const InstanceStorage::Id& anim,
               size_t iboOff, size_t iboLen);
    Item alloc(const StaticMesh& mesh, const Material& mat,
               size_t iboOff, size_t iboLen, Type type);
    Item alloc(const StaticMesh& mesh, const Material& mat,
               size_t iboOff, size_t iboLen, const PackedMesh::Cluster* cluster, Type type);

    void dbgDraw(Tempest::Painter& p, Tempest::Vec2 wsz);

    bool commit(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void preFrameUpdate(uint8_t fId);
    void prepareUniforms();
    void invalidateUbo();

    void visibilityPass (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int pass);
    void drawGBuffer    (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawShadow     (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, int layer);
    void drawTranslucent(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawWater      (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);
    void drawHiZ        (Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    void fillTlas(RtScene& out);

  private:
    enum TaskLinkpackage : uint8_t {
      T_Scene    = 0,
      T_Payload  = 1,
      T_Instance = 2,
      T_Bucket   = 3,
      T_Indirect = 4,
      T_Clusters = 5,
      T_HiZ      = 6,
      };

    enum UboLinkpackage : uint8_t {
      L_Scene    = 0,
      L_Payload  = 1,
      L_Instance = 2,
      L_Pfx      = L_Instance,
      L_Bucket   = 3,
      L_Ibo      = 4,
      L_Vbo      = 5,
      L_Diffuse  = 6,
      L_Sampler  = 7,
      L_Shadow0  = 8,
      L_Shadow1  = 9,
      L_MorphId  = 10,
      L_Morph    = 11,
      L_SceneClr = 12,
      L_GDepth   = 13,
      };

    using Bucket  = DrawBuckets::Bucket;
    using Cluster = DrawClusters::Cluster;

    struct MorphAnim {
      size_t   id        = 0;
      uint64_t timeStart = 0;
      uint64_t timeUntil = 0;
      float    intensity = 0;
      };

    struct Object {
      bool     isEmpty() const { return cmdId==uint16_t(-1); }

      Tempest::Matrix4x4  pos;
      InstanceStorage::Id objInstance;
      InstanceStorage::Id objMorphAnim;

      Type                type      = Type::Landscape;
      uint32_t            iboOff    = 0;
      uint32_t            iboLen    = 0;
      uint32_t            animPtr   = 0;
      DrawBuckets::Id     bucketId;
      uint16_t            cmdId     = uint16_t(-1);
      uint32_t            clusterId = 0;

      Material::AlphaFunc alpha = Material::Solid;
      MorphAnim           morphAnim[Resources::MAX_MORPH_LAYERS];
      phoenix::animation_mode wind = phoenix::animation_mode::none;
      float               windIntensity = 0;
      float               fatness   = 0;
      bool                isGhost   = false;
      };

    struct MorphDesc final {
      uint32_t indexOffset = 0;
      uint32_t sample0     = 0;
      uint32_t sample1     = 0;
      uint16_t alpha       = 0;
      uint16_t intensity   = 0;
      };

    struct MorphData {
      MorphDesc morph[Resources::MAX_MORPH_LAYERS];
      };

    struct InstanceDesc {
      void     setPosition(const Tempest::Matrix4x4& m);
      float    pos[4][3] = {};
      float    fatness   = 0;
      uint32_t animPtr   = 0;
      uint32_t padd0     = {};
      uint32_t padd1     = {};
      };

    struct IndirectCmd {
      uint32_t vertexCount   = 0;
      uint32_t instanceCount = 0;
      uint32_t firstVertex   = 0;
      uint32_t firstInstance = 0;
      uint32_t writeOffset   = 0;
      };

    struct TaskCmd {
      SceneGlobals::VisCamera        viewport = SceneGlobals::V_Main;
      Tempest::DescriptorSet         desc;
      };

    struct DrawCmd {
      const Tempest::RenderPipeline* pMain        = nullptr;
      const Tempest::RenderPipeline* pShadow      = nullptr;
      const Tempest::RenderPipeline* pHiZ         = nullptr;
      uint32_t                       bucketId     = 0; // bindfull only
      Type                           type         = Type::Landscape;

      Tempest::DescriptorSet         desc[SceneGlobals::V_Count];
      Material::AlphaFunc            alpha        = Material::Solid;
      uint32_t                       firstPayload = 0;
      uint32_t                       maxPayload   = 0;


      bool                           isForwardShading() const;
      bool                           isShadowmapRequired() const;
      bool                           isSceneInfoRequired() const;
      bool                           isTextureInShadowPass() const;
      };

    struct View {
      Tempest::DescriptorSet         descInit;
      Tempest::StorageBuffer         visClusters, indirectCmd;
      };

    void                           free(size_t id);
    void                           updateInstance(size_t id, Tempest::Matrix4x4* pos = nullptr);
    void                           updateRtAs(size_t id);

    void                           startMMAnim(size_t i, std::string_view animName, float intensity, uint64_t timeUntil);
    void                           setAsGhost(size_t i, bool g);

    void                           preFrameUpdateWind(uint8_t fId);
    void                           preFrameUpdateMorph(uint8_t fId);

    void                           drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, SceneGlobals::VisCamera view, Material::AlphaFunc func);
    static bool                    cmpDraw(const DrawCmd* l, const DrawCmd* r);

    bool                           commitCommands();
    void                           patchClusters(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId);

    size_t                         implAlloc();
    uint16_t                       commandId(const Material& m, Type type, uint32_t bucketId);
    uint32_t                       clusterId(const PackedMesh::Cluster* cluster, size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);
    uint32_t                       clusterId(const Bucket&  bucket,  size_t firstMeshlet, size_t meshletCount, uint16_t bucketId, uint16_t commandId);

    void                           dbgDraw(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const Cluster& c);
    void                           dbgDrawBBox(Tempest::Painter& p, Tempest::Vec2 wsz, const Camera& cam, const Cluster& c);

    VisualObjects&                 owner;
    const SceneGlobals&            scene;

    std::vector<Object>            objects;
    std::unordered_set<size_t>     objectsWind;
    std::unordered_set<size_t>     objectsMorph;
    std::unordered_set<size_t>     objectsFree;

    DrawClusters                   clusters;

    std::vector<TaskCmd>           tasks;
    std::vector<DrawCmd>           cmd;
    std::vector<DrawCmd*>          ord;
    size_t                         totalPayload = 0;
    bool                           cmdDurtyBit = false;
    View                           views[SceneGlobals::V_Count];
  };
