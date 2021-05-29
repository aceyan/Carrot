//
// Created by jglrxavpok on 28/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui_node_editor.h"
#include "imgui.h"
#include "EditorGraph.h"
#include "EditorSettings.h"
#include "ProjectMenuHolder.h"

namespace Tools {
    class TemplateEditor: public ProjectMenuHolder {
    public:
        TemplateEditor(Carrot::Engine& engine);

    public:
        void tick(double deltaTime);
        void onFrame(size_t frameIndex) override;
        void open();

        void performLoad(filesystem::path path) override;

        void saveToFile(std::filesystem::path path) override;

        bool showUnsavedChangesPopup() override;

    public:
        EditorGraph& getGraph() { return graph; };

    private:
        bool isOpen = false;
        bool hasUnsavedChanges = false;

        std::string title;
        char titleImGuiBuffer[128];
        Carrot::Engine& engine;
        EditorSettings settings;
        EditorGraph graph;
    };
}
