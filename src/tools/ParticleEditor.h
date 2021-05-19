//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui_node_editor.h"
#include "imgui.h"
#include "EditorGraph.h"

namespace Tools {
    class ParticleEditor {
    private:
        Carrot::Engine& engine;
        EditorGraph updateGraph;
        EditorGraph renderGraph;

        void updateUpdateGraph(size_t frameIndex);
        void updateRenderGraph(size_t frameIndex);

        void addCommonNodes(Tools::EditorGraph& graph);

    public:
        explicit ParticleEditor(Carrot::Engine& engine);

        void onFrame(size_t frameIndex);
        void tick(double deltaTime);

        ~ParticleEditor();
    };
}