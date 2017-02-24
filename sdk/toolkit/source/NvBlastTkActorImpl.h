/*
* Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef NVBLASTTKACTORIMPL_H
#define NVBLASTTKACTORIMPL_H


#include "NvBlastTkCommon.h"

#include "NvBlastAssert.h"
#include "NvBlastDLink.h"
#include "NvBlastIteratorBase.h"

#include "NvBlastTkJointImpl.h"

#include "NvBlast.h"

#include "NvBlastTkActor.h"

#include "PxFlags.h"

namespace Nv
{
namespace Blast
{

// Forward declarations:
class TkGroupImpl;
class TkFamilyImpl;
class TkAssetImpl;
class TkJointImpl;


/**
Struct-enum for actor status flags, used in TkGroup processing.
*/
struct TkActorFlag
{
	enum Enum
	{
		DAMAGED = (1 << 0),	//!< The actor had fractures applied successfully and will take the split step.
		PENDING = (1 << 1), //!< The actor will be processed when its group executes, used to update job queues when moving group.
	};
};


/**
Implementation of TkActor.
*/
class TkActorImpl : public TkActor
{
public:
	TkActorImpl();
	~TkActorImpl();

	// Begin TkActor
	virtual const NvBlastActor*	getActorLL() const override;

	virtual TkFamily&			getFamily() const override;

	virtual uint32_t			getIndex() const override;

	virtual TkGroup*			getGroup() const override;

	virtual TkGroup*			removeFromGroup() override;

	virtual const TkAsset*		getAsset() const override;

	virtual uint32_t 			getVisibleChunkCount() const override;

	virtual uint32_t 			getVisibleChunkIndices(uint32_t* visibleChunkIndices, uint32_t visibleChunkIndicesSize) const override;

	virtual uint32_t 			getGraphNodeCount() const override;

	virtual uint32_t 			getGraphNodeIndices(uint32_t* graphNodeIndices, uint32_t graphNodeIndicesSize) const override;

	virtual const float* 		getBondHealths() const override;

	virtual uint32_t			getSplitMaxActorCount() const override;

	virtual void				damage(const NvBlastDamageProgram& program, const NvBlastProgramParams* programParams) override;
	virtual void				damage(const NvBlastDamageProgram& program, const void* damageDesc, uint32_t descSize) override;
	virtual void				damage(const NvBlastDamageProgram& program, const void* damageDesc, uint32_t descSize, const void* material) override;

	virtual bool				isPending() const override;

	virtual void				generateFracture(NvBlastFractureBuffers* commands, const NvBlastDamageProgram& program, const NvBlastProgramParams* programParams) const override;

	virtual void				applyFracture(NvBlastFractureBuffers* eventBuffers, const NvBlastFractureBuffers* commands) override;

	virtual uint32_t			getJointCount() const override;

	virtual uint32_t			getJoints(TkJoint** joints, uint32_t jointsSize) const override;
	// End TkActor

	// Begin TkObject
	virtual void				release() override;
	// End TkObject


	// Public methods
	
	/**
	Factory create method.

	\param[in]	desc	Actor descriptor set by the user.

	\return a pointer to a new TkActorImpl object if successful, NULL otherwise.
	*/
	static TkActorImpl*			create(const TkActorDesc& desc);

	/**
	TkActorImpl objects are created in an array within a TkFamilyImpl.  Actors may become
	'inactive' without their memory being freed.  If inactive, the actor should be treated as if
	it has been released.

	\return the active status of this TkActorImpl.
	*/
	bool						isActive() const;

	/**
	Utility to return the low-level family to which the low-level actor belongs.

	\return a pointer to the NvBlastFamily to which the low-level actor belongs.
	*/
	NvBlastFamily*				getFamilyLL() const;

	/**
	Utility to access the TkFamily to which this actor belongs.

	\return a reference to the TkFamilyImpl to which this TkActorImpl belongs.
	*/
	TkFamilyImpl&				getFamilyImpl() const;

	/**
	\return the index of this actor with its TkFamilyImpl.
	*/
	uint32_t					getIndexInternal() const;

	/**
	Access to the group to which this actor belongs, if any.

	\return a pointer to the TkGroupImpl to which this TkActorImpl belongs, if any.  If this actor is not in a group, this function returns NULL.
	*/
	TkGroupImpl*				getGroupImpl() const;

	/**
	Access to the low-level actor associated with this TkActorImpl.

	\return a pointer to the NvBlastActor associated with this TkActorImpl.  If this actor is inactive (see isActive), this function returns NULL.
	*/
	NvBlastActor*				getActorLLInternal() const;

	/**
	\return the number of TkJointImpl objects that reference this actor.
	*/
	uint32_t					getJointCountInternal() const;

	/**
		Joint iterator.  Usage:

		Given a TkActorImpl a,

		for (TkActorImpl::JointIt i(a); (bool)i; ++i)
		{
			TkJointImpl* joint = (TkJointImpl*)i;
			// ...
		}
	*/
	class JointIt : public DList::It
	{
	public:
		/** Constructed from an actor. */
		JointIt(const TkActorImpl& actor, Direction dir = Forward);

		/** Current joint. */
		TkJointImpl* operator * () const;
	};

	/**
	Implicit converter to TkActorData for events.
	*/
	operator Nv::Blast::TkActorData() const;

private:
	/**
	Used to buffer damage for deferred fracture generation. Unifies 2 different ways to pass and store damage data.
	*/
	struct DamageData
	{
		DamageData(const NvBlastDamageProgram& program, const NvBlastProgramParams* params);
		DamageData(const NvBlastDamageProgram& program, const void* material, const void* desc, uint32_t descSize);

		bool tryAppend(const NvBlastDamageProgram& program, const void* material, const void* desc, uint32_t descSize);
		void generateFracture(NvBlastFractureBuffers* commandBuffers, const NvBlastActor* actorLL, NvBlastTimers* timers) const;

		enum Type
		{
			Plain,
			Buffered
		};

		Type getType() const;

		NvBlastDamageProgram  m_program;
		union
		{
			const void*					m_material;			//!< for Buffered type
			const NvBlastProgramParams*	m_programParams;	//!< for Plain type
		};
		TkArray<char>::type	m_damageDescs;
		uint32_t			m_damageDescCount;
	};


	/**
	Functions to raise or check 'damaged' state: this actor will take the split step.
	'damaged' actors automatically become 'pending' also.
	*/
	void						markAsDamaged();
	bool						isDamaged() const;

	/**
	Raise actor to 'pending' state: this actor will be processed when its group executes next.
	Enqueues the actor in its group's job list if a group is set. Otherwise the group will enqueue the actor when it is added.
	*/
	void						makePending();

	/**
	Functions to add or remove an internal reference to a joint.  (Joints and actors mutually reference each other.)
	*/
	void						addJoint(TkJointLink& jointLink);
	void						removeJoint(TkJointLink& jointLink);


	// Data

	NvBlastActor*							m_actorLL;			//!< The low-level actor associated with this actor
	TkFamilyImpl*							m_family;			//!< The TkFamilyImpl to which this actor belongs
	TkGroupImpl*							m_group;			//!< The TkGroupImpl (if any) to which this actor belongs
	uint32_t								m_groupJobIndex;	//!< The index of this actor's job within its group's job list
	physx::PxFlags<TkActorFlag::Enum, char>	m_flags;			//!< Status flags for this actor
	TkArray<DamageData>::type				m_damageBuffer;		//!< Buffered damage input
	uint32_t								m_jointCount;		//!< The number of joints referenced in m_jointList
	DList									m_jointList;		//!< A doubly-linked list of joint references

//#if NV_PROFILE
	NvBlastTimers							m_timers;			//!< If profiling, each actor stores timing data
//#endif

	friend class TkWorker;					// m_damageBuffer and m_flags 
	friend class TkGroupImpl;
	friend class TkFamilyImpl;
	friend class TkJointImpl;
	friend class TkFrameworkImpl;
};


//////// TkActorImpl inline methods ////////

NV_INLINE TkFamilyImpl& TkActorImpl::getFamilyImpl() const
{
	NVBLAST_ASSERT(m_family != nullptr);

	return *m_family;
}


NV_INLINE uint32_t TkActorImpl::getIndexInternal() const
{
	NVBLAST_ASSERT(isActive());
	return NvBlastActorGetIndex(m_actorLL, TkFrameworkImpl::get()->log);
}


NV_INLINE NvBlastActor* TkActorImpl::getActorLLInternal() const
{
	return m_actorLL;
}


NV_INLINE uint32_t TkActorImpl::getJointCountInternal() const
{
	return m_jointCount;
}


NV_INLINE TkGroupImpl* TkActorImpl::getGroupImpl() const
{
	return m_group;
}


NV_INLINE bool TkActorImpl::isActive() const
{
	return m_actorLL != nullptr;
}


NV_INLINE bool TkActorImpl::isPending() const
{
	return m_flags.isSet(TkActorFlag::PENDING);
}


NV_INLINE void TkActorImpl::addJoint(TkJointLink& jointLink)
{
	NVBLAST_ASSERT(m_jointList.isSolitary(jointLink));

	m_jointList.insertHead(jointLink);
	++m_jointCount;
}


NV_INLINE void TkActorImpl::removeJoint(TkJointLink& jointLink)
{
	NVBLAST_ASSERT(!m_jointList.isSolitary(jointLink));
	NVBLAST_ASSERT(m_jointCount > 0);
	if (m_jointCount > 0)
	{
		--m_jointCount;
		m_jointList.remove(jointLink);
	}
}


//////// TkActorImpl::DamageData inline methods ////////

NV_INLINE TkActorImpl::DamageData::Type TkActorImpl::DamageData::getType() const
{
	return m_damageDescCount > 0 ? Buffered : Plain;
}


NV_INLINE void TkActorImpl::DamageData::generateFracture(NvBlastFractureBuffers* commandBuffers, const NvBlastActor* actorLL, NvBlastTimers* timers) const
{
	if (getType() == Plain)
	{
		NvBlastActorGenerateFracture(commandBuffers, actorLL, m_program, m_programParams, TkFrameworkImpl::get()->log, timers);
	}
	else
	{
		const NvBlastProgramParams programParams = {
			m_damageDescs.begin(),
			m_damageDescCount,
			m_material,
		};
		NvBlastActorGenerateFracture(commandBuffers, actorLL, m_program, &programParams, TkFrameworkImpl::get()->log, timers);
	}
}


//////// TkActorImpl::JointIt methods ////////

NV_INLINE TkActorImpl::JointIt::JointIt(const TkActorImpl& actor, Direction dir) : DList::It(actor.m_jointList, dir) {}


NV_INLINE TkJointImpl* TkActorImpl::JointIt::operator * () const
{
	const DLink* link = (const DLink*)(*this);
	return reinterpret_cast<const TkJointLink*>(link)->m_joint;
}

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTTKACTORIMPL_H