#include "InspectorSectionsPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <AL/al.h>

#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "components/AudioListenerComponent.h"
#include "components/AudioSourceComponent.h"
#include "components/BoundingSphereComponent.h"
#include "components/BossAIComponent.h"
#include "components/BoxColliderComponent.h"
#include "components/CameraComponent.h"
#include "components/CapsuleColliderComponent.h"
#include "components/CollisionEventsComponent.h"
#include "components/CombatAttackComponent.h"
#include "components/CombatHitboxComponent.h"
#include "components/CombatHurtboxComponent.h"
#include "components/CombatStatsComponent.h"
#include "components/JointComponent.h"
#include "components/MeshComponent.h"
#include "components/MeshColliderComponent.h"
#include "components/ParticleEmitterComponent.h"
#include "components/PrefabInstanceComponent.h"
#include "components/RigidbodyComponent.h"
#include "components/ScriptComponent.h"
#include "components/SkeletonComponent.h"
#include "components/TransformComponent.h"
#include "audio/AudioClip.h"
#include "audio/AudioEngine.h"
#include "core/AnimationEventBus.h"
#include "core/AssetManager.h"
#include "core/FileDialog.h"
#include "core/LayerMask.h"
#include "ecs/TransformHierarchy.h"
#include "editor/EditorStyle.h"
#include "imgui.h"
#include "serialization/SceneSerializer.h"

namespace MyEngine::Editor::Panels
{
	void DrawInspectorPrefabAndEntitySections(InspectorSectionsContext& context)
	{
		if (!context.scene || !context.selectedEntity || !context.state)
			return;

		auto* selectedEntity = context.selectedEntity;
		auto& state = *context.state;

		ImGui::TextUnformatted(selectedEntity->GetName().c_str());
		ImGui::TextDisabled("Entity ID: %u", selectedEntity->GetID());
		if (InspectorActionButton("Save as Prefab"))
		{
			std::string prefabPath = MyEngine::FileDialog::SavePrefabFile();
			if (!prefabPath.empty())
			{
				if (MyEngine::Serialization::SavePrefab(*context.scene, selectedEntity, prefabPath))
					ImGui::OpenPopup("PrefabSaved");
			}
		}
		if (ImGui::BeginPopup("PrefabSaved"))
		{
			ImGui::Text("Prefab saved successfully.");
			if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		if (selectedEntity->HasComponent<PrefabInstanceComponent>())
		{
			InspectorGroupLabel("Prefab Instance");
			auto& prefab = selectedEntity->GetComponent<PrefabInstanceComponent>();
			ImGui::TextWrapped("Source: %s", prefab.sourcePrefabPath.empty() ? "<unsaved>" : prefab.sourcePrefabPath.c_str());
			ImGui::Text("Source Entity ID: %u", prefab.sourceEntityID);
			if (prefab.isVariantInstance)
			{
				ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Variant Instance");
				ImGui::TextWrapped("Base Prefab: %s", prefab.variantBasePrefabPath.empty() ? "<none>" : prefab.variantBasePrefabPath.c_str());
				ImGui::Text("Base Entity ID: %u", prefab.variantBaseEntityID);
			}

			if (InspectorActionButton("Create Variant From Selected"))
			{
				if (prefab.sourcePrefabPath.empty() || prefab.sourceEntityID == 0)
				{
					state.prefabVariantStatus = "Prefab source metadata is missing; cannot create variant.";
					state.prefabVariantStatusIsError = true;
				}
				else
				{
					std::string variantPath = MyEngine::FileDialog::SavePrefabFile();
					if (!variantPath.empty())
					{
						const bool saved = MyEngine::Serialization::SavePrefabVariant(
							*context.scene,
							selectedEntity,
							variantPath,
							prefab.sourcePrefabPath,
							prefab.sourceEntityID);
						state.prefabVariantStatus = saved
							? ("Variant prefab saved: " + variantPath)
							: "Failed to save variant prefab.";
						state.prefabVariantStatusIsError = !saved;
					}
				}
			}
			if (!state.prefabVariantStatus.empty())
			{
				ImVec4 statusColor = state.prefabVariantStatusIsError
					? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
					: ImVec4(0.5f, 0.9f, 0.5f, 1.0f);
				ImGui::TextColored(statusColor, "%s", state.prefabVariantStatus.c_str());
			}

			auto overrides = context.describePrefabOverrides ? context.describePrefabOverrides(selectedEntity) : std::vector<std::string>{};
			if (overrides.empty())
			{
				ImGui::TextDisabled("No overrides detected.");
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Overrides:");
				for (const auto& overrideName : overrides)
					ImGui::BulletText("%s", overrideName.c_str());

				if (state.selectedOverrideIndex >= static_cast<int>(overrides.size()))
					state.selectedOverrideIndex = 0;

				const char* selectedLabel = overrides.empty() ? "<none>" : overrides[state.selectedOverrideIndex].c_str();
				if (ImGui::BeginCombo("Override Target", selectedLabel))
				{
					for (int oi = 0; oi < static_cast<int>(overrides.size()); ++oi)
					{
						bool selected = (oi == state.selectedOverrideIndex);
						if (ImGui::Selectable(overrides[oi].c_str(), selected))
							state.selectedOverrideIndex = oi;
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				bool appliedAction = false;
				if (InspectorActionButton("Revert Selected Override") && !overrides.empty() && context.revertSelectedPrefabOverride)
				{
					std::string actionMessage;
					bool ok = context.revertSelectedPrefabOverride(selectedEntity, overrides[state.selectedOverrideIndex], actionMessage);
					state.prefabOverrideActionStatus = actionMessage;
					state.prefabOverrideActionStatusIsError = !ok;
					appliedAction = ok;
				}
				if (InspectorActionButton("Apply All Overrides To Prefab") && context.applyAllPrefabOverrides)
				{
					std::string actionMessage;
					bool ok = context.applyAllPrefabOverrides(selectedEntity, actionMessage);
					state.prefabOverrideActionStatus = actionMessage;
					state.prefabOverrideActionStatusIsError = !ok;
					appliedAction = ok;
				}

				if (!state.prefabOverrideActionStatus.empty())
				{
					ImVec4 statusColor = state.prefabOverrideActionStatusIsError
						? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
						: ImVec4(0.5f, 0.9f, 0.5f, 1.0f);
					ImGui::TextColored(statusColor, "%s", state.prefabOverrideActionStatus.c_str());
				}

				if (appliedAction && context.describePrefabOverrides)
					overrides = context.describePrefabOverrides(selectedEntity);
			}
		}

		InspectorGroupLabel("Entity Settings");
		{
			static char tagBuf[64] = "";
			strncpy_s(tagBuf, selectedEntity->GetTag().c_str(), sizeof(tagBuf) - 1);
			InspectorFullWidth();
			if (ImGui::InputText("Tag##entityTag", tagBuf, sizeof(tagBuf)))
				selectedEntity->SetTag(tagBuf);

			int currentLayer = static_cast<int>(selectedEntity->GetLayer());
			const auto& layerNames = MyEngine::LayerMask::GetNames();
			InspectorFullWidth();
			if (ImGui::BeginCombo("Layer##entityLayer", layerNames[currentLayer].c_str()))
			{
				for (int li = 0; li < MyEngine::MAX_LAYERS; ++li)
				{
					bool sel = (li == currentLayer);
					if (ImGui::Selectable(layerNames[li].c_str(), sel))
						selectedEntity->SetLayer(static_cast<uint32_t>(li));
					if (sel) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		ImGui::Spacing();
	}

	void DrawInspectorTransformAndCameraSections(InspectorSectionsContext& context)
	{
		if (!context.scene || !context.selectedEntity || !context.undoStack)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<TransformComponent>())
		{
			if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& transform = selectedEntity->GetComponent<TransformComponent>();
				TransformComponent beforeEdit = transform;
				bool edited = false;
				edited |= ImGui::DragFloat3("Position##transform", &transform.position.x, 0.1f);
				edited |= ImGui::DragFloat3("Rotation##transform", &transform.rotation.x, 1.0f);
				edited |= ImGui::DragFloat3("Scale##transform", &transform.scale.x, 0.1f, 0.01f, 100.0f);
				if (edited && ImGui::IsItemDeactivatedAfterEdit())
				{
					context.undoStack->Push(std::make_unique<EditorUndo::TransformEditCommand>(
						selectedEntity->GetID(), beforeEdit, transform));
				}
			}
		}

		if (selectedEntity->HasComponent<CameraComponent>())
		{
			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& cam = selectedEntity->GetComponent<CameraComponent>();
				ImGui::Checkbox("Primary Camera", &cam.isPrimary);
				ImGui::DragFloat("FOV", &cam.fov, 1.0f, 1.0f, 179.0f);
				ImGui::DragFloat("Near Plane", &cam.nearPlane, 0.01f, 0.01f, 10.0f);
				ImGui::DragFloat("Far Plane", &cam.farPlane, 10.0f, 10.0f, 10000.0f);
				ImGui::DragFloat("Move Speed", &cam.moveSpeed, 0.1f, 0.1f, 50.0f);
				ImGui::DragFloat("Mouse Sensitivity", &cam.mouseSensitivity, 0.01f, 0.01f, 1.0f);
				ImGui::Checkbox("Enable Input", &cam.enableInput);
				ImGui::Checkbox("Fly Mode", &cam.flyMode);

				ImGui::Separator();
				ImGui::Text("Third-Person Follow");
				ImGui::Checkbox("Enable Third-Person", &cam.thirdPerson);
				if (cam.thirdPerson)
				{
					ImGui::DragFloat("Follow Distance", &cam.followDistance, 0.1f, 0.5f, 50.0f);
					ImGui::DragFloat("Follow Height", &cam.followHeight, 0.1f, -10.0f, 20.0f);

					std::string currentLabel = "(none)";
					if (cam.followTargetID != 0)
					{
						auto target = TransformHierarchy::FindEntityByID(*context.scene, cam.followTargetID);
						if (target)
							currentLabel = target->GetName();
						else
							currentLabel = "(missing entity)";
					}

					if (ImGui::BeginCombo("Follow Target", currentLabel.c_str()))
					{
						for (auto& e : context.scene->GetEntities())
						{
							if (!e || e.get() == selectedEntity || !e->HasComponent<TransformComponent>())
								continue;

							bool isSelected = (e->GetID() == cam.followTargetID);
							if (ImGui::Selectable(e->GetName().c_str(), isSelected))
								cam.followTargetID = e->GetID();
							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					ImGui::Separator();
					ImGui::Text("Lock-On (1v1)");
					ImGui::Checkbox("Enable Lock-On", &cam.lockOnEnabled);
					ImGui::DragFloat("Lock-On Max Distance", &cam.lockOnMaxDistance, 0.1f, 1.0f, 100.0f);
					ImGui::DragFloat("Lock-On Max Angle", &cam.lockOnMaxAngleDegrees, 1.0f, 1.0f, 179.0f);
					ImGui::DragFloat("Lock-On Height Offset", &cam.lockOnHeightOffset, 0.05f, -2.0f, 5.0f);
					ImGui::DragFloat("Lock-On Camera Distance", &cam.lockOnCameraDistance, 0.05f, 0.5f, 20.0f);
					ImGui::DragFloat("Lock-On Camera Height", &cam.lockOnCameraHeight, 0.05f, -2.0f, 10.0f);

					std::string lockLabel = "(auto)";
					if (cam.lockOnTargetID != 0)
					{
						auto lockTarget = TransformHierarchy::FindEntityByID(*context.scene, cam.lockOnTargetID);
						if (lockTarget)
							lockLabel = lockTarget->GetName();
					}
					ImGui::Text("Current Lock-On Target: %s", lockLabel.c_str());
					if (ImGui::Button("Clear Lock-On Target"))
						cam.lockOnTargetID = 0;
				}
			}
		}
	}

	void DrawInspectorCombatSections(InspectorSectionsContext& context)
	{
		if (!context.scene || !context.selectedEntity)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<CombatAttackComponent>())
		{
			if (BeginInspectorSection("Combat Attack"))
			{
				auto& combatAttack = selectedEntity->GetComponent<CombatAttackComponent>();
				InspectorGroupLabel("Authoring");
				char attackPathBuffer[260] = {};
				strncpy_s(attackPathBuffer, combatAttack.attackSetPath.c_str(), sizeof(attackPathBuffer) - 1);
				if (ImGui::InputText("Attack Set JSON", attackPathBuffer, sizeof(attackPathBuffer)))
					combatAttack.attackSetPath = attackPathBuffer;
				ImGui::Checkbox("Auto Load From JSON", &combatAttack.autoLoadFromJson);
				if (InspectorActionButton("Reload Attack Set"))
					combatAttack.requestReloadFromJson = true;
				if (!combatAttack.lastLoadError.empty())
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", combatAttack.lastLoadError.c_str());

				InspectorGroupLabel("Runtime");
				ImGui::Checkbox("Request Attack", &combatAttack.attackRequested);
				ImGui::DragInt("Selected Attack Index", &combatAttack.selectedAttackIndex, 1.0f, 0,
					std::max(0, static_cast<int>(combatAttack.attacks.size()) - 1));
				ImGui::Text("Is Attacking: %s", combatAttack.isAttacking ? "Yes" : "No");
				ImGui::Text("Cooldown: %.2f", combatAttack.cooldownTimer);

				InspectorGroupLabel("Attack Definitions");
				for (int ai = 0; ai < static_cast<int>(combatAttack.attacks.size()); ++ai)
				{
					auto& def = combatAttack.attacks[ai];
					std::string label = def.name.empty() ? ("Attack " + std::to_string(ai)) : def.name;
					if (ImGui::TreeNode((label + "##attackDef").c_str()))
					{
						char nameBuffer[128] = {};
						strncpy_s(nameBuffer, def.name.c_str(), sizeof(nameBuffer) - 1);
						if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
							def.name = nameBuffer;
						ImGui::DragInt("Startup Frames", &def.startupFrames, 1.0f, 0, 300);
						ImGui::DragInt("Active Frames", &def.activeFrames, 1.0f, 1, 300);
						ImGui::DragInt("Recovery Frames", &def.recoveryFrames, 1.0f, 0, 300);
						ImGui::DragFloat("Damage", &def.damage, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat("Posture Damage", &def.postureDamage, 0.1f, 0.0f, 1000.0f);
						ImGui::DragFloat3("Hitbox Center", &def.hitboxCenter.x, 0.05f);
						ImGui::DragFloat("Hitbox Radius", &def.hitboxRadius, 0.01f, 0.01f, 10.0f);
						ImGui::DragFloat("Cooldown", &def.cooldownSeconds, 0.01f, 0.0f, 10.0f);
						if (InspectorDangerButton("Remove Attack Definition"))
						{
							combatAttack.attacks.erase(combatAttack.attacks.begin() + ai);
							ImGui::TreePop();
							break;
						}
						ImGui::TreePop();
					}
				}

				if (InspectorActionButton("Add Attack Definition"))
					combatAttack.attacks.emplace_back();

				if (InspectorDangerButton("Remove Combat Attack"))
					selectedEntity->RemoveComponent<CombatAttackComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Combat Attack"))
			{
				selectedEntity->AddComponent<CombatAttackComponent>();
				if (!selectedEntity->HasComponent<CombatHitboxComponent>())
					selectedEntity->AddComponent<CombatHitboxComponent>();
			}
		}

		if (selectedEntity->HasComponent<CombatStatsComponent>())
		{
			if (BeginInspectorSection("Combat Stats"))
			{
				auto& combatStats = selectedEntity->GetComponent<CombatStatsComponent>();
				InspectorGroupLabel("Vitals");
				ImGui::DragFloat("Max Health", &combatStats.maxHealth, 0.1f, 1.0f, 5000.0f);
				ImGui::DragFloat("Health", &combatStats.health, 0.1f, 0.0f, combatStats.maxHealth);
				ImGui::DragFloat("Max Posture", &combatStats.maxPosture, 0.1f, 1.0f, 5000.0f);
				ImGui::DragFloat("Posture", &combatStats.posture, 0.1f, 0.0f, combatStats.maxPosture);
				ImGui::DragFloat("Posture Recovery", &combatStats.postureRecoveryPerSecond, 0.1f, 0.0f, 500.0f);

				InspectorGroupLabel("Defense");
				ImGui::Checkbox("Guarding", &combatStats.guarding);
				ImGui::DragFloat("Guard Posture Mult", &combatStats.guardPostureMultiplier, 0.01f, 0.0f, 5.0f);

				InspectorGroupLabel("Runtime");
				ImGui::Text("Dead: %s", combatStats.isDead ? "Yes" : "No");
				ImGui::Text("Reaction Timer: %.2f", combatStats.reactionTimer);
				ImGui::Text("Last Damage: %.2f", combatStats.lastDamageTaken);
				ImGui::Text("Last Posture Damage: %.2f", combatStats.lastPostureDamageTaken);

				if (InspectorDangerButton("Remove Combat Stats"))
					selectedEntity->RemoveComponent<CombatStatsComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Combat Stats"))
			{
				selectedEntity->AddComponent<CombatStatsComponent>();
				if (!selectedEntity->HasComponent<CombatHurtboxComponent>())
					selectedEntity->AddComponent<CombatHurtboxComponent>();
			}
		}

		if (selectedEntity->HasComponent<BossAIComponent>())
		{
			if (BeginInspectorSection("Boss AI"))
			{
				auto& bossAI = selectedEntity->GetComponent<BossAIComponent>();
				InspectorGroupLabel("Behavior");
				ImGui::Checkbox("Enabled", &bossAI.enabled);
				ImGui::DragFloat("Approach Speed", &bossAI.approachSpeed, 0.05f, 0.0f, 20.0f);
				ImGui::DragFloat("Strafe Speed", &bossAI.strafeSpeed, 0.05f, 0.0f, 20.0f);
				ImGui::DragFloat("Desired Range", &bossAI.desiredRange, 0.05f, 0.1f, 20.0f);
				ImGui::DragFloat("Attack Range", &bossAI.attackRange, 0.05f, 0.1f, 20.0f);
				ImGui::DragFloat("Punish Range", &bossAI.punishRange, 0.05f, 0.1f, 20.0f);
				ImGui::DragFloat("Decision Interval", &bossAI.decisionInterval, 0.01f, 0.01f, 2.0f);
				ImGui::DragFloat("Punish Window", &bossAI.punishWindowSeconds, 0.01f, 0.0f, 5.0f);

				std::string targetLabel = "(auto player)";
				if (bossAI.targetEntityID != 0)
				{
					auto target = TransformHierarchy::FindEntityByID(*context.scene, bossAI.targetEntityID);
					if (target)
						targetLabel = target->GetName();
				}
				if (ImGui::BeginCombo("Target", targetLabel.c_str()))
				{
					bool autoTarget = (bossAI.targetEntityID == 0);
					if (ImGui::Selectable("(auto player)", autoTarget))
						bossAI.targetEntityID = 0;
					for (const auto& candidate : context.scene->GetEntities())
					{
						if (!candidate || candidate.get() == selectedEntity || !candidate->HasComponent<TransformComponent>())
							continue;
						const bool selected = (candidate->GetID() == bossAI.targetEntityID);
						if (ImGui::Selectable(candidate->GetName().c_str(), selected))
							bossAI.targetEntityID = candidate->GetID();
					}
					ImGui::EndCombo();
				}

				InspectorGroupLabel("Runtime");
				ImGui::Text("Distance To Target: %.2f", bossAI.distanceToTarget);
				ImGui::Text("Facing Dot: %.2f", bossAI.facingDotToTarget);
				ImGui::Text("Punish Timer: %.2f", bossAI.punishWindowTimer);

				if (InspectorDangerButton("Remove Boss AI"))
					selectedEntity->RemoveComponent<BossAIComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Boss AI"))
				selectedEntity->AddComponent<BossAIComponent>();
		}
	}

	void DrawInspectorJointSections(InspectorSectionsContext& context)
	{
		if (!context.scene || !context.selectedEntity)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<JointComponent>())
		{
			if (BeginInspectorSection("Joint"))
			{
				auto& joint = selectedEntity->GetComponent<JointComponent>();

				InspectorGroupLabel("Setup");
				ImGui::Checkbox("Enabled", &joint.enabled);

				const char* jointTypeNames[] = { "Fixed", "Spring", "Hinge" };
				int jointTypeIndex = static_cast<int>(joint.type);
				if (ImGui::Combo("Type", &jointTypeIndex, jointTypeNames, IM_ARRAYSIZE(jointTypeNames)))
					joint.type = static_cast<JointType>(jointTypeIndex);

				std::string connectedLabel = "<World Anchor>";
				if (joint.connectedEntityID != 0)
				{
					auto connected = TransformHierarchy::FindEntityByID(*context.scene, joint.connectedEntityID);
					connectedLabel = connected ? connected->GetName() : "<Missing Entity>";
				}
				if (ImGui::BeginCombo("Connected To", connectedLabel.c_str()))
				{
					bool isWorldSelected = (joint.connectedEntityID == 0);
					if (ImGui::Selectable("<World Anchor>", isWorldSelected))
						joint.connectedEntityID = 0;
					for (const auto& other : context.scene->GetEntities())
					{
						if (!other || other->GetID() == selectedEntity->GetID())
							continue;
						bool isSelected = (other->GetID() == joint.connectedEntityID);
						if (ImGui::Selectable(other->GetName().c_str(), isSelected))
							joint.connectedEntityID = other->GetID();
					}
					ImGui::EndCombo();
				}

				InspectorGroupLabel("Anchors");
				ImGui::DragFloat3("Anchor Offset", &joint.anchor.x, 0.05f);
				ImGui::DragFloat3("Connected Anchor", &joint.connectedAnchor.x, 0.05f);
				if (InspectorActionButton("Snap Anchors to Current Offset##snapAnchor"))
				{
					if (selectedEntity->HasComponent<TransformComponent>())
					{
						joint.anchor = glm::vec3(0.0f);
						if (joint.connectedEntityID != 0)
						{
							auto connected = TransformHierarchy::FindEntityByID(*context.scene, joint.connectedEntityID);
							if (connected && connected->HasComponent<TransformComponent>())
							{
								auto& selfTf = selectedEntity->GetComponent<TransformComponent>();
								auto& otherTf = connected->GetComponent<TransformComponent>();
								joint.connectedAnchor = otherTf.position - selfTf.position;
							}
						}
						else
						{
							auto& selfTf = selectedEntity->GetComponent<TransformComponent>();
							joint.connectedAnchor = selfTf.position;
						}
					}
				}
				ImGui::SetItemTooltip("Computes connected anchor as the current relative offset between the two bodies");

				if (selectedEntity->HasComponent<TransformComponent>())
				{
					auto& selfTf = selectedEntity->GetComponent<TransformComponent>();
					glm::vec3 worldAnchor = selfTf.position + joint.anchor;
					ImGui::TextDisabled("World Anchor: %.2f, %.2f, %.2f", worldAnchor.x, worldAnchor.y, worldAnchor.z);
				}

				InspectorGroupLabel("Constraint");
				if (joint.type == JointType::Spring)
				{
					ImGui::DragFloat("Rest Length", &joint.restLength, 0.05f, 0.0f, 100.0f);
					ImGui::DragFloat("Stiffness", &joint.stiffness, 0.5f, 0.0f, 1000.0f);
					ImGui::DragFloat("Damping", &joint.damping, 0.1f, 0.0f, 100.0f);
				}
				else if (joint.type == JointType::Hinge)
				{
					ImGui::DragFloat("Hinge Distance", &joint.hingeDistance, 0.05f, 0.0f, 100.0f);
				}

				InspectorGroupLabel("Break Settings");
				ImGui::DragFloat("Break Force", &joint.breakForce, 0.5f, 0.0f, 10000.0f);
				ImGui::SetItemTooltip("Joint is removed when corrective force exceeds this value. 0 = unbreakable.");
				if (joint.breakForce > 0.0f)
					ImGui::TextDisabled("Joint will break above %.1f N", joint.breakForce);

				if (InspectorDangerButton("Remove Joint"))
					selectedEntity->RemoveComponent<JointComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Joint"))
				selectedEntity->AddComponent<JointComponent>();
		}
	}

	void DrawInspectorAnimationSections(InspectorSectionsContext& context)
	{
		if (!context.selectedEntity || !context.state)
			return;

		auto* selectedEntity = context.selectedEntity;
		auto& state = *context.state;

		std::string skinnedSourceAssetPath;
		bool hasSkinnedSourceAsset = false;
		if (selectedEntity->HasComponent<MeshComponent>())
		{
			const auto& meshComponent = selectedEntity->GetComponent<MeshComponent>();
			if (!meshComponent.assetPath.empty())
			{
				std::filesystem::path meshAssetPath(meshComponent.assetPath);
				std::string ext = meshAssetPath.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				hasSkinnedSourceAsset = (ext == ".fbx" || ext == ".gltf" || ext == ".glb");
				if (hasSkinnedSourceAsset)
					skinnedSourceAssetPath = meshComponent.assetPath;
			}
		}

		if (!(selectedEntity->HasComponent<SkeletonComponent>() ||
			selectedEntity->HasComponent<AnimationComponent>() ||
			hasSkinnedSourceAsset))
		{
			return;
		}

		auto isGenericImportedName = [](const std::string& name)
		{
			if (name.empty())
				return true;

			std::string trimmed = name;
			trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
			trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

			std::string lower = trimmed;
			std::transform(lower.begin(), lower.end(), lower.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			return lower.empty() ||
				lower == "mixamo.com" ||
				lower == "mixamo_com" ||
				lower == "mixamo.com|mixamo.com";
		};

		if (!selectedEntity->HasComponent<AnimationComponent>())
		{
			if (ImGui::Button("Add Animation Component"))
			{
				auto& anim = selectedEntity->AddComponent<AnimationComponent>();
				anim.playing = true;
				anim.looping = true;
			}

			if (hasSkinnedSourceAsset)
			{
				ImGui::SameLine();
				if (ImGui::Button("Load Animations From Asset##anim"))
				{
					MyEngine::SkinnedModelData skinnedData = MyEngine::AssetManager::LoadSkinnedModel(skinnedSourceAssetPath);
					if (skinnedData.meshes.empty())
					{
						state.animationBootstrapStatus = "Failed to load model data from asset.";
					}
					else if (!skinnedData.skeleton || skinnedData.skeleton->GetBoneCount() <= 0)
					{
						state.animationBootstrapStatus = "Selected model has no skeleton/bones.";
					}
					else
					{
						auto& skeletonComponent = selectedEntity->HasComponent<SkeletonComponent>()
							? selectedEntity->GetComponent<SkeletonComponent>()
							: selectedEntity->AddComponent<SkeletonComponent>();
						skeletonComponent.skeleton = skinnedData.skeleton;

						auto& anim = selectedEntity->HasComponent<AnimationComponent>()
							? selectedEntity->GetComponent<AnimationComponent>()
							: selectedEntity->AddComponent<AnimationComponent>();
						anim.clips = skinnedData.clips;
						anim.activeClipIndex = (anim.clips && !anim.clips->empty()) ? 0 : -1;
						anim.time = 0.0f;
						anim.previousTime = 0.0f;
						anim.blendElapsed = 0.0f;
						anim.playing = true;
						anim.looping = true;

						const int clipCount = anim.clips ? static_cast<int>(anim.clips->size()) : 0;
						state.animationBootstrapStatus = "Loaded " + std::to_string(clipCount) + " clip(s) from asset.";
					}
				}

				if (!state.animationBootstrapStatus.empty())
					ImGui::TextWrapped("%s", state.animationBootstrapStatus.c_str());
			}

			return;
		}

		if (!BeginInspectorSection("Animation"))
			return;

		auto& anim = selectedEntity->GetComponent<AnimationComponent>();
		std::shared_ptr<MyEngine::Skeleton> skeleton = selectedEntity->HasComponent<SkeletonComponent>()
			? selectedEntity->GetComponent<SkeletonComponent>().skeleton
			: nullptr;
		const bool hasClips = anim.clips && !anim.clips->empty();
		AnimationStateMachineComponent* smComponent = selectedEntity->HasComponent<AnimationStateMachineComponent>()
			? &selectedEntity->GetComponent<AnimationStateMachineComponent>()
			: nullptr;

		InspectorGroupLabel("Playback");
		ImGui::Checkbox("Playing##anim", &anim.playing);
		ImGui::SameLine();
		ImGui::Checkbox("Looping##anim", &anim.looping);
		ImGui::DragFloat("Playback Speed##anim", &anim.playbackSpeed, 0.01f, 0.0f, 4.0f, "%.2f");

		if (smComponent)
		{
			InspectorGroupLabel("State Machine");
			ImGui::TextWrapped("State Machine: %s", smComponent->assetPath.empty() ? "<unsaved>" : smComponent->assetPath.c_str());
			ImGui::Checkbox("Auto Initialize##animsm", &smComponent->autoInitialize);
			ImGui::SameLine();
			ImGui::Checkbox("Pause Transitions##animsm", &smComponent->debugPauseTransitions);

			if (smComponent->stateMachine)
			{
				if (InspectorActionButton("Edit State Machine##animsm") &&
					context.editingAnimationStateMachine &&
					context.selectedAnimationStateMachinePath)
				{
					*context.editingAnimationStateMachine = smComponent->stateMachine;
					*context.selectedAnimationStateMachinePath = smComponent->assetPath;
				}
			}
			if (InspectorDangerButton("Remove State Machine##animsm"))
			{
				selectedEntity->RemoveComponent<AnimationStateMachineComponent>();
				smComponent = nullptr;
			}

			if (smComponent && smComponent->stateMachine)
			{
				auto& sm = *smComponent->stateMachine;
				ImGui::Text("Current State Index: %d", smComponent->currentStateIndex);
				if (!smComponent->debugCurrentStateName.empty())
					ImGui::Text("Current State: %s", smComponent->debugCurrentStateName.c_str());
				else if (sm.IsValidStateIndex(smComponent->currentStateIndex))
					ImGui::Text("Current State: %s", sm.states[smComponent->currentStateIndex].name.c_str());

				if (!smComponent->debugPendingStateName.empty())
					ImGui::Text("Pending Transition: %s", smComponent->debugPendingStateName.c_str());
				else
					ImGui::TextDisabled("Pending Transition: none");

				if (smComponent->debugLastBlockedTransitionIndex >= 0)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.35f, 1.0f),
						"Blocked Transition [%d]: %s",
						smComponent->debugLastBlockedTransitionIndex,
						smComponent->debugLastBlockedReason.c_str());
				}

				if (!smComponent->debugTransitionMessages.empty() &&
					ImGui::CollapsingHeader("Transition Debug##animsm", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (const auto& message : smComponent->debugTransitionMessages)
						ImGui::BulletText("%s", message.c_str());
				}

				for (size_t paramIndex = 0; paramIndex < sm.parameters.size(); ++paramIndex)
				{
					if (paramIndex >= smComponent->parameterValues.size())
						smComponent->parameterValues.resize(sm.parameters.size());
					auto& parameter = sm.parameters[paramIndex];
					auto& value = smComponent->parameterValues[paramIndex];
					if (parameter.type == MyEngine::AnimationStateMachineParameterType::Bool)
					{
						ImGui::Checkbox((parameter.name + "##animsmparam").c_str(), &value.boolValue);
					}
					else if (parameter.type == MyEngine::AnimationStateMachineParameterType::Float)
					{
						ImGui::DragFloat((parameter.name + "##animsmparam").c_str(), &value.floatValue, 0.01f);
					}
					else if (parameter.type == MyEngine::AnimationStateMachineParameterType::Trigger)
					{
						bool triggerPressed = ImGui::Button((parameter.name + "##animsmtrigger").c_str());
						ImGui::SameLine();
						ImGui::TextDisabled(value.triggerValue ? "armed" : "idle");
						if (triggerPressed)
							value.triggerValue = true;
					}
				}
			}
		}
		else
		{
			InspectorGroupLabel("State Machine");
			if (InspectorActionButton("Create State Machine##animsm"))
			{
				auto& sm = selectedEntity->AddComponent<AnimationStateMachineComponent>();
				sm.stateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
				sm.stateMachine->name = selectedEntity->GetName() + " State Machine";
				if (hasClips)
				{
					MyEngine::AnimationStateMachineState idleState;
					idleState.name = "Default";
					idleState.clipName = (*anim.clips)[std::max(anim.activeClipIndex, 0)].name;
					sm.stateMachine->states.push_back(idleState);
					sm.stateMachine->defaultStateIndex = 0;
				}
				if (context.editingAnimationStateMachine && context.selectedAnimationStateMachinePath)
				{
					*context.editingAnimationStateMachine = sm.stateMachine;
					context.selectedAnimationStateMachinePath->clear();
				}
				smComponent = &sm;
			}
			if (InspectorActionButton("Assign State Machine...##animsm"))
			{
				std::string path = MyEngine::FileDialog::OpenAnimationStateMachineFile();
				if (!path.empty())
				{
					auto stateMachine = std::make_shared<MyEngine::AnimationStateMachine>();
					if (stateMachine->LoadFromFile(path))
					{
						auto& sm = selectedEntity->AddComponent<AnimationStateMachineComponent>();
						sm.stateMachine = stateMachine;
						sm.assetPath = path;
						sm.parameterValues.clear();
						sm.currentStateIndex = stateMachine->defaultStateIndex;
						if (context.editingAnimationStateMachine && context.selectedAnimationStateMachinePath)
						{
							*context.editingAnimationStateMachine = stateMachine;
							*context.selectedAnimationStateMachinePath = path;
						}
						smComponent = &sm;
					}
				}
			}
		}

		InspectorGroupLabel("Clip Preview");
		if (hasClips)
		{
			if (anim.activeClipIndex < 0 || anim.activeClipIndex >= static_cast<int>(anim.clips->size()))
				anim.activeClipIndex = 0;

			std::string currentClipLabel = (*anim.clips)[anim.activeClipIndex].name;
			if (currentClipLabel.empty())
				currentClipLabel = "Clip " + std::to_string(anim.activeClipIndex);

			if (ImGui::BeginCombo("Active Clip##anim", currentClipLabel.c_str()))
			{
				for (int clipIndex = 0; clipIndex < static_cast<int>(anim.clips->size()); ++clipIndex)
				{
					std::string clipLabel = (*anim.clips)[clipIndex].name;
					if (clipLabel.empty())
						clipLabel = "Clip " + std::to_string(clipIndex);

					std::string selectableLabel = clipLabel + "##animclip_" + std::to_string(clipIndex);
					bool isSelected = (clipIndex == anim.activeClipIndex);
					if (ImGui::Selectable(selectableLabel.c_str(), isSelected))
					{
						if (clipIndex != anim.activeClipIndex)
							anim.TransitionTo(clipIndex, 0.2f);
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", clipLabel.c_str());
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			const auto& activeClip = (*anim.clips)[anim.activeClipIndex];
			float durationSeconds = activeClip.GetDurationSeconds();
			if (durationSeconds > 0.0001f)
			{
				anim.time = std::clamp(anim.time, 0.0f, durationSeconds);
				ImGui::SliderFloat("Time##anim", &anim.time, 0.0f, durationSeconds, "%.2fs");
				ImGui::Text("Duration: %.2fs | Ticks/Sec: %.2f | Tracks: %d",
					durationSeconds,
					activeClip.ticksPerSecond,
					static_cast<int>(activeClip.tracks.size()));
			}
			else
			{
				ImGui::TextDisabled("Active clip has no duration.");
			}
		}
		else
		{
			ImGui::TextDisabled("No animation clips assigned.");
		}

		if (InspectorActionButton("Restart Clip##anim"))
		{
			anim.time = 0.0f;
			anim.previousTime = 0.0f;
			anim.blendElapsed = 0.0f;
		}
		if (InspectorActionButton("Pause/Resume##anim"))
		{
			anim.playing = !anim.playing;
		}

		InspectorGroupLabel("Import");
		if (hasSkinnedSourceAsset && InspectorActionButton("Load Animations From Entity Asset##anim"))
		{
			MyEngine::SkinnedModelData skinnedData = MyEngine::AssetManager::LoadSkinnedModel(skinnedSourceAssetPath);
			if (!skinnedData.skeleton || skinnedData.skeleton->GetBoneCount() <= 0)
			{
				state.animationImportStatus = "Entity asset has no skeleton/bones.";
			}
			else if (!skinnedData.clips || skinnedData.clips->empty())
			{
				state.animationImportStatus = "Entity asset has no embedded animation clips.";
			}
			else
			{
				auto& skeletonComponent = selectedEntity->HasComponent<SkeletonComponent>()
					? selectedEntity->GetComponent<SkeletonComponent>()
					: selectedEntity->AddComponent<SkeletonComponent>();
				skeletonComponent.skeleton = skinnedData.skeleton;
				skeleton = skeletonComponent.skeleton;

				anim.clips = skinnedData.clips;
				anim.activeClipIndex = 0;
				anim.time = 0.0f;
				anim.previousTime = 0.0f;
				anim.blendElapsed = 0.0f;
				anim.playing = true;
				anim.looping = true;
				state.animationImportStatus = "Loaded " + std::to_string(anim.clips->size()) + " clip(s) from entity asset.";
			}
		}
		if (InspectorActionButton("Import Animation Files...##anim"))
		{
			std::vector<std::string> paths = MyEngine::FileDialog::OpenModelFiles();
			if (!paths.empty())
			{
				if (!anim.clips)
					anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();

				int importedCount = 0;
				int incompatibleCount = 0;
				int noClipFiles = 0;

				for (const auto& path : paths)
				{
					auto externalClips = MyEngine::AssetManager::LoadAnimationClips(path);
					if (!externalClips || externalClips->empty())
					{
						++noClipFiles;
						continue;
					}

					std::filesystem::path sourcePath(path);
					std::string sourceStem = sourcePath.stem().string();
					bool importedFromThisFile = false;

					for (const auto& clip : *externalClips)
					{
						if (!MyEngine::AssetManager::IsAnimationClipCompatible(clip, skeleton))
						{
							++incompatibleCount;
							continue;
						}

						MyEngine::AnimationClip importedClip = clip;
						std::string baseName = isGenericImportedName(importedClip.name)
							? sourceStem
							: importedClip.name;
						importedClip.name = baseName;

						bool duplicateName = false;
						for (const auto& existingClip : *anim.clips)
						{
							if (existingClip.name == importedClip.name)
							{
								duplicateName = true;
								break;
							}
						}
						if (duplicateName)
							importedClip.name += " [" + sourceStem + "]";

						anim.clips->push_back(std::move(importedClip));
						++importedCount;
						importedFromThisFile = true;
					}

					if (importedFromThisFile)
					{
						bool alreadyTracked = false;
						for (const auto& importedPath : anim.importedAnimationFilePaths)
						{
							if (importedPath == path)
							{
								alreadyTracked = true;
								break;
							}
						}
						if (!alreadyTracked)
							anim.importedAnimationFilePaths.push_back(path);
					}
				}

				if (importedCount > 0)
				{
					if (anim.activeClipIndex < 0)
						anim.activeClipIndex = 0;
					state.animationImportStatus = "Imported " + std::to_string(importedCount) + " clip(s) from " + std::to_string(paths.size()) + " file(s).";
					if (incompatibleCount > 0 || noClipFiles > 0)
					{
						state.animationImportStatus += " Skipped: " + std::to_string(incompatibleCount) + " incompatible clip(s), " + std::to_string(noClipFiles) + " file(s) without clips.";
					}
				}
				else
				{
					state.animationImportStatus = "No compatible clips found in selected files.";
				}
			}
		}
		ImGui::SameLine();
		if (InspectorActionButton("Retarget Animation File...##anim"))
		{
			if (!skeleton)
			{
				state.animationRetargetStatus = "Target entity has no skeleton.";
			}
			else
			{
				std::string path = MyEngine::FileDialog::OpenModelFile();
				if (!path.empty())
				{
					auto sourceData = MyEngine::AssetManager::LoadSkinnedModel(path);
					if (!sourceData.skeleton || !sourceData.clips || sourceData.clips->empty())
					{
						state.animationRetargetStatus = "No source skeleton/clips available for retargeting.";
					}
					else
					{
						auto retargeted = MyEngine::AssetManager::RetargetAnimationClips(sourceData.clips, sourceData.skeleton, skeleton);
						if (!retargeted || retargeted->empty())
						{
							state.animationRetargetStatus = "Retargeting produced no clips.";
						}
						else
						{
							if (!anim.clips)
								anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();
							int added = 0;
							for (auto& clip : *retargeted)
							{
								std::string baseName = clip.name.empty() ? "Retargeted" : clip.name;
								clip.name = baseName + " [Retargeted]";
								anim.clips->push_back(std::move(clip));
								++added;
							}
							if (anim.activeClipIndex < 0)
								anim.activeClipIndex = 0;
							state.animationRetargetStatus = "Retargeted " + std::to_string(added) + " clip(s).";
						}
					}
				}
			}
		}
		if (!state.animationImportStatus.empty())
			ImGui::TextWrapped("%s", state.animationImportStatus.c_str());
		if (!state.animationRetargetStatus.empty())
			ImGui::TextWrapped("%s", state.animationRetargetStatus.c_str());

		InspectorGroupLabel("Imported Animation Sources");
		if (anim.importedAnimationFilePaths.empty())
		{
			ImGui::TextDisabled("No imported animation source files.");
		}
		else
		{
			int reimportSourceIndex = -1;
			int removeSourceIndex = -1;

			for (size_t sourceIndex = 0; sourceIndex < anim.importedAnimationFilePaths.size(); ++sourceIndex)
			{
				const std::string& importedPath = anim.importedAnimationFilePaths[sourceIndex];
				std::filesystem::path sourcePath(importedPath);
				const std::string sourceName = sourcePath.filename().string().empty()
					? importedPath
					: sourcePath.filename().string();

				ImGui::PushID(static_cast<int>(sourceIndex));
				ImGui::TextUnformatted(sourceName.c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", importedPath.c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("Reimport"))
					reimportSourceIndex = static_cast<int>(sourceIndex);
				ImGui::SameLine();
				if (ImGui::SmallButton("Remove"))
					removeSourceIndex = static_cast<int>(sourceIndex);
				ImGui::PopID();
			}

			if (reimportSourceIndex >= 0 && reimportSourceIndex < static_cast<int>(anim.importedAnimationFilePaths.size()))
			{
				const std::string path = anim.importedAnimationFilePaths[reimportSourceIndex];
				auto externalClips = MyEngine::AssetManager::LoadAnimationClips(path);
				if (!externalClips || externalClips->empty())
				{
					state.animationImportStatus = "Reimport failed: no clips found in source file.";
				}
				else
				{
					if (!anim.clips)
						anim.clips = std::make_shared<std::vector<MyEngine::AnimationClip>>();

					std::filesystem::path sourcePath(path);
					std::string sourceStem = sourcePath.stem().string();
					int importedCount = 0;
					int incompatibleCount = 0;

					for (const auto& clip : *externalClips)
					{
						if (!MyEngine::AssetManager::IsAnimationClipCompatible(clip, skeleton))
						{
							++incompatibleCount;
							continue;
						}

						MyEngine::AnimationClip importedClip = clip;
						std::string baseName = isGenericImportedName(importedClip.name) ? sourceStem : importedClip.name;
						importedClip.name = baseName;

						bool duplicateName = false;
						for (const auto& existingClip : *anim.clips)
						{
							if (existingClip.name == importedClip.name)
							{
								duplicateName = true;
								break;
							}
						}
						if (duplicateName)
							importedClip.name += " [" + sourceStem + "]";

						anim.clips->push_back(std::move(importedClip));
						++importedCount;
					}

					if (importedCount > 0)
					{
						if (anim.activeClipIndex < 0)
							anim.activeClipIndex = 0;
						state.animationImportStatus = "Reimported " + std::to_string(importedCount) + " clip(s) from " + sourceStem + ".";
						if (incompatibleCount > 0)
							state.animationImportStatus += " Skipped " + std::to_string(incompatibleCount) + " incompatible clip(s).";
					}
					else
					{
						state.animationImportStatus = "Reimport found no compatible clips for this skeleton.";
					}
				}
			}

			if (removeSourceIndex >= 0 && removeSourceIndex < static_cast<int>(anim.importedAnimationFilePaths.size()))
			{
				const std::string removedPath = anim.importedAnimationFilePaths[removeSourceIndex];
				anim.importedAnimationFilePaths.erase(anim.importedAnimationFilePaths.begin() + removeSourceIndex);
				state.animationImportStatus = "Removed imported source file from scene persistence: " + removedPath;
			}
		}

		InspectorGroupLabel("Event Track");
		static char newAnimEventName[64] = "Footstep";
		static float newAnimEventTime = 0.0f;
		ImGui::SetNextItemWidth(220.0f);
		ImGui::InputText("Event Name##animEvent", newAnimEventName, sizeof(newAnimEventName));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat("Time (s)##animEvent", &newAnimEventTime, 0.01f, 0.0f, 600.0f, "%.2f");
		ImGui::SameLine();
		if (InspectorActionButton("Add Event##animEvent"))
		{
			AnimationComponent::AnimationEvent ev;
			ev.timeSeconds = std::max(0.0f, newAnimEventTime);
			ev.name = newAnimEventName;
			ev.enabled = true;
			if (!ev.name.empty())
			{
				anim.events.push_back(ev);
				std::sort(anim.events.begin(), anim.events.end(),
					[](const AnimationComponent::AnimationEvent& a, const AnimationComponent::AnimationEvent& b)
					{
						return a.timeSeconds < b.timeSeconds;
					});
			}
		}

		for (size_t eventIndex = 0; eventIndex < anim.events.size(); ++eventIndex)
		{
			auto& ev = anim.events[eventIndex];
			ImGui::PushID(static_cast<int>(eventIndex));
			ImGui::Checkbox("##enabled", &ev.enabled);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(180.0f);
			char eventNameBuffer[128] = {};
			std::strncpy(eventNameBuffer, ev.name.c_str(), sizeof(eventNameBuffer) - 1);
			eventNameBuffer[sizeof(eventNameBuffer) - 1] = '\0';
			if (ImGui::InputText("##name", eventNameBuffer, sizeof(eventNameBuffer)))
				ev.name = eventNameBuffer;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(110.0f);
			ImGui::DragFloat("##time", &ev.timeSeconds, 0.01f, 0.0f, 600.0f, "%.2f");

			if (ImGui::TreeNode("Actions"))
			{
				ImGui::Checkbox("Play Audio", &ev.triggerAudio);
				if (ev.triggerAudio)
				{
					char audioPathBuffer[256] = {};
					std::strncpy(audioPathBuffer, ev.audioClipPath.c_str(), sizeof(audioPathBuffer) - 1);
					audioPathBuffer[sizeof(audioPathBuffer) - 1] = '\0';
					if (ImGui::InputText("Audio Clip Path", audioPathBuffer, sizeof(audioPathBuffer)))
						ev.audioClipPath = audioPathBuffer;
					ImGui::SliderFloat("Audio Volume", &ev.audioVolume, 0.0f, 1.0f);
					ImGui::DragFloat("Audio Pitch", &ev.audioPitch, 0.01f, 0.1f, 4.0f, "%.2f");
				}

				ImGui::Checkbox("Particle Burst", &ev.triggerParticleBurst);
				if (ev.triggerParticleBurst)
					ImGui::DragInt("Burst Count", &ev.particleBurstCount, 1.0f, 1, 2048);

				ImGui::Checkbox("Script Callback", &ev.triggerScriptCallback);
				if (ev.triggerScriptCallback)
				{
					char callbackBuffer[128] = {};
					std::strncpy(callbackBuffer, ev.scriptCallbackName.c_str(), sizeof(callbackBuffer) - 1);
					callbackBuffer[sizeof(callbackBuffer) - 1] = '\0';
					if (ImGui::InputText("Callback Name", callbackBuffer, sizeof(callbackBuffer)))
						ev.scriptCallbackName = callbackBuffer;
					if (ev.scriptCallbackName.empty())
						ev.scriptCallbackName = "OnAnimationEvent";
				}

				ev.audioVolume = std::clamp(ev.audioVolume, 0.0f, 1.0f);
				ev.audioPitch = std::clamp(ev.audioPitch, 0.1f, 4.0f);
				ev.particleBurstCount = std::max(ev.particleBurstCount, 1);
				ImGui::TreePop();
			}

			if (InspectorDangerButton("X##removeEvent"))
			{
				anim.events.erase(anim.events.begin() + static_cast<std::ptrdiff_t>(eventIndex));
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}

		if (!anim.triggeredEventsThisFrame.empty())
		{
			ImGui::TextDisabled("Triggered This Frame:");
			for (const auto& eventName : anim.triggeredEventsThisFrame)
				ImGui::BulletText("%s", eventName.c_str());
		}

		const auto& runtimeAnimEvents = MyEngine::AnimationEventBus::GetRecentEvents();
		if (!runtimeAnimEvents.empty() && ImGui::CollapsingHeader("Runtime Animation Events", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const int maxShown = std::min<int>(12, static_cast<int>(runtimeAnimEvents.size()));
			for (int i = static_cast<int>(runtimeAnimEvents.size()) - maxShown; i < static_cast<int>(runtimeAnimEvents.size()); ++i)
			{
				const auto& evt = runtimeAnimEvents[static_cast<size_t>(i)];
				ImGui::BulletText("[%u] %s -> %s @ %.2fs",
					evt.entityID,
					evt.entityName.c_str(),
					evt.eventName.c_str(),
					evt.eventTimeSeconds);
			}
		}
	}

	void DrawInspectorAudioSections(InspectorSectionsContext& context)
	{
		if (!context.selectedEntity)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<AudioSourceComponent>())
		{
			if (BeginInspectorSection("Audio Source"))
			{
				auto& source = selectedEntity->GetComponent<AudioSourceComponent>();

				InspectorGroupLabel("Clip");
				if (source.clip)
					ImGui::TextWrapped("Clip: %s", source.clipPath.c_str());
				else
					ImGui::TextDisabled("No clip loaded");

				static std::vector<std::string> availableClips;
				static bool clipsScanned = false;
				if (!clipsScanned)
				{
					clipsScanned = true;
					availableClips.clear();
					const std::string audioDir = "assets/audio";
					if (std::filesystem::exists(audioDir))
					{
						for (const auto& entry : std::filesystem::directory_iterator(audioDir))
						{
							if (entry.is_regular_file() && entry.path().extension() == ".wav")
								availableClips.push_back(entry.path().generic_string());
						}
					}
				}

				if (InspectorActionButton("Rescan Clips##audio"))
					clipsScanned = false;

				if (ImGui::BeginCombo("Clip##audioClipCombo", source.clipPath.empty() ? "<select clip>" : source.clipPath.c_str()))
				{
					for (const auto& clipPath : availableClips)
					{
						bool isSelected = (clipPath == source.clipPath);
						if (ImGui::Selectable(clipPath.c_str(), isSelected))
						{
							auto newClip = AssetManager::LoadAudioClip(clipPath);
							if (newClip && newClip->IsValid())
							{
								if (source.sourceID != 0)
								{
									alSourceStop(source.sourceID);
									alDeleteSources(1, &source.sourceID);
									source.sourceID = 0;
								}
								source.clip = newClip;
								source.clipPath = clipPath;
								source.isPlaying = false;
							}
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				InspectorGroupLabel("Playback");
				ImGui::SliderFloat("Volume", &source.volume, 0.0f, 1.0f);
				ImGui::DragFloat("Pitch", &source.pitch, 0.01f, 0.1f, 4.0f);
				ImGui::Checkbox("Loop", &source.loop);
				ImGui::Checkbox("Auto Play", &source.autoPlay);

				InspectorGroupLabel("Spatial");
				ImGui::Checkbox("Spatial (3D)", &source.spatial);
				if (source.spatial)
				{
					ImGui::DragFloat("Min Distance", &source.minDistance, 0.1f, 0.1f, 1000.0f);
					ImGui::DragFloat("Max Distance", &source.maxDistance, 1.0f, 1.0f, 10000.0f);
				}

				InspectorGroupLabel("Transport");
				if (source.clip && source.clip->IsValid())
				{
					if (!source.isPlaying)
					{
						if (InspectorActionButton("Play##audio"))
							source.playRequested = true;
					}
					else
					{
						if (InspectorDangerButton("Stop##audio"))
							source.stopRequested = true;
					}
					ImGui::TextDisabled(source.isPlaying ? "Status: Playing" : "Status: Stopped");
				}
				else
				{
					ImGui::TextDisabled("Status: waiting for a valid clip");
				}

				if (InspectorDangerButton("Remove Audio Source"))
				{
					if (source.sourceID != 0)
					{
						alSourceStop(source.sourceID);
						alDeleteSources(1, &source.sourceID);
						source.sourceID = 0;
					}
					selectedEntity->RemoveComponent<AudioSourceComponent>();
				}
			}
		}
		else
		{
			if (InspectorActionButton("Add Audio Source"))
				selectedEntity->AddComponent<AudioSourceComponent>();
		}
	}

	void DrawInspectorAudioListenerSections(InspectorSectionsContext& context)
	{
		if (!context.selectedEntity)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<AudioListenerComponent>())
		{
			if (ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& listener = selectedEntity->GetComponent<AudioListenerComponent>();
				ImGui::Checkbox("Primary Listener", &listener.isPrimary);
				ImGui::SliderFloat("Gain", &listener.gain, 0.0f, 2.0f);

				ImGui::Separator();
				ImGui::Text("Master Audio");
				float masterVolume = MyEngine::AudioEngine::GetMasterVolume();
				if (ImGui::SliderFloat("Master Volume", &masterVolume, 0.0f, 1.0f))
					MyEngine::AudioEngine::SetMasterVolume(masterVolume);
				bool muted = MyEngine::AudioEngine::IsMuted();
				if (ImGui::Checkbox("Mute", &muted))
					MyEngine::AudioEngine::SetMuted(muted);

				if (ImGui::Button("Remove Audio Listener"))
					selectedEntity->RemoveComponent<AudioListenerComponent>();
			}
		}
		else
		{
			if (ImGui::Button("Add Audio Listener"))
				selectedEntity->AddComponent<AudioListenerComponent>();
		}
	}

	void DrawInspectorScriptSections(InspectorSectionsContext& context)
	{
		if (!context.selectedEntity)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<ScriptComponent>())
		{
			if (BeginInspectorSection("Script"))
			{
				auto& script = selectedEntity->GetComponent<ScriptComponent>();

				InspectorGroupLabel("Runtime");
				ImGui::Checkbox("Enabled##script", &script.enabled);
				ImGui::Checkbox("Auto Start##script", &script.autoStart);

				InspectorGroupLabel("Source");
				if (script.scriptPath.find("rin_animation_hotkeys.lua") != std::string::npos)
					ImGui::TextWrapped("Hotkeys: press 1-9 to switch imported animation clips for this character.");

				std::string scriptDisplay = script.scriptPath.empty() ? "(none)" : script.scriptPath;
				ImGui::TextWrapped("Path: %s", scriptDisplay.c_str());
				ImGui::TextDisabled("Status: %s", script.scriptPath.empty() ? "No script assigned" : (script.requestReload ? "Reload pending" : "Ready"));

				if (InspectorActionButton("Browse Script...##script"))
				{
					std::string path = MyEngine::FileDialog::OpenScriptFile();
					if (!path.empty())
					{
						script.scriptPath = path;
						script.requestReload = true;
					}
				}
				if (InspectorActionButton("Reload Script##script"))
					script.requestReload = true;

				if (InspectorActionButton("Clear Script Path##script"))
				{
					script.scriptPath.clear();
					script.requestReload = true;
				}

				if (InspectorDangerButton("Remove Script Component"))
					selectedEntity->RemoveComponent<ScriptComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Script Component"))
				selectedEntity->AddComponent<ScriptComponent>();
		}
	}

	void DrawInspectorParticleSections(InspectorSectionsContext& context)
	{
		if (!context.selectedEntity)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<ParticleEmitterComponent>())
		{
			if (BeginInspectorSection("Particle Emitter"))
			{
				auto& emitter = selectedEntity->GetComponent<ParticleEmitterComponent>();

				InspectorGroupLabel("Simulation");
				bool emitting = emitter.emitting;
				if (ImGui::Checkbox("Preview Emission", &emitting))
				{
					emitter.emitting = emitting;
					emitter.poolDirty = true;
				}
				ImGui::TextDisabled("Toggle live particle spawning for preview.");

				if (ImGui::SliderInt("Max Particles", &emitter.maxParticles, 1, 10000))
					emitter.poolDirty = true;

				ImGui::SliderFloat("Spawn Rate", &emitter.spawnRate, 0.0f, 500.0f);
				ImGui::SliderFloat("Lifetime", &emitter.lifetime, 0.05f, 20.0f);
				ImGui::SliderFloat("Lifetime Variance", &emitter.lifetimeVariance, 0.0f, 5.0f);

				InspectorGroupLabel("Shape");
				int shape = static_cast<int>(emitter.shape);
				const char* shapeNames[] = { "Point", "Sphere", "Box", "Cone" };
				if (ImGui::Combo("Shape", &shape, shapeNames, IM_ARRAYSIZE(shapeNames)))
					emitter.shape = static_cast<ParticleEmitterComponent::EmissionShape>(shape);
				switch (emitter.shape)
				{
				case ParticleEmitterComponent::EmissionShape::Sphere:
					ImGui::SliderFloat("Radius##shape", &emitter.shapeRadius, 0.0f, 10.0f);
					break;
				case ParticleEmitterComponent::EmissionShape::Box:
					ImGui::SliderFloat3("Box Extents##shape", &emitter.shapeExtents.x, 0.0f, 10.0f);
					break;
				case ParticleEmitterComponent::EmissionShape::Cone:
					ImGui::SliderFloat("Cone Radius##shape", &emitter.shapeRadius, 0.0f, 10.0f);
					ImGui::SliderFloat("Cone Height##shape", &emitter.shapeHeight, 0.0f, 20.0f);
					break;
				case ParticleEmitterComponent::EmissionShape::Point:
				default:
					ImGui::TextDisabled("Using emitter origin");
					break;
				}

				InspectorGroupLabel("Rendering");
				int blendMode = static_cast<int>(emitter.blendMode);
				const char* blendNames[] = { "Alpha", "Additive" };
				if (ImGui::Combo("Blend Mode", &blendMode, blendNames, IM_ARRAYSIZE(blendNames)))
					emitter.blendMode = static_cast<ParticleEmitterComponent::BlendMode>(blendMode);

				InspectorGroupLabel("Emission");
				ImGui::SliderFloat3("Direction##emit", &emitter.emitDirection.x, -1.0f, 1.0f);
				ImGui::SliderFloat("Speed", &emitter.emitSpeed, 0.0f, 30.0f);
				ImGui::SliderFloat("Speed Variance", &emitter.emitSpeedVariance, 0.0f, 10.0f);
				ImGui::SliderFloat("Spread Angle", &emitter.spreadAngle, 0.0f, 180.0f);
				ImGui::SliderFloat3("Gravity##emit", &emitter.gravity.x, -20.0f, 20.0f);

				InspectorGroupLabel("Appearance");
				ImGui::ColorEdit4("Color Start", &emitter.colorStart.r);
				ImGui::ColorEdit4("Color End", &emitter.colorEnd.r);
				ImGui::SliderFloat("Size Start", &emitter.sizeStart, 0.0f, 5.0f);
				ImGui::SliderFloat("Size End", &emitter.sizeEnd, 0.0f, 5.0f);

				InspectorGroupLabel("Presets");
				if (InspectorActionButton("Fire##particlePreset"))
				{
					emitter.emitting = true;
					emitter.shape = ParticleEmitterComponent::EmissionShape::Cone;
					emitter.shapeRadius = 0.35f;
					emitter.shapeHeight = 0.6f;
					emitter.spawnRate = 80.0f;
					emitter.lifetime = 1.0f;
					emitter.lifetimeVariance = 0.3f;
					emitter.emitDirection = glm::vec3(0.0f, 1.0f, 0.0f);
					emitter.emitSpeed = 3.5f;
					emitter.emitSpeedVariance = 1.0f;
					emitter.spreadAngle = 18.0f;
					emitter.colorStart = glm::vec4(1.0f, 0.75f, 0.2f, 1.0f);
					emitter.colorEnd = glm::vec4(0.9f, 0.1f, 0.0f, 0.0f);
					emitter.sizeStart = 0.25f;
					emitter.sizeEnd = 0.0f;
					emitter.gravity = glm::vec3(0.0f, 1.0f, 0.0f);
					emitter.poolDirty = true;
				}
				if (InspectorActionButton("Smoke##particlePreset"))
				{
					emitter.emitting = true;
					emitter.shape = ParticleEmitterComponent::EmissionShape::Sphere;
					emitter.shapeRadius = 0.4f;
					emitter.spawnRate = 20.0f;
					emitter.lifetime = 4.0f;
					emitter.lifetimeVariance = 1.5f;
					emitter.emitDirection = glm::vec3(0.0f, 1.0f, 0.0f);
					emitter.emitSpeed = 0.8f;
					emitter.emitSpeedVariance = 0.4f;
					emitter.spreadAngle = 55.0f;
					emitter.colorStart = glm::vec4(0.35f, 0.35f, 0.35f, 0.7f);
					emitter.colorEnd = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
					emitter.sizeStart = 0.35f;
					emitter.sizeEnd = 1.2f;
					emitter.gravity = glm::vec3(0.0f, 0.3f, 0.0f);
					emitter.poolDirty = true;
				}
				if (InspectorActionButton("Sparks##particlePreset"))
				{
					emitter.emitting = true;
					emitter.shape = ParticleEmitterComponent::EmissionShape::Point;
					emitter.spawnRate = 150.0f;
					emitter.lifetime = 0.6f;
					emitter.lifetimeVariance = 0.2f;
					emitter.emitDirection = glm::vec3(0.0f, 1.0f, 0.0f);
					emitter.emitSpeed = 8.0f;
					emitter.emitSpeedVariance = 3.0f;
					emitter.spreadAngle = 65.0f;
					emitter.colorStart = glm::vec4(1.0f, 0.9f, 0.3f, 1.0f);
					emitter.colorEnd = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f);
					emitter.sizeStart = 0.08f;
					emitter.sizeEnd = 0.0f;
					emitter.gravity = glm::vec3(0.0f, -8.0f, 0.0f);
					emitter.poolDirty = true;
				}

				InspectorGroupLabel("Texture");
				static char particleTexturePathBuf[256] = "";
				if (particleTexturePathBuf[0] == '\0' && !emitter.texturePath.empty())
					std::snprintf(particleTexturePathBuf, sizeof(particleTexturePathBuf), "%s", emitter.texturePath.c_str());
				if (ImGui::InputText("Texture Path", particleTexturePathBuf, sizeof(particleTexturePathBuf)))
				{
					emitter.texturePath = particleTexturePathBuf;
					emitter.poolDirty = true;
				}
				if (InspectorActionButton("Browse Texture##particle"))
				{
					std::string picked = MyEngine::FileDialog::OpenImageFile();
					if (!picked.empty())
					{
						emitter.texturePath = picked;
						std::snprintf(particleTexturePathBuf, sizeof(particleTexturePathBuf), "%s", picked.c_str());
						emitter.poolDirty = true;
					}
				}
				if (InspectorActionButton("Clear Texture##particle"))
				{
					emitter.texturePath.clear();
					particleTexturePathBuf[0] = '\0';
					emitter.poolDirty = true;
				}

				if (!emitter.texturePath.empty())
					ImGui::TextWrapped("%s", emitter.texturePath.c_str());

				InspectorGroupLabel("Debug");
				int alive = 0;
				for (const auto& p : emitter.particles)
					if (p.alive)
						++alive;
				ImGui::Text("Alive: %d / %d", alive, emitter.maxParticles);

				if (InspectorDangerButton("Remove Particle Emitter"))
					selectedEntity->RemoveComponent<ParticleEmitterComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Particle Emitter"))
			{
				auto& emitter = selectedEntity->AddComponent<ParticleEmitterComponent>();
				emitter.poolDirty = true;
			}
		}
	}

	void DrawInspectorPhysicsSections(InspectorSectionsContext& context)
	{
		if (!context.selectedEntity)
			return;

		auto* selectedEntity = context.selectedEntity;

		if (selectedEntity->HasComponent<RigidbodyComponent>())
		{
			if (BeginInspectorSection("Rigidbody"))
			{
				auto& rb = selectedEntity->GetComponent<RigidbodyComponent>();

				InspectorGroupLabel("Material");
				ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.1f, 1000.0f);
				ImGui::DragFloat("Drag", &rb.drag, 0.01f, 0.0f, 10.0f);
				ImGui::SliderFloat("Bounciness", &rb.bounciness, 0.0f, 1.0f);

				InspectorGroupLabel("Motion");
				ImGui::Checkbox("Use Gravity", &rb.useGravity);
				ImGui::DragFloat("Gravity Scale", &rb.gravityScale, 0.1f, -10.0f, 10.0f);
				ImGui::Checkbox("Kinematic", &rb.isKinematic);
				ImGui::Checkbox("CCD (Continuous Collision)", &rb.useCCD);
				ImGui::SetItemTooltip("Sub-steps this body's sweep each tick to prevent tunnelling at high speeds");

				InspectorGroupLabel("Constraints");
				ImGui::TextDisabled("Freeze Position");
				ImGui::Checkbox("X##freezeX", &rb.freezePositionX); ImGui::SameLine();
				ImGui::Checkbox("Y##freezeY", &rb.freezePositionY); ImGui::SameLine();
				ImGui::Checkbox("Z##freezeZ", &rb.freezePositionZ);

				InspectorGroupLabel("Debug");
				ImGui::Text("Velocity:  %.2f, %.2f, %.2f", rb.velocity.x, rb.velocity.y, rb.velocity.z);

				auto& transform = selectedEntity->GetComponent<TransformComponent>();
				ImGui::Text("Position:  %.2f, %.2f, %.2f",
					transform.position.x, transform.position.y, transform.position.z);

				if (selectedEntity->HasComponent<BoundingSphereComponent>())
				{
					auto& bs = selectedEntity->GetComponent<BoundingSphereComponent>();
					ImGui::Text("Sphere Radius: %.2f", bs.radius);
					ImGui::Text("Bottom Y: %.2f", transform.position.y - bs.radius);
				}
				if (selectedEntity->HasComponent<BoxColliderComponent>())
				{
					auto& box = selectedEntity->GetComponent<BoxColliderComponent>();
					ImGui::Text("Box Half-Extents: %.2f, %.2f, %.2f", box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);
					ImGui::Text("Bottom Y: %.2f", transform.position.y + box.center.y - box.halfExtents.y);
				}

				if (InspectorActionButton("Reset Velocity"))
					rb.velocity = glm::vec3(0.0f);
			}
		}
		else
		{
			if (InspectorActionButton("Add Rigidbody"))
			{
				selectedEntity->AddComponent<RigidbodyComponent>();
				if (!selectedEntity->HasComponent<BoundingSphereComponent>() &&
					!selectedEntity->HasComponent<BoxColliderComponent>())
				{
					selectedEntity->AddComponent<BoundingSphereComponent>();
				}
			}
		}

		if (selectedEntity->HasComponent<BoxColliderComponent>())
		{
			if (BeginInspectorSection("Box Collider"))
			{
				auto& box = selectedEntity->GetComponent<BoxColliderComponent>();
				InspectorGroupLabel("Shape");
				ImGui::DragFloat3("Center Offset", &box.center.x, 0.05f);
				ImGui::DragFloat3("Half Extents", &box.halfExtents.x, 0.05f, 0.01f, 100.0f);
				InspectorGroupLabel("Behavior");
				ImGui::Checkbox("Is Trigger", &box.isTrigger);
				if (InspectorDangerButton("Remove Box Collider"))
					selectedEntity->RemoveComponent<BoxColliderComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Box Collider"))
			{
				auto& box = selectedEntity->AddComponent<BoxColliderComponent>();
				box.halfExtents = glm::vec3(0.5f);
				if (selectedEntity->HasComponent<BoundingSphereComponent>())
					selectedEntity->RemoveComponent<BoundingSphereComponent>();
			}
		}

		if (selectedEntity->HasComponent<CapsuleColliderComponent>())
		{
			if (BeginInspectorSection("Capsule Collider"))
			{
				auto& capsule = selectedEntity->GetComponent<CapsuleColliderComponent>();
				InspectorGroupLabel("Shape");
				ImGui::DragFloat3("Point A", &capsule.pointA.x, 0.02f);
				ImGui::DragFloat3("Point B", &capsule.pointB.x, 0.02f);
				ImGui::DragFloat("Radius", &capsule.radius, 0.01f, 0.01f, 100.0f, "%.3f");
				InspectorGroupLabel("Behavior");
				ImGui::Checkbox("Is Trigger##capsule", &capsule.isTrigger);

				if (selectedEntity->HasComponent<SkeletonComponent>())
				{
					InspectorGroupLabel("Character Tools");
					if (InspectorActionButton("Auto-Refit From Skeleton##capsule"))
					{
						auto skeleton = selectedEntity->GetComponent<SkeletonComponent>().skeleton;
						glm::vec3 fitA(0.0f), fitB(0.0f);
						float fitRadius = 0.0f;
						if (MyEngine::AssetManager::ComputeCharacterCapsuleFromSkeleton(skeleton, fitA, fitB, fitRadius))
						{
							capsule.pointA = fitA;
							capsule.pointB = fitB;
							capsule.radius = fitRadius;
						}
					}
					ImGui::TextDisabled("Use auto-refit for imported characters, then fine-tune manually.");
				}

				if (InspectorDangerButton("Remove Capsule Collider"))
					selectedEntity->RemoveComponent<CapsuleColliderComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Capsule Collider"))
			{
				auto& capsule = selectedEntity->AddComponent<CapsuleColliderComponent>();
				capsule.pointA = glm::vec3(0.0f, -0.4f, 0.0f);
				capsule.pointB = glm::vec3(0.0f, 0.4f, 0.0f);
				capsule.radius = 0.5f;
				if (selectedEntity->HasComponent<BoundingSphereComponent>())
					selectedEntity->RemoveComponent<BoundingSphereComponent>();
			}
		}

		if (selectedEntity->HasComponent<MeshColliderComponent>())
		{
			if (BeginInspectorSection("Mesh Collider"))
			{
				auto& mesh = selectedEntity->GetComponent<MeshColliderComponent>();
				InspectorGroupLabel("Source");
				ImGui::Checkbox("Is Trigger##meshcol", &mesh.isTrigger);
				ImGui::TextDisabled("Triangles: %d", static_cast<int>(mesh.triangles.size()));
				if (!mesh.modelPath.empty())
					ImGui::TextWrapped("Source: %s", mesh.modelPath.c_str());

				if (InspectorActionButton("Build from Entity Mesh##meshcol"))
				{
					if (selectedEntity->HasComponent<MeshComponent>())
					{
						auto& mc = selectedEntity->GetComponent<MeshComponent>();
						mesh.triangles.clear();
						mesh.modelPath = mc.assetPath;
						if (mc.mesh)
						{
							const auto& verts = mc.mesh->GetVertices();
							const auto& indices = mc.mesh->GetIndices();
							for (size_t ti = 0; ti + 2 < indices.size(); ti += 3)
							{
								std::array<glm::vec3, 3> tri;
								tri[0] = verts[indices[ti + 0]].Position;
								tri[1] = verts[indices[ti + 1]].Position;
								tri[2] = verts[indices[ti + 2]].Position;
								mesh.triangles.push_back(tri);
							}
						}
						mesh.RebuildAABB();
					}
				}
				ImGui::SetItemTooltip("Extracts collision triangles from the entity's MeshComponent");

				if (InspectorActionButton("Clear Triangles##meshcol"))
				{
					mesh.triangles.clear();
					mesh.RebuildAABB();
				}
				if (InspectorDangerButton("Remove Mesh Collider##meshcol"))
					selectedEntity->RemoveComponent<MeshColliderComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Mesh Collider"))
				selectedEntity->AddComponent<MeshColliderComponent>();
		}

		if (selectedEntity->HasComponent<CollisionEventsComponent>())
		{
			if (BeginInspectorSection("Collision Events"))
			{
				InspectorGroupLabel("Diagnostics");
				ImGui::TextWrapped("Logs collision and trigger enter/exit events to the console for gameplay debugging.");
				if (InspectorDangerButton("Remove Collision Events"))
					selectedEntity->RemoveComponent<CollisionEventsComponent>();
			}
		}
		else
		{
			if (InspectorActionButton("Add Collision Events"))
			{
				auto& events = selectedEntity->AddComponent<CollisionEventsComponent>();
				uint32_t selfID = selectedEntity->GetID();
				std::string selfName = selectedEntity->GetName();
				events.onCollisionEnter = [selfID, selfName](const std::shared_ptr<Entity>& other)
				{
					std::cout << "[Collision] " << selfName << " (id " << selfID << ") entered collision with "
						<< (other ? other->GetName() : "unknown") << std::endl;
				};
				events.onCollisionExit = [selfID, selfName](const std::shared_ptr<Entity>& other)
				{
					std::cout << "[Collision] " << selfName << " (id " << selfID << ") exited collision with "
						<< (other ? other->GetName() : "unknown") << std::endl;
				};
				events.onTriggerEnter = [selfID, selfName](const std::shared_ptr<Entity>& other)
				{
					std::cout << "[Trigger] " << selfName << " (id " << selfID << ") entered trigger with "
						<< (other ? other->GetName() : "unknown") << std::endl;
				};
				events.onTriggerExit = [selfID, selfName](const std::shared_ptr<Entity>& other)
				{
					std::cout << "[Trigger] " << selfName << " (id " << selfID << ") exited trigger with "
						<< (other ? other->GetName() : "unknown") << std::endl;
				};
			}
		}
	}
}
#endif
