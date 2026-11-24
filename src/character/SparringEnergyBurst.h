#pragma once

#include "character/CombatShapes.h"
#include "components/CombatHitboxComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/LightComponent.h"
#include "components/ParticleEmitterComponent.h"
#include "components/SparringPartnerComponent.h"
#include "core/Raycast.h"

namespace MyEngine
{
	struct SparringEnergyBurst
	{
		uint32_t owner, target, activation;
		int team;
		float damage, posture, radius;
		glm::vec3 direction;
		std::shared_ptr<BurstPress> press;
		float remaining = 12.0f, fade = -1.0f;
		float ringTimer = 0.06f;
		int impactRegion = -1, glowParticle = -1;
		float speed = 22.0f;
		bool impactFlash = false;
	};

	inline void UpdateBurstLight(LightComponent& light, const SparringEnergyBurst& burst, glm::vec3 position)
	{
		const bool heavy = burst.posture >= 20.f;
		const float envelope = burst.fade < 0 ? 1.f : std::clamp(burst.fade/(heavy ? .28f : .22f),0.f,1.f);
		const float flash = burst.impactFlash ? envelope*envelope : 0.f;
		light.position = position - burst.direction*(.25f*flash);
		light.color = glm::mix(glm::vec3(.12f,.78f,1.f),glm::vec3(.76f,.97f,1.f),flash);
		light.intensity = (heavy ? 6.f : 3.4f)*envelope + (heavy ? 13.f : 7.f)*flash;
		light.range = (heavy ? 5.f : 3.6f) + 2.f*flash;
	}

	inline glm::vec2 SparringImpactShake(const Scene& scene)
	{
		float envelope = 0;
		for (const auto& e : scene.GetEntities())
		{
			if (!e || !e->HasComponent<SparringEnergyBurst>()) continue;
			const auto& burst = e->GetComponent<SparringEnergyBurst>();
			if (burst.impactRegion >= 0 && burst.posture >= 20.f)
				envelope = std::max(envelope,std::clamp((burst.fade-.12f)/.16f,0.f,1.f));
		}
		return envelope*envelope*glm::vec2(.028f*std::cos(12.f*(1.f-envelope)),.017f*std::cos(18.f*(1.f-envelope)));
	}

	inline void EmitSparringRing(ParticleEmitterComponent& fx, glm::vec3 center, glm::vec3 axis, bool impact)
	{
		const auto u = glm::normalize(glm::cross(axis,std::abs(axis.y) < .95f ? glm::vec3(0,1,0) : glm::vec3(1,0,0)));
		const auto v = glm::cross(axis,u);
		int i = 0;
		for (auto& p : fx.particles)
		{
			if (p.alive) continue;
			const float angle = glm::radians(15.f * i);
			const auto radial = u * std::cos(angle) + v * std::sin(angle);
			p = Particle{}; p.alive = true;
			p.position = center + radial * .38f;
			p.mark = Particle::Mark::Brush; p.strokeAxis = glm::cross(axis,radial);
			p.stretch = impact ? 2.6f : 1.8f;
			p.velocity = radial * (impact ? 5.f : 2.f) + axis * (impact ? 0.f : 9.f);
			p.sizeStart = .19f; p.sizeEnd = .08f; p.lifetime = impact ? .25f : .16f;
			p.colorStart = {.3f,1,1,1}; p.colorEnd = {.02f,.7f,1,0};
			if (++i == 24) break;
		}
	}

	enum class PressureStage { Launch, Wake, Impact };

	inline void EmitPressure(ParticleEmitterComponent& fx, glm::vec3 center, glm::vec3 axis, bool heavy, PressureStage stage, float phase)
	{
		const auto u = glm::normalize(glm::cross(axis,std::abs(axis.y) < .95f ? glm::vec3(0,1,0) : glm::vec3(1,0,0)));
		const auto v = glm::cross(axis,u);
		const int primary = stage == PressureStage::Wake ? (heavy ? 24 : 16) : (heavy ? 28 : 16);
		const int count = primary + (stage == PressureStage::Impact ? primary/2 : 0);
		if (stage == PressureStage::Impact && fx.particles.size() > 1) fx.particles.front().alive = false;
		int i = 0;
		for (auto& p : fx.particles)
		{
			if (p.alive) continue;
			const float t = static_cast<float>(i)/primary;
			const float a = phase + glm::radians(360.f)*t;
			const auto radial = u*std::cos(a) + v*std::sin(a);
			const auto tangent = v*std::cos(a) - u*std::sin(a);
			const bool spark = stage == PressureStage::Impact && i%3 == 0;
			const float width = heavy ? .34f : .16f;
			p = Particle{}; p.alive = true;
			p.position = center + width*radial;
			if (stage == PressureStage::Wake) p.position -= axis*(.08f + t*.7f);
			p.velocity = stage == PressureStage::Launch ? radial*2.f - axis*.8f : stage == PressureStage::Wake ? tangent*1.2f + axis*3.f : radial*(spark ? 5.f : 2.f) - axis*(spark ? 2.f : .5f);
			p.sizeStart = spark ? .09f : .14f; p.sizeEnd = spark ? .015f : .24f;
			p.colorStart = spark ? glm::vec4(.85f,1,1,1) : glm::vec4(.35f,.9f,1,.55f);
			p.colorEnd = {.08f,.62f,.9f,0}; p.lifetime = stage == PressureStage::Impact ? .20f : .12f;
			if (stage == PressureStage::Launch && i%2 == 0)
			{
				p.position += width*.65f*radial - axis*.08f;
				p.velocity = -radial*(width*11.f) + tangent*.6f + axis*.8f;
				p.sizeStart = .12f; p.sizeEnd = .045f; p.lifetime = .09f;
				p.colorStart = {.75f,1.f,1.f,.9f};
				p.mark = Particle::Mark::Brush; p.strokeAxis = glm::normalize(-radial + tangent*.65f); p.stretch = 2.7f;
			}
			else if (stage == PressureStage::Impact && i == 0)
			{
				p.position = center; p.velocity = glm::vec3(0);
				p.mark = Particle::Mark::Impact; p.strokeAxis = axis; p.stretch = 1.35f;
				p.sizeStart = heavy ? 1.05f : .70f; p.sizeEnd = heavy ? 1.65f : 1.15f;
				p.colorStart = {.72f,1.f,1.f,1.f}; p.lifetime = .11f;
			}
			else if (i >= primary)
			{
				const float fan = (i-primary)*2.39996f + phase;
				const auto ray = std::cos(fan)*u + std::sin(fan)*v;
				p.position = center + ray*(width*.45f);
				p.velocity = ray*(3.f + i%4) - axis*(1.2f + .4f*(i%3));
				p.sizeStart = .065f; p.sizeEnd = .012f; p.lifetime = .12f + .02f*(i%4);
				p.colorStart = {.85f,.98f,1.f,1.f};
				p.mark = Particle::Mark::Brush; p.strokeAxis = glm::normalize(p.velocity); p.stretch = 3.f + i%3;
			}
			else if (stage == PressureStage::Wake && i < 2)
			{
				const float turn = phase + (i ? glm::radians(180.f) : 0.f);
				const auto peel = u*std::cos(turn) + v*std::sin(turn);
				const auto sweep = v*std::cos(turn) - u*std::sin(turn);
				p.mark = Particle::Mark::Crescent;
				p.position = center - axis*(.18f+.10f*i) + peel*width;
				p.velocity = axis*1.25f + peel*(heavy ? 2.3f : 1.7f) + sweep*.8f;
				p.strokeAxis = glm::normalize(peel + axis*.35f); p.stretch = 1.4f;
				p.sizeStart = heavy ? .58f : .36f; p.sizeEnd = heavy ? 1.15f : .72f;
				p.colorStart = {.48f,.94f,1.f,.84f}; p.colorEnd = {.09f,.58f,1.f,0.f};
				p.lifetime = heavy ? .24f : .18f;
			}
			else if (stage == PressureStage::Wake)
			{
				const float tail = static_cast<float>(i/2)/(primary/2-1);
				const float turn = phase + (i%2 ? 3.14f : 0.f) + tail*(5.f + .6f*std::sin(phase));
				const auto curl = u*std::cos(turn) + v*std::sin(turn);
				const auto swirl = v*std::cos(turn) - u*std::sin(turn);
				p.position = center - axis*(.06f + .82f*tail) + curl*(width*(.4f+.65f*tail));
				p.velocity = axis*(2.5f-tail) + swirl*(.55f+.45f*tail) + curl*.25f;
				p.sizeStart = .105f-.045f*tail; p.sizeEnd = .12f+.06f*tail;
				p.colorStart = {.42f,.93f,1.f,.48f-.22f*tail};
				p.lifetime = .09f+.05f*tail;
				p.mark = Particle::Mark::Brush; p.strokeAxis = glm::normalize(axis + swirl*(.12f+.24f*tail));
				p.stretch = (heavy ? 4.2f : 6.f) - 2.f*tail;
			}
			else if (stage == PressureStage::Impact && !spark)
			{
				const bool outer = i%3 == 2;
				const float eddy = std::sin(phase + i*2.4f);
				p.position = center + radial*width*(outer ? .9f : .35f) - axis*(.04f+.025f*eddy);
				p.velocity = radial*((outer ? 1.f : 2.8f)+.3f*eddy) + tangent*(eddy*.65f) - axis*(outer ? .35f : 1.1f);
				p.velocity.y += outer ? .55f : .15f;
				p.sizeStart = outer ? .16f : .09f;
				p.sizeEnd = (outer ? .43f : .24f)*(heavy ? 1.25f : 1.f);
				p.colorStart = outer ? glm::vec4(.48f,.79f,.85f,.24f) : glm::vec4(.63f,.95f,1.f,.48f);
				p.lifetime = outer ? (heavy ? .26f : .21f) : .14f+.02f*eddy;
			}
			if (++i == count) break;
		}
	}

	inline bool BurstBoxContact(const Scene& scene, const Entity& obstacle, const Ray& ray, float& distance)
	{
		if (!obstacle.HasComponent<BoxColliderComponent>()) return false;
		const auto& box = obstacle.GetComponent<BoxColliderComponent>();
		if (box.isTrigger) return false;
		const auto world = TransformHierarchy::GetWorldMatrix(scene,obstacle);
		if (std::abs(glm::determinant(world)) < 1e-8f) return false;
		const auto inv = glm::inverse(world);
		const Ray local{glm::vec3(inv*glm::vec4(ray.origin,1)),glm::vec3(inv*glm::vec4(ray.direction,0))};
		float t;
		if (!RayIntersectsAABB(local,box.center,box.halfExtents,t) || t > distance) return false;
		distance = t; return true;
	}

	inline bool ClearBurstLane(const Scene& scene, const Entity& shooter, const Entity& target)
	{
		const glm::vec3 start = TransformHierarchy::GetWorldMatrix(scene,shooter)*glm::vec4(0,1,0,1);
		const glm::vec3 end = TransformHierarchy::GetWorldMatrix(scene,target)*glm::vec4(target.GetComponent<CombatHurtboxComponent>().center,1);
		float length = glm::distance(start,end);
		if (length < .001f) return false;
		const Ray ray{start,(end-start)/length};
		for (const auto& obj : scene.GetEntities())
			if (obj && obj.get()!=&shooter && obj.get()!=&target && BurstBoxContact(scene,*obj,ray,length)) return false;
		return true;
	}

	inline glm::vec3 IncomingBurstLane(const Scene& scene, const Entity& target)
	{
		const auto& hurt = target.GetComponent<CombatHurtboxComponent>();
		glm::vec3 lane(0);
		float soonest = .38f;
		for (const auto& e : scene.GetEntities())
		{
			if (!e || !e->HasComponent<SparringEnergyBurst>()) continue;
			const auto& shot = e->GetComponent<SparringEnergyBurst>();
			if (shot.target != target.GetID() || shot.team == hurt.team || shot.fade >= 0 || shot.speed <= 0 || !hurt.canBeHit) continue;
			const Ray ray{e->GetComponent<TransformComponent>().position,shot.direction};
			float limit = std::min(soonest*shot.speed,shot.remaining);
			bool intersects = false;
			CombatShapes::ForEachHurtSphere(scene,target,hurt,[&](int,const CombatShapes::Sphere& body) {
				float t;
				if (RayIntersectsSphere(ray,body.center,body.radius+shot.radius,t) && t >= 0 && t <= limit) { intersects = true; limit = t; }
			});
			if (!intersects) continue;
			bool blocked = false;
			for (const auto& obstacle : scene.GetEntities())
				if (obstacle && obstacle.get() != &target && obstacle->GetID() != shot.owner && BurstBoxContact(scene,*obstacle,ray,limit)) { blocked = true; break; }
			if (blocked) continue;
			soonest = limit/shot.speed;
			const glm::vec3 side(-shot.direction.z,0,shot.direction.x);
			lane = glm::length(side) > .001f ? glm::normalize(side) : glm::vec3(1,0,0);
		}
		return lane;
	}

	inline void LaunchSparringBurst(Scene& scene, const Entity& player, const Entity& partner, int slot, const std::shared_ptr<BurstPress>& press)
	{
		const auto& hit = player.GetComponent<CombatHitboxComponent>();
		const auto& attack = player.GetComponent<CombatAttackComponent>();
		if (slot < 0 || slot >= static_cast<int>(attack.attacks.size())) return;
		const auto& def = attack.attacks[slot];
		const auto strikes = CombatShapes::SampleStrikes(scene,player,slot,def.hitboxCenter,def.hitboxRadius);
		const auto world = TransformHierarchy::GetWorldMatrix(scene,player);
		glm::vec3 forward = world[2];
		if (glm::length(forward) < .001f) return;
		forward = glm::normalize(forward);
		glm::vec3 start = strikes.spheres[0].center;
		for (int i = 1; i < strikes.count; ++i)
			if (glm::dot(strikes.spheres[i].center-start,forward) > 0) start = strikes.spheres[i].center;
		const bool aiShot = player.HasComponent<SparringPartnerComponent>();
		const auto chest = aiShot ? partner.GetComponent<CombatHurtboxComponent>().center : partner.GetComponent<SparringRig>().Pose(partner)[SparringRig::Chest];
		const glm::vec3 aim = glm::vec3(TransformHierarchy::GetWorldMatrix(scene,partner)*glm::vec4(chest,1)) - start;
		if (glm::length(aim) > .001f && (aiShot || glm::dot(glm::normalize(aim),forward) >= .82f)) forward = glm::normalize(aim);
		auto effect = scene.CreateEntity("Sparring energy burst");
		effect->AddComponent<TransformComponent>().position = start;
		const float posture = attack.attacks[slot].postureDamage;
		auto& burst = effect->AddComponent<SparringEnergyBurst>(SparringEnergyBurst{player.GetID(),partner.GetID(),press->id,hit.team,def.damage,posture,posture >= 20 ? .45f : .32f,forward,press});
		if (aiShot) burst.speed = 12.f;
		auto& light = effect->AddComponent<LightComponent>();
		light.type = LightComponent::Type::Point; light.transientEffect = true;
		UpdateBurstLight(light,burst,start);
		auto& fx = effect->AddComponent<ParticleEmitterComponent>();
		fx.pressureTrail = true; fx.previousEmissionPosition = start;
		fx.blendMode = ParticleEmitterComponent::BlendMode::Additive;
		fx.maxParticles = 64; fx.spawnRate = 150; fx.burstRequestCount = 6;
		fx.colorStart = {.66f,1,1,1}; fx.colorEnd = {.04f,.24f,1,0};
		fx.sizeStart = posture >= 20 ? .45f : .20f; fx.sizeEnd = .045f;
		fx.gravity = glm::vec3(0); fx.emitDirection = -forward; fx.emitSpeed = 3.5f;
		fx.spreadAngle = 4; fx.lifetime = .12f; fx.lifetimeVariance = .015f;
		fx.maxParticles = posture >= 20 ? 224 : 96;
		fx.particles.resize(fx.maxParticles); fx.poolDirty = false;
		EmitPressure(fx,start,forward,posture >= 20,PressureStage::Launch,0.f);
		if (posture >= 20) EmitSparringRing(fx,start,forward,false);
	}
}
