//
// Created by jglrxavpok on 24/11/2021.
//

#include <cstdint>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include "FileIncluder.h"
#include <core/data/ShaderMetadata.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filewritestream.h>
#include <core/utils/stringmanip.h>

// imports from glslang
#include <SPIRV/Logger.h>
#include <SPIRV/SpvTools.h>
#include <SPIRV/GlslangToSpv.h>
#include "glslang/Public/ShaderLang.h"
#include "glslang/Public/ResourceLimits.h"

static std::filesystem::path outputList = "shadercompilerlist.txt";

void showUsage() {
    std::cerr <<
        "shadercompiler [base path] [input file] [output file] [stage]" << '\n'
        << "\tCompiles a shader and write additional metadata next to the output." << '\n'
        << "\t\t- [base path]: Path to <source folder>/resources/shaders" << '\n'
        << "\t\t- [input file]: Path of file inside <source folder>/resources/shaders to compile" << '\n'
        << "\t\t- [output file]: Path of file inside <build folder>/resources/shaders to compile" << '\n'
        << "\t\t- [stage]: Shader type to add" << '\n'
        << '\n'
        << std::endl;
}

int main(int argc, const char** argv) {
    if(argc < 5) {
        std::cerr << "Missing arguments" << std::endl;
        showUsage();
        return -1;
    }

    const char* basePath = argv[1];
    const char* filename = argv[2];
    const char* outFilename = argv[3];
    const char* stageStr = argv[4];

    EShLanguage stage = EShLangFragment;
    if(strcmp(stageStr, "fragment") == 0) {
        stage = EShLangFragment;
    } else if(strcmp(stageStr, "vertex") == 0) {
        stage = EShLangVertex;
    } else if(strcmp(stageStr, "rgen") == 0) {
        stage = EShLangRayGen;
    } else if(strcmp(stageStr, "rchit") == 0) {
        stage = EShLangClosestHit;
    } else if(strcmp(stageStr, "compute") == 0) {
        stage = EShLangCompute;
    } else if(strcmp(stageStr, "rmiss") == 0) {
        stage = EShLangMiss;
    } else if(strcmp(stageStr, "task") == 0) {
        stage = EShLangTask;
    } else if(strcmp(stageStr, "mesh") == 0) {
        stage = EShLangMesh;
    } else {
        std::cerr << "Invalid stage: " << stageStr << std::endl;
        return -1;
    }

    if(!glslang::InitializeProcess()) {
        std::cerr << "Failed to setup glslang." << std::endl;
        return -2;
    }

    std::filesystem::path inputFile = filename;
    std::filesystem::path outputPath = outFilename;

    if(!std::filesystem::exists(inputFile)) {
        std::cerr << "File does not exist: " << inputFile.string().c_str() << std::endl;
        return -3;
    }

    glslang::TShader shader(stage);

    shader.setEntryPoint("main");
    shader.setSourceEntryPoint("main");
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

    std::ifstream file(inputFile, std::ios::in);

    std::string filecontents;

    std::string line;
    while (getline(file, line)) {
        if (!filecontents.empty()) {
            filecontents += '\n';
        }
        filecontents += line;
    }

    std::string preamble = R"(
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_control_flow_attributes: enable
#extension GL_EXT_samplerless_texture_functions: enable
#extension GL_ARB_shader_draw_parameters: enable
)";
    auto filepath = inputFile.string();
    std::array strs {
        filecontents.c_str(),
    };
    std::array names {
            filepath.c_str(),
    };
    shader.setPreamble(preamble.c_str());
    shader.setStringsWithLengthsAndNames(strs.data(), nullptr, names.data(), strs.size());

    ShaderCompiler::FileIncluder includer { basePath };
    TBuiltInResource Resources = *GetDefaultResources();
    if(!shader.parse(&Resources, 460, false, EShMsgDefault, includer)) {
        std::cerr << "Failed shader compilation. " << shader.getInfoLog() << std::endl;
        return -4;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if(!program.link(EShMsgDefault)) {
        std::cerr << "Failed shader linking. " << program.getInfoLog() << std::endl;
        return -5;
    }

    auto& shaders = program.getShaders(stage);
    if(shaders.empty()) {
        std::cerr << "No program of type " << stageStr << " has been linked. This should NOT happen!!" << std::endl;
        return -6;
    }

    if(!program.mapIO()) {
        std::cerr << "Failed shader linking (glslang mapIO). " << program.getInfoLog() << std::endl;
        return -7;
    }

    std::vector<std::uint32_t> spirv;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions;

    // TODO: argument
    spvOptions.generateDebugInfo = true;
    spvOptions.stripDebugInfo = false;

    spvOptions.disableOptimizer = true;
    spvOptions.optimizeSize = false;
    spvOptions.disassemble = false;
    spvOptions.validate = true;
    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv, &logger, &spvOptions);

    {
        std::ofstream outputFile(outputPath, std::ios::binary);
        outputFile.write(reinterpret_cast<const char *>(spirv.data()), spirv.size() * sizeof(std::uint32_t));
    }

    // runtime metadata file (for hot reload)
    {
        ShaderCompiler::Metadata metadata;
        for (const auto& includedFile: includer.includedFiles) {
            metadata.sourceFiles.push_back(std::filesystem::absolute(includedFile));
        }
        metadata.sourceFiles.push_back(std::filesystem::absolute(inputFile));

        metadata.commandArguments[0] = basePath;
        metadata.commandArguments[1] = filename;
        metadata.commandArguments[2] = outFilename;
        metadata.commandArguments[3] = stageStr;

        auto metadataPath = outputPath;
        metadataPath.replace_extension(".meta.json");
        FILE *fp = fopen(Carrot::toString(metadataPath.u8string()).c_str(), "wb"); // non-Windows use "w"

        char writeBuffer[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

        rapidjson::Document document;
        document.SetObject();

        metadata.writeJSON(document);

        document.Accept(writer);
        fclose(fp);
    }

    // depfile (for CMake)
    {
        auto depfilePath = outputPath;
        depfilePath.replace_extension(".spv.d");
        std::wofstream outputFile(depfilePath);

        std::filesystem::path relativeOutput = std::filesystem::relative(outputPath, std::filesystem::current_path());
        outputFile << outputPath.c_str() << ": ";
        for(const auto& includedFile : includer.includedFiles) {
            std::wstring path = includedFile.c_str();
            // replace separators
            for(std::size_t i = 0; i < path.size(); i++) {
                if(path[i] == L'\\') {
                    path[i] = L'/';
                }
            }
            outputFile << path << " ";
        }
    }

    return 0;
}