#include "resources.h"

#include <Tempest/MemReader>
#include <Tempest/Pixmap>
#include <Tempest/Device>
#include <Tempest/Dir>

#include <Tempest/Log>

#include <zenload/zCModelMeshLib.h>
#include <zenload/zCMorphMesh.h>
#include <zenload/zCProgMeshProto.h>
#include <zenload/ztex2dds.h>

#include <fstream>

#include "graphics/submesh/staticmesh.h"
#include "graphics/submesh/animmesh.h"
#include "graphics/skeleton.h"
#include "graphics/protomesh.h"
#include "gothic.h"

using namespace Tempest;

Resources* Resources::inst=nullptr;

Resources::Resources(Gothic &gothic, Tempest::Device &device)
  : device(device), asset("data",device),gothic(gothic) {
  inst=this;

  menuFnt = Font("data/font/menu.ttf");
  menuFnt.setPixelSize(44);

  mainFnt = Font("data/font/main.ttf");
  mainFnt.setPixelSize(24);

  fallback = device.loadTexture("data/fallback.png");

  // TODO: priority for mod files
  Dir::scan(gothic.path()+"Data/",[this](const std::string& vdf,Dir::FileType t){
    if(t==Dir::FT_File)
      gothicAssets.loadVDF(this->gothic.path() + "Data/" + vdf);
    });
  gothicAssets.finalizeLoad();
  }

Resources::~Resources() {
  inst=nullptr;
  }

VDFS::FileIndex& Resources::vdfsIndex() {
  return inst->gothicAssets;
  }

void Resources::addVdf(const char *vdf) {
  gothicAssets.loadVDF(gothic.path() + vdf);
  }

Tempest::Texture2d* Resources::implLoadTexture(const std::string& name) {
  if(name.size()==0)
    return nullptr;

  auto it=texCache.find(name);
  if(it!=texCache.end())
    return it->second.get();

  std::vector<uint8_t> data=getFileData(name);
  if(data.empty())
    return &fallback;

  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);

    std::unique_ptr<Texture2d> t{new Texture2d(device.loadTexture(pm))};
    Texture2d* ret=t.get();
    texCache[name] = std::move(t);
    return ret;
    }
  catch(...){
    Log::e("unable to load texture \"",name,"\"");
    return &fallback;
    }
  }

ProtoMesh* Resources::implLoadMesh(const std::string &name) {
  if(name.size()==0)
    return nullptr;

  auto it=aniMeshCache.find(name);
  if(it!=aniMeshCache.end())
    return it->second.get();

  try {
    ZenLoad::PackedMesh        sPacked;
    ZenLoad::zCModelMeshLib    library;
    auto                       code=loadMesh(sPacked,library,name);
    std::unique_ptr<ProtoMesh> t{code==MeshLoadCode::Static ? new ProtoMesh(sPacked) : new ProtoMesh(library)};
    ProtoMesh* ret=t.get();
    aniMeshCache[name] = std::move(t);
    if(code==MeshLoadCode::Error)
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load mesh \"",name,"\"");
    return nullptr;
    }
  }

Skeleton* Resources::implLoadSkeleton(std::string name) {
  if(name.size()==0)
    return nullptr;

  if(name.rfind(".MDS")==name.size()-4 ||
     name.rfind(".mds")==name.size()-4)
    std::memcpy(&name[name.size()-3],"MDH",3);

  auto it=skeletonCache.find(name);
  if(it!=skeletonCache.end())
    return it->second.get();

  try {
    ZenLoad::zCModelMeshLib library(name,gothicAssets,1.f);
    std::unique_ptr<Skeleton> t{new Skeleton(library)};
    Skeleton* ret=t.get();
    skeletonCache[name] = std::move(t);
    if(!hasFile(name))
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load skeleton \"",name,"\"");
    return nullptr;
    }
  }

bool Resources::hasFile(const std::string &fname) {
  return gothicAssets.hasFile(fname);
  }

Tempest::Texture2d* Resources::loadTexture(const char *name) {
  return inst->implLoadTexture(name);
  }

Tempest::Texture2d* Resources::loadTexture(const std::string &name) {
  return inst->implLoadTexture(name);
  }

const ProtoMesh *Resources::loadMesh(const std::string &name) {
  return inst->implLoadMesh(name);
  }

const Skeleton *Resources::loadSkeleton(const std::string &name) {
  return inst->implLoadSkeleton(name);
  }

std::vector<uint8_t> Resources::getFileData(const char *name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  return data;
  }

std::vector<uint8_t> Resources::getFileData(const std::string &name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  if(data.size()!=0)
    return data;

  if(name.rfind(".TGA")==name.size()-4){
    auto n = name;
    n.resize(n.size()-4);
    n+="-C.TEX";
    inst->gothicAssets.getFileData(n,data);
    std::vector<uint8_t> ztex;
    ZenLoad::convertZTEX2DDS(data,ztex);
    //ZenLoad::convertDDSToRGBA8(data, ztex);
    return ztex;
    }
  return data;
  }

Resources::MeshLoadCode Resources::loadMesh(ZenLoad::PackedMesh& sPacked, ZenLoad::zCModelMeshLib& library, std::string name) {
  std::vector<uint8_t> data;
  std::vector<uint8_t> dds;

  // Check if this isn't the compiled version
  if(name.rfind("-C")==std::string::npos) {
    // Strip the extension ".***"
    // Add "compiled"-extension
    if(name.rfind(".3DS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MRM",3); else
    if(name.rfind(".MMS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MMB",3); else
    if(name.rfind(".ASC")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MDL",3);
    }

  if(name.rfind(".MRM")==name.size()-4) {
    ZenLoad::zCProgMeshProto zmsh(name,gothicAssets);
    if(zmsh.getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmsh.packMesh(sPacked,1.f);
    return MeshLoadCode::Static;
    }

  if(name.rfind(".MMB")==name.size()-4) {
    ZenLoad::zCMorphMesh zmm(name,gothicAssets);
    if(zmm.getMesh().getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmm.getMesh().packMesh(sPacked,1.f);
    return MeshLoadCode::Dynamic;
    }

  if(name.rfind(".MDMS")==name.size()-5 ||
     name.rfind(".MDS") ==name.size()-4 ||
     name.rfind(".MDL") ==name.size()-4 ||
     name.rfind(".MDM") ==name.size()-4 ||
     name.rfind(".mds") ==name.size()-4){
    library = loadMDS(name);
    return MeshLoadCode::Dynamic;
    }

  return MeshLoadCode::Error;
  }

ZenLoad::zCModelMeshLib Resources::loadMDS(std::string &name) {
  if(name.rfind(".MDMS")==name.size()-5){
    name.resize(name.size()-1);
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
    }
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  std::memcpy(&name[name.size()-3],"MDL",3);
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  std::memcpy(&name[name.size()-3],"MDM",3);
  if(hasFile(name)) {
    ZenLoad::zCModelMeshLib lib(name,gothicAssets,1.f);
    std::memcpy(&name[name.size()-3],"MDH",3);
    if(hasFile(name)) {
      auto data=getFileData(name);
      if(!data.empty()){
        ZenLoad::ZenParser parser(data.data(), data.size());
        lib.loadMDH(parser,1.f);
        }
      }
    return lib;
    }
  std::memcpy(&name[name.size()-3],"MDH",3);
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  return ZenLoad::zCModelMeshLib();
  }
