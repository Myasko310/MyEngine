#include "AiAssetGeneratorPanel.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <cstring>
#include <string>

#include "ai/AiAssetGeneration.h"
#include "core/AssetPipeline.h"
#include "editor/EditorStyle.h"
#include "editor/PanelDockManager.h"

namespace MyEngine::Editor::Panels
{
	void DrawAiAssetGeneratorPanel(Context& context)
	{
		if (!context.ui)
			return;

		auto& ui = *context.ui;
		if (!ui.showAiAssetGeneratorPanel)
			return;

		PanelDockManager::Instance().SnapPanel("AI Asset Generator");
		ImGui::Begin("AI Asset Generator", &ui.showAiAssetGeneratorPanel, MyEngine::Editor::DockablePanelFlags);

		ImGui::TextWrapped("Describe a prop and generate complete local assets (geometry, textures, collision, and LODs). Generated assets are written under assets/generated/ai and queued for import.");
		ImGui::Spacing();

		static char promptBuffer[512] = "Stone temple pillar with worn edges and carved runes";
		static char outputNameBuffer[128] = "temple_pillar";
		static int lodCount = 3;
		static int textureResolution = 512;
		static bool generateCollision = true;
		static std::string statusMessage;
		static bool lastGenerationSucceeded = false;

		auto& generationService = MyEngine::AI::AiAssetGenerationService::Get();
		std::vector<std::string> providerIds = generationService.GetProviderIds();
		if (providerIds.empty())
			providerIds.push_back("none");

		int selectedProviderIndex = 0;
		for (std::size_t i = 0; i < providerIds.size(); ++i)
		{
			if (providerIds[i] == generationService.GetActiveProviderId())
			{
				selectedProviderIndex = static_cast<int>(i);
				break;
			}
		}

		std::string providerList;
		for (const auto& provider : providerIds)
		{
			providerList += provider;
			providerList.push_back('\0');
		}
		providerList.push_back('\0');

		InspectorGroupLabel("Generator Settings");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("Provider", &selectedProviderIndex, providerList.c_str()))
			generationService.SetActiveProvider(providerIds[static_cast<std::size_t>(selectedProviderIndex)]);

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##aiPrompt", "Describe the object to generate", promptBuffer, sizeof(promptBuffer));
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##aiOutputName", "Output asset name (optional)", outputNameBuffer, sizeof(outputNameBuffer));
		ImGui::SliderInt("LOD Count", &lodCount, 1, 4);
		ImGui::SliderInt("Texture Resolution", &textureResolution, 128, 2048, "%d", ImGuiSliderFlags_Logarithmic);
		ImGui::Checkbox("Generate Collision Mesh", &generateCollision);

		if (InspectorActionButton("Generate AI Asset"))
		{
			MyEngine::AI::AiAssetGenerationRequest request;
			request.prompt = promptBuffer;
			request.outputName = outputNameBuffer;
			request.outputRootDirectory = "assets/generated/ai";
			request.options.generateCollisionMesh = generateCollision;
			request.options.lodCount = lodCount;
			request.options.textureResolution = textureResolution;

			MyEngine::AI::AiAssetGenerationResult result = generationService.Generate(request);
			lastGenerationSucceeded = result.success;
			statusMessage = result.message;

			if (result.success)
			{
				auto& pipeline = MyEngine::AssetPipeline::Get();
				for (const auto& filePath : result.generatedFiles)
				{
					if (filePath.empty())
						continue;
					pipeline.QueueImport(filePath);
				}
				statusMessage = "Generated and queued imports from: " + result.assetDirectory;
			}
		}

		ImGui::Spacing();
		if (!statusMessage.empty())
		{
			ImVec4 color = lastGenerationSucceeded ? ImVec4(0.30f, 1.00f, 0.50f, 1.0f) : ImVec4(1.00f, 0.45f, 0.35f, 1.0f);
			ImGui::TextColored(color, "%s", statusMessage.c_str());
		}

		ImGui::TextDisabled("Tip: after generation, open Asset Browser to inspect imported models/materials.");
		ImGui::End();
	}
}
#endif
