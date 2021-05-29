//
// Created by jglrxavpok on 27/05/2021.
//

#include "EditorSettings.h"
#include <iostream>
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>
#include <filesystem>
#include <cstdlib>
#include <engine/io/IO.h>
#include <engine/utils/Containers.h>
#include <engine/utils/JSON.h>

void Tools::EditorSettings::load() {
    recentProjects.clear();

    if(!std::filesystem::exists(settingsFile)) {
        return;
    }

    rapidjson::Document description;
    description.Parse(IO::readFileAsText(settingsFile.string()).c_str());

    auto projects = description["recent_projects"].GetArray();
    for(const auto& project : projects) {
        recentProjects.emplace_back(project.GetString());
    }
}

void Tools::EditorSettings::save() {
    FILE* fp = fopen(settingsFile.string().c_str(), "wb"); // non-Windows use "w"

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);

    rapidjson::Document document;
    document.SetObject();

    rapidjson::Value recentProjectArray;
    recentProjectArray.SetArray();

    uint64_t recentProjectCount = 0;
    for(const auto& project : recentProjects) {
        if(recentProjectCount >= MaxRecentFiles) {
            break;
        }

        recentProjectArray.PushBack(Carrot::JSON::makeRef(project.string()), document.GetAllocator());
        recentProjectCount++;
    }

    document.AddMember("recent_projects", recentProjectArray, document.GetAllocator());

    document.Accept(writer);
    fclose(fp);
    Carrot::JSON::clearReferences();
}

void Tools::EditorSettings::addToRecentProjects(std::filesystem::path toAdd) {
    Carrot::removeIf(recentProjects, [&](const auto& a) { return a == toAdd; });
    recentProjects.push_front(toAdd);

    save();
}
