/*
* Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef BLAST_FAMILY_MODEL_SIMPLE_H
#define BLAST_FAMILY_MODEL_SIMPLE_H

#include "BlastFamily.h"
#include "BlastAssetModelSimple.h"

class SimpleRenderMesh;
class Renderable;
class Renderer;

class BlastFamilyModelSimple : public BlastFamily
{
public:
	//////// ctor ////////

	BlastFamilyModelSimple(PhysXController& physXController, ExtPxManager& pxManager, Renderer& renderer, const BlastAssetModelSimple& blastAsset, const BlastAsset::ActorDesc& desc);
	virtual ~BlastFamilyModelSimple();

protected:
	//////// abstract implementation ////////

	virtual void onActorCreated(const ExtPxActor& actor);
	virtual void onActorUpdate(const ExtPxActor& actor);
	virtual void onActorDestroyed(const ExtPxActor& actor);
	virtual void onActorHealthUpdate(const ExtPxActor& pxActor);

private:
	//////// internal data ////////

	Renderer& m_renderer;

	struct Chunk
	{
		std::vector<SimpleRenderMesh*> renderMeshes;
		std::vector<Renderable*> renderables;
	};

	std::vector<Chunk> m_chunks;
};


#endif //BLAST_FAMILY_MODEL_SIMPLE_H