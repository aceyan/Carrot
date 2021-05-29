//
// Created by jglrxavpok on 26/05/2021.
//

#pragma once
#include "../EditorNode.h"
#include "engine/io/IO.h"
#include "engine/utils/JSON.h"
#include <filesystem>

namespace Tools {
    class TemplateParseError: public std::exception {
    public:
        explicit TemplateParseError(const std::string& message, const std::string& templateName, const char* file, const char* function, int line, std::string condition) {
            fullMessage = "Error in template '" + templateName + "': " + file + ", in " + function + " (line " + std::to_string(line) + ")\n"
                          "Condition is: " + condition;
        }

        const char *what() const override {
            return fullMessage.c_str();
        }

    private:
        std::string fullMessage;
    };

    /// User-defined black box
    class TemplateNode: public EditorNode {
#define THROW_IF(condition, message) if(condition) throw TemplateParseError(message, templateName, __FILE__, __FUNCTION__, __LINE__, #condition);
    public:
        explicit TemplateNode(Tools::EditorGraph& graph, const std::string& name): EditorNode(graph, name, "template"), templateName(name) {
            load();
        }

        explicit TemplateNode(Tools::EditorGraph& graph, const rapidjson::Value& json): EditorNode(graph, "tmp-template", "template", json) {
            THROW_IF(!json.HasMember("extra"), "Missing 'extra' member inside json object!");
            templateName = json["extra"]["template_name"].GetString();
            load();
        }

    public:
        rapidjson::Value serialiseToJSON(rapidjson::Document& doc) const override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                                     .AddMember("template_name", Carrot::JSON::makeRef(templateName), doc.GetAllocator())
            );
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex) const override {
            throw "TODO";
        }

    private:
        inline static std::array<std::string, 2> Paths = {
                "resources/node_templates/", // default builtin templates
                "user/node_templates/", // user defined templates (TODO: move elsewhere?)
        };

        void load() {
            rapidjson::Document description;
            for(const auto& pathPrefix : Paths) {
                if(std::filesystem::exists(pathPrefix)) {
                    description.Parse(IO::readFileAsText(pathPrefix + templateName + ".json").c_str());
                    break;
                }
            }

            THROW_IF(!description.HasMember("title"), "Missing 'title' member");
            THROW_IF(!description.HasMember("inputs"), "Missing 'inputs' member");
            THROW_IF(!description.HasMember("outputs"), "Missing 'outputs' member");
            title = description["title"].GetString();

            auto inputArray = description["inputs"].GetArray();
            for(const auto& input : inputArray) {
                THROW_IF(!input.HasMember("name"), "Missing 'name' member inside input");
                THROW_IF(!input.HasMember("type"), "Missing 'type' member inside input");
                auto type = Carrot::ExpressionTypes::fromName(input["type"].GetString());
                newInput(input["name"].GetString(), type);
            }

            auto outputArray = description["outputs"].GetArray();
            for(const auto& output : outputArray) {
                THROW_IF(!output.HasMember("name"), "Missing 'name' member inside output");
                THROW_IF(!output.HasMember("type"), "Missing 'type' member inside output");
                auto type = Carrot::ExpressionTypes::fromName(output["type"].GetString());
                newOutput(output["name"].GetString(), type);
            }
        }

    private:
        std::string templateName;
    };
}
