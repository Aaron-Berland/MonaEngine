#include "AnimationClip.hpp"
#include <algorithm>
#include <math.h>
#include "../Core/Log.hpp"
#include "Skeleton.hpp"
namespace Mona {
	AnimationClip::AnimationClip(std::vector<AnimationTrack>&& animationTracks,
		std::vector<std::string>&& trackNames,
		std::shared_ptr<Skeleton> skeletonPtr,
		float duration,
		float ticksPerSecond,
		bool removeRootMotion) :
		m_animationTracks(std::move(animationTracks)),
		m_trackNames(std::move(trackNames)),
		m_skeletonPtr(nullptr),
		m_duration(duration),
		m_ticksPerSecond(ticksPerSecond)
	{
		m_jointIndices.resize(m_trackNames.size());
		SetSkeleton(skeletonPtr);
		if (removeRootMotion)
			RemoveRootMotion();
	}

	float AnimationClip::Sample(std::vector<JointPose>& outPose, float time, bool isLooping) {
		//Primero se obtiene el tiempo de muestreo correcto
		float newTime = GetSamplingTime(time, isLooping);

		//Por cada articulación o joint animada
		for (uint32_t i = 0; i < m_animationTracks.size(); i++)
		{
			const AnimationTrack& animationTrack = m_animationTracks[i];
			uint32_t jointIndex = m_jointIndices[i];
			std::pair<uint32_t, float> fp;
			glm::vec3 localPosition;
			/* Siempre se chequea si hay solo una muestra en el track dado que en ese caso no tiene sentido interpolar.
			En cada cado (rotación, translación y escala) primero se llama una función que obtiene, dado el tiempo de muestreo, 
			el primer indice cuyo tiempo de muestreo es mayor al obtenido y un valor entre 0 y 1 que representa que tan cerca
			esta de dicha muestra, este valor se usa para interpolar.*/
			if (animationTrack.positions.size() > 1)
			{
				
				fp = GetTimeFraction(animationTrack.positionTimeStamps, newTime);
				const glm::vec3& position = animationTrack.positions[fp.first - 1];
				const glm::vec3& nextPosition = animationTrack.positions[fp.first % animationTrack.positions.size()];
				localPosition = glm::mix(position, nextPosition, fp.second);
			}
			else {
				localPosition = animationTrack.positions[0];
			}

			glm::fquat localRotation;
			if (animationTrack.rotations.size() > 1)
			{
				fp = GetTimeFraction(animationTrack.rotationTimeStamps, newTime);
				const glm::fquat& rotation = animationTrack.rotations[fp.first - 1];
				const glm::fquat& nextRotation = animationTrack.rotations[fp.first % animationTrack.rotations.size()];
				//Para interpolar rotaciones se usa slerp en vez de mix (linear interpolation)
				localRotation = glm::slerp(rotation, nextRotation, fp.second);
			}
			else {
				localRotation = animationTrack.rotations[0];
			}

			glm::vec3 localScale;
			if (animationTrack.scales.size() > 1) {
				fp = GetTimeFraction(animationTrack.scaleTimeStamps, newTime);
				const glm::vec3& scale = animationTrack.scales[fp.first - 1];
				const glm::vec3& nextScale = animationTrack.scales[fp.first % animationTrack.scales.size()];
				localScale = glm::mix(scale, nextScale, fp.second);
			}
			else {
				localScale = animationTrack.scales[0];
			}

			outPose[jointIndex] = JointPose(localRotation, localPosition, localScale);

		}
		return newTime;
	}

	void AnimationClip::SetSkeleton(std::shared_ptr<Skeleton> skeletonPtr) {
		if (!skeletonPtr || skeletonPtr == m_skeletonPtr)
		{
			return;
		}
		//Al momento de configurar el esqueleto se reccorren los tracks para obtener los indices de las articulaciones,
		// dentro del esqueleto recien configurado. De esta forma, dentro del main-loop se usaran indices a arreglos.
		for (uint32_t i = 0; i < m_trackNames.size(); i++) {
			const std::string& name = m_trackNames[i];
			int32_t signIndex = skeletonPtr->GetJointIndex(name);
			MONA_ASSERT(signIndex >= 0, "AnimationClip Error: Given skeleton incompatible with importing animation");
			uint32_t jointIndex = static_cast<uint32_t>(signIndex);
			m_jointIndices[i] = jointIndex;
		}
		m_skeletonPtr = skeletonPtr;
	}

	void AnimationClip::RemoveRootMotion() {
		//Remueve las translaciones del track de animación asociado a la raiz del esqueleto
		for (uint32_t i = 0; i < m_animationTracks.size(); i++)
		{
			AnimationTrack& animationTrack = m_animationTracks[i];
			uint32_t jointIndex = m_jointIndices[i];
			if (jointIndex != 0) continue;
			for (uint32_t j = 0; j < animationTrack.positions.size(); j++)
			{
				animationTrack.positions[j] = glm::vec3(0.0f);
			}
		}
	}

	float AnimationClip::GetSamplingTime(float time, bool isLooping) const {
		if (isLooping)
			return std::fmod(time, m_duration);
		return std::clamp(time, 0.0f, m_duration);
	}

	std::pair<uint32_t, float> AnimationClip::GetTimeFraction(const std::vector<float>& timeStamps, float time) const {
		uint32_t sample = 0;
		bool outOfRange = false;
		// Se avanza el indice hasta que el tiempo que se busca es menor al timeStamp actual
		// con esto se sabe que la muestra esta entre este indice y el anterior.
		while (!outOfRange && time >= timeStamps[sample]) {
			sample++;
			outOfRange = sample >= timeStamps.size();
		}

		float start = timeStamps[sample - 1];
		float end = outOfRange ? m_duration : timeStamps[sample];
		float frac = (time - start) / (end - start);
		return { sample, frac };
	}


}