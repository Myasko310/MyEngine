#include "AnimationStateMachinePanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "animation/AnimationStateMachine.h"
#include "components/AnimationComponent.h"
#include "components/AnimationStateMachineComponent.h"
#include "core/FileDialog.h"
#include "editor/EditorStyle.h"

namespace MyEngine::Editor::Panels
{
	void DrawAnimationStateMachinePanel(Context& context)
	{
		if (!context.ui || !context.selectedEntity)
			return;

		auto& ui = *context.ui;
		auto* selectedEntity = *context.selectedEntity;
		if (!ui.editingAnimationStateMachine)
			return;

		ImGui::SetNextWindowPos(ImVec2(1010, 440), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(540, 560), ImGuiCond_FirstUseEver);
		bool openAnimationStateMachineEditor = true;
		ImGui::Begin("Animation State Machine", &openAnimationStateMachineEditor);
		if (!openAnimationStateMachineEditor)
		{
			ui.editingAnimationStateMachine.reset();
			ui.selectedAnimationStateMachinePath.clear();
		}
		else
		{
			auto& sm = *ui.editingAnimationStateMachine;
			static char smNameBuffer[256] = "";
			static std::string smLastPath;
			if (smLastPath != ui.selectedAnimationStateMachinePath)
			{
				smLastPath = ui.selectedAnimationStateMachinePath;
				std::strncpy(smNameBuffer, sm.name.c_str(), sizeof(smNameBuffer) - 1);
				smNameBuffer[sizeof(smNameBuffer) - 1] = '\0';
			}

			ImGui::TextWrapped("Path: %s", ui.selectedAnimationStateMachinePath.empty() ? "Unsaved state machine" : ui.selectedAnimationStateMachinePath.c_str());
			static char animFilter[64] = "";
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##animsmFilter", "Filter parameters, states, or transitions", animFilter, sizeof(animFilter));
			InspectorFullWidth();
			if (ImGui::InputText("Name##animsm", smNameBuffer, sizeof(smNameBuffer)))
				sm.name = smNameBuffer;

			if (InspectorActionButton("Save##animsm"))
			{
				std::string savePath = ui.selectedAnimationStateMachinePath.empty()
					? MyEngine::FileDialog::SaveAnimationStateMachineFile()
					: ui.selectedAnimationStateMachinePath;
				if (!savePath.empty())
				{
					sm.SetPath(savePath);
					sm.SaveToFile(savePath);
					ui.selectedAnimationStateMachinePath = savePath;
				}
			}
			if (InspectorActionButton("Save As...##animsm"))
			{
				std::string savePath = MyEngine::FileDialog::SaveAnimationStateMachineFile();
				if (!savePath.empty())
				{
					sm.SetPath(savePath);
					sm.SaveToFile(savePath);
					ui.selectedAnimationStateMachinePath = savePath;
				}
			}

			const std::vector<MyEngine::AnimationClip>* stateMachineEditorClips = nullptr;
			if (selectedEntity && selectedEntity->HasComponent<AnimationComponent>())
			{
				auto& selectedAnim = selectedEntity->GetComponent<AnimationComponent>();
				if (selectedAnim.clips && !selectedAnim.clips->empty())
					stateMachineEditorClips = selectedAnim.clips.get();
			}

			InspectorGroupLabel("Clip Source");
			if (stateMachineEditorClips)
				ImGui::TextDisabled("Clip source: selected entity (%d clip(s))", static_cast<int>(stateMachineEditorClips->size()));
			else
				ImGui::TextDisabled("Select an animated entity to use clip dropdowns and validation.");

			if (ImGui::CollapsingHeader("Parameters##animsm", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (InspectorActionButton("Add Bool##animsmParam"))
					sm.parameters.push_back({ "NewBool", MyEngine::AnimationStateMachineParameterType::Bool, 0.0f, false });
				if (InspectorActionButton("Add Float##animsmParam"))
					sm.parameters.push_back({ "NewFloat", MyEngine::AnimationStateMachineParameterType::Float, 0.0f, false });
				if (InspectorActionButton("Add Trigger##animsmParam"))
					sm.parameters.push_back({ "NewTrigger", MyEngine::AnimationStateMachineParameterType::Trigger, 0.0f, false });

				const char* paramTypeNames[] = { "Bool", "Float", "Trigger" };
				for (size_t i = 0; i < sm.parameters.size(); ++i)
				{
					auto& param = sm.parameters[i];
					std::string filterValue = param.name;
					filterValue += " ";
					filterValue += paramTypeNames[static_cast<int>(param.type)];
					if (animFilter[0] != '\0')
					{
						std::string lhs = filterValue;
						std::string rhs = animFilter;
						std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
						if (lhs.find(rhs) == std::string::npos)
							continue;
					}
					ImGui::PushID(static_cast<int>(i));
					char paramNameBuffer[128] = {};
					std::strncpy(paramNameBuffer, param.name.c_str(), sizeof(paramNameBuffer) - 1);
					if (ImGui::InputText("Name", paramNameBuffer, sizeof(paramNameBuffer)))
						param.name = paramNameBuffer;
					int typeIndex = static_cast<int>(param.type);
					if (ImGui::Combo("Type", &typeIndex, paramTypeNames, IM_ARRAYSIZE(paramTypeNames)))
						param.type = static_cast<MyEngine::AnimationStateMachineParameterType>(typeIndex);
					if (param.type == MyEngine::AnimationStateMachineParameterType::Float)
						ImGui::DragFloat("Default", &param.defaultFloatValue, 0.01f);
					else if (param.type == MyEngine::AnimationStateMachineParameterType::Bool)
						ImGui::Checkbox("Default", &param.defaultBoolValue);
					if (InspectorDangerButton("Remove Parameter"))
					{
						sm.parameters.erase(sm.parameters.begin() + static_cast<std::ptrdiff_t>(i));
						ImGui::PopID();
						break;
					}
					ImGui::Separator();
					ImGui::PopID();
				}
			}

			if (ImGui::CollapsingHeader("States##animsm", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (InspectorActionButton("Add State##animsmState"))
				{
					MyEngine::AnimationStateMachineState state;
					state.name = "State " + std::to_string(sm.states.size());
					sm.states.push_back(state);
					if (sm.defaultStateIndex < 0)
						sm.defaultStateIndex = 0;
				}

				if (!sm.states.empty())
					ImGui::DragInt("Default State Index##animsm", &sm.defaultStateIndex, 1.0f, 0, static_cast<int>(sm.states.size()) - 1);

				for (size_t stateIndex = 0; stateIndex < sm.states.size(); ++stateIndex)
				{
					auto& state = sm.states[stateIndex];
					ImGui::PushID(static_cast<int>(stateIndex) + 1000);
					if (ImGui::TreeNode((state.name.empty() ? ("State " + std::to_string(stateIndex)) : state.name).c_str()))
					{
						char stateNameBuffer[128] = {};
						std::strncpy(stateNameBuffer, state.name.c_str(), sizeof(stateNameBuffer) - 1);
						if (ImGui::InputText("State Name", stateNameBuffer, sizeof(stateNameBuffer)))
							state.name = stateNameBuffer;
						if (stateMachineEditorClips && !stateMachineEditorClips->empty())
						{
							int resolvedClipIndex = sm.ResolveClipIndex(*stateMachineEditorClips, state);
							std::string clipLabel = resolvedClipIndex >= 0
								? ((*stateMachineEditorClips)[resolvedClipIndex].name.empty()
									? ("Clip " + std::to_string(resolvedClipIndex))
									: (*stateMachineEditorClips)[resolvedClipIndex].name)
								: (state.clipName.empty() ? "<select clip>" : state.clipName + " (missing)");

							if (ImGui::BeginCombo("Clip##animsm", clipLabel.c_str()))
							{
								for (int clipIndex = 0; clipIndex < static_cast<int>(stateMachineEditorClips->size()); ++clipIndex)
								{
									std::string availableClipName = (*stateMachineEditorClips)[clipIndex].name;
									if (availableClipName.empty())
										availableClipName = "Clip " + std::to_string(clipIndex);

									bool isSelected = (resolvedClipIndex == clipIndex);
									if (ImGui::Selectable(availableClipName.c_str(), isSelected))
										state.clipName = (*stateMachineEditorClips)[clipIndex].name;
									if (isSelected)
										ImGui::SetItemDefaultFocus();
								}
								ImGui::EndCombo();
							}
						}
						else
						{
							char clipNameBuffer[128] = {};
							std::strncpy(clipNameBuffer, state.clipName.c_str(), sizeof(clipNameBuffer) - 1);
							if (ImGui::InputText("Clip Name", clipNameBuffer, sizeof(clipNameBuffer)))
								state.clipName = clipNameBuffer;
						}

						if (state.clipName.empty())
						{
							ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No clip assigned.");
						}
						else if (stateMachineEditorClips)
						{
							int validationClipIndex = sm.ResolveClipIndex(*stateMachineEditorClips, state);
							if (validationClipIndex >= 0)
								ImGui::TextDisabled("Resolved clip: %s", (*stateMachineEditorClips)[validationClipIndex].name.empty() ? "<unnamed clip>" : (*stateMachineEditorClips)[validationClipIndex].name.c_str());
							else
								ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Assigned clip is not available on the selected entity.");
						}

						InspectorGroupLabel("Playback");
						ImGui::Checkbox("Loop", &state.loop);
						ImGui::DragFloat("Playback Speed", &state.playbackSpeed, 0.01f, 0.0f, 4.0f, "%.2f");

						InspectorGroupLabel("Transitions");
						if (InspectorActionButton("Add Transition"))
							state.transitions.push_back({});

						for (size_t transitionIndex = 0; transitionIndex < state.transitions.size(); ++transitionIndex)
						{
							auto& transition = state.transitions[transitionIndex];
							ImGui::PushID(static_cast<int>(transitionIndex) + 2000);
							if (ImGui::TreeNode(("Transition " + std::to_string(transitionIndex)).c_str()))
							{
								std::string targetLabel = "<none>";
								if (sm.IsValidStateIndex(transition.targetStateIndex))
								{
									const auto& targetState = sm.states[transition.targetStateIndex];
									targetLabel = targetState.name.empty() ? ("State " + std::to_string(transition.targetStateIndex)) : targetState.name;
								}
								else if (transition.targetStateIndex >= 0)
								{
									targetLabel = "Invalid index: " + std::to_string(transition.targetStateIndex);
								}

								if (ImGui::BeginCombo("Target State", targetLabel.c_str()))
								{
									for (int targetIndex = 0; targetIndex < static_cast<int>(sm.states.size()); ++targetIndex)
									{
										std::string availableStateName = sm.states[targetIndex].name.empty()
											? ("State " + std::to_string(targetIndex))
											: sm.states[targetIndex].name;
										bool isSelected = (transition.targetStateIndex == targetIndex);
										if (ImGui::Selectable(availableStateName.c_str(), isSelected))
											transition.targetStateIndex = targetIndex;
										if (isSelected)
											ImGui::SetItemDefaultFocus();
									}
									ImGui::EndCombo();
								}

								ImGui::DragFloat("Blend Duration", &transition.blendDuration, 0.01f, 0.0f, 5.0f, "%.2f");
								ImGui::Checkbox("Requires Exit Time", &transition.requiresExitTime);
								ImGui::SliderFloat("Exit Time", &transition.exitTimeNormalized, 0.0f, 1.0f, "%.2f");
								ImGui::Checkbox("Reset Time On Enter", &transition.resetTimeOnEnter);
								if (InspectorActionButton("Add Condition"))
									transition.conditions.push_back({});

								const char* conditionOpNames[] = { "IfTrue", "IfFalse", "Greater", "Less", "Trigger" };
								for (size_t conditionIndex = 0; conditionIndex < transition.conditions.size(); ++conditionIndex)
								{
									auto& condition = transition.conditions[conditionIndex];
									ImGui::PushID(static_cast<int>(conditionIndex) + 3000);

									if (sm.parameters.empty())
									{
										char conditionParamBuffer[128] = {};
										std::strncpy(conditionParamBuffer, condition.parameterName.c_str(), sizeof(conditionParamBuffer) - 1);
										if (ImGui::InputText("Parameter", conditionParamBuffer, sizeof(conditionParamBuffer)))
											condition.parameterName = conditionParamBuffer;
									}
									else
									{
										std::string paramLabel = condition.parameterName.empty() ? "<select parameter>" : condition.parameterName;
										if (ImGui::BeginCombo("Parameter", paramLabel.c_str()))
										{
											for (const auto& parameter : sm.parameters)
											{
												bool isSelected = (condition.parameterName == parameter.name);
												if (ImGui::Selectable(parameter.name.c_str(), isSelected))
													condition.parameterName = parameter.name;
												if (isSelected)
													ImGui::SetItemDefaultFocus();
											}
											ImGui::EndCombo();
										}
									}

									int opIndex = static_cast<int>(condition.op);
									if (ImGui::Combo("Operator", &opIndex, conditionOpNames, IM_ARRAYSIZE(conditionOpNames)))
										condition.op = static_cast<MyEngine::AnimationStateMachineConditionOperator>(opIndex);
									if (condition.op == MyEngine::AnimationStateMachineConditionOperator::Greater || condition.op == MyEngine::AnimationStateMachineConditionOperator::Less)
										ImGui::DragFloat("Threshold", &condition.threshold, 0.01f);

									if (condition.parameterName.empty())
										ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Condition parameter is not set.");
									else if (sm.FindParameterIndex(condition.parameterName) < 0)
										ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Condition references missing parameter.");

									if (InspectorDangerButton("Remove Condition"))
									{
										transition.conditions.erase(transition.conditions.begin() + static_cast<std::ptrdiff_t>(conditionIndex));
										ImGui::PopID();
										break;
									}
									ImGui::Separator();
									ImGui::PopID();
								}

								if (!sm.IsValidStateIndex(transition.targetStateIndex))
									ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Transition target state is invalid.");
								if (transition.conditions.empty())
									ImGui::TextDisabled("No conditions: this transition can fire immediately once exit-time requirements pass.");

								if (InspectorDangerButton("Remove Transition"))
								{
									state.transitions.erase(state.transitions.begin() + static_cast<std::ptrdiff_t>(transitionIndex));
									ImGui::TreePop();
									ImGui::PopID();
									break;
								}
								ImGui::TreePop();
							}
							ImGui::PopID();
						}

						if (InspectorDangerButton("Remove State"))
						{
							sm.states.erase(sm.states.begin() + static_cast<std::ptrdiff_t>(stateIndex));
							if (sm.states.empty())
								sm.defaultStateIndex = -1;
							else
								sm.defaultStateIndex = std::clamp(sm.defaultStateIndex, 0, static_cast<int>(sm.states.size()) - 1);
							ImGui::TreePop();
							ImGui::PopID();
							break;
						}
						ImGui::TreePop();
					}
					ImGui::PopID();
				}
			}

			if (selectedEntity && selectedEntity->HasComponent<AnimationStateMachineComponent>())
			{
				auto& runtimeSM = selectedEntity->GetComponent<AnimationStateMachineComponent>();
				if (ImGui::CollapsingHeader("Runtime Debug##animsm", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Pause Transitions", &runtimeSM.debugPauseTransitions);
					ImGui::TextDisabled("Current State: %s", runtimeSM.debugCurrentStateName.empty() ? "<none>" : runtimeSM.debugCurrentStateName.c_str());
					ImGui::TextDisabled("Pending State: %s", runtimeSM.debugPendingStateName.empty() ? "<none>" : runtimeSM.debugPendingStateName.c_str());
					if (!runtimeSM.debugLastBlockedReason.empty())
						ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.25f, 1.0f), "Last blocked: %s", runtimeSM.debugLastBlockedReason.c_str());

					if (runtimeSM.stateMachine && !runtimeSM.stateMachine->parameters.empty())
					{
						InspectorGroupLabel("Runtime Parameters");
						if (runtimeSM.parameterValues.size() < runtimeSM.stateMachine->parameters.size())
							runtimeSM.parameterValues.resize(runtimeSM.stateMachine->parameters.size());

						for (size_t paramIndex = 0; paramIndex < runtimeSM.stateMachine->parameters.size(); ++paramIndex)
						{
							const auto& definition = runtimeSM.stateMachine->parameters[paramIndex];
							auto& runtimeValue = runtimeSM.parameterValues[paramIndex];
							ImGui::PushID(static_cast<int>(paramIndex) + 7000);
							if (definition.type == MyEngine::AnimationStateMachineParameterType::Float)
							{
								ImGui::DragFloat(definition.name.c_str(), &runtimeValue.floatValue, 0.01f);
							}
							else if (definition.type == MyEngine::AnimationStateMachineParameterType::Bool)
							{
								ImGui::Checkbox(definition.name.c_str(), &runtimeValue.boolValue);
							}
							else
							{
								ImGui::Text("%s", definition.name.c_str());
								ImGui::SameLine();
								if (ImGui::Button("Fire Trigger"))
									runtimeValue.triggerValue = true;
							}
							ImGui::PopID();
						}
					}

					if (!runtimeSM.debugTransitionMessages.empty())
					{
						InspectorGroupLabel("Transition Evaluation");
						for (const auto& msg : runtimeSM.debugTransitionMessages)
							ImGui::TextWrapped("%s", msg.c_str());
					}
				}
			}
		}
		ImGui::End();
	}
}
#endif
