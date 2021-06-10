//
// Created by jglrxavpok on 19/05/2021.
//

#include "ParticleShaderGenerator.h"
#include "engine/expressions/ExpressionVisitor.h"
#include <stack>
#include <utility>
#include "tools/nodes/VariableNodes.h"
#include "tools/nodes/TerminalNodes.h"
#include "SPIRV/GLSL.std.450.h"

namespace Tools {
    using namespace spv;

    struct Variables {
        Id glslImport;

        Id position;
        Id life;
        Id velocity;
        Id size;
        Id particleIndex;
        Id emissionID;

        union {
            struct {
                Id deltaTime;
            } compute;

            struct {
                Id particleColor;
                Id fragmentPosition;
            } fragment;
        };

        ParticleShaderMode shaderMode;
    };

    class SPIRVisitor: public Carrot::ExpressionVisitor {
    public:
        inline explicit SPIRVisitor(spv::Builder& builder, Variables vars): builder(builder), vars(vars) {}

    public:
        void visitConstant(Carrot::ConstantExpression& expression) override {
            auto value = expression.getValue();
            auto exprType = expression.getType();
            types.push(exprType);

            if(exprType == Carrot::ExpressionTypes::Float) {
                ids.push(builder.makeFloatConstant(value.asFloat));
            } else if(exprType == Carrot::ExpressionTypes::Int) {
                ids.push(builder.makeIntConstant(value.asInt));
            } else if(exprType == Carrot::ExpressionTypes::Bool) {
                ids.push(builder.makeBoolConstant(value.asBool));
            } else {
                throw std::runtime_error("Invalid type");
            }
        }

        void visitGetVariable(Carrot::GetVariableExpression& expression) override {
            const auto& var = expression.getVariableName();

            if(var == VariableNode::getInternalName(VariableNodeType::GetSize)) {
                ids.push(builder.createLoad(vars.size, spv::NoPrecision));
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetVelocity)) {
                auto access = builder.createAccessChain(spv::StorageClassStorageBuffer, vars.velocity, {builder.makeIntConstant(expression.getSubIndex())});
                ids.push(builder.createLoad(access, spv::NoPrecision));
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetLife)) {
                ids.push(vars.life);
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetParticleIndex)) {
                ids.push(vars.particleIndex);
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetEmissionID)) {
                ids.push(builder.createLoad(vars.emissionID, spv::NoPrecision));
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetPosition)) {
                auto access = builder.createAccessChain(spv::StorageClassStorageBuffer, vars.position,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                ids.push(builder.createLoad(access, spv::NoPrecision));
                types.push(Carrot::ExpressionTypes::Float);
            }
        // === COMPUTE ===
            else if(var == VariableNode::getInternalName(VariableNodeType::GetDeltaTime)) {
                assert(vars.shaderMode == ParticleShaderMode::Compute);
                ids.push(vars.compute.deltaTime);
                types.push(Carrot::ExpressionTypes::Float);
            }
        // === FRAGMENT ===
            else if(var == VariableNode::getInternalName(VariableNodeType::GetFragmentPosition)) {
                assert(vars.shaderMode == ParticleShaderMode::Fragment);
                auto access = builder.createAccessChain(spv::StorageClassFunction, vars.fragment.fragmentPosition,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                ids.push(builder.createLoad(access, spv::NoPrecision));
                types.push(Carrot::ExpressionTypes::Float);
            }
        // ================
            else {
                throw std::runtime_error("Unsupported variable type: "+var);
            }
        }

        void visitSetVariable(Carrot::SetVariableExpression& expression) override {
            visit(expression.getValue());

            const auto& var = expression.getVariableName();

            auto typeToStore = popType();

        // === COMPUTE ===
            if(var == TerminalNode::getInternalName(TerminalNodeType::SetSize)) {
                throwInvalidType(Carrot::ExpressionTypes::Float, typeToStore, "set size");
                builder.createStore(popID(), vars.size);
            } else if(var == TerminalNode::getInternalName(TerminalNodeType::SetVelocity)) {
                throwInvalidType(Carrot::ExpressionTypes::Float, typeToStore, "set velocity component");
                auto access = builder.createAccessChain(spv::StorageClassStorageBuffer, vars.velocity,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                builder.createStore(popID(), access);
            }
        // === FRAGMENT ===
            else if(var == TerminalNode::getInternalName(TerminalNodeType::SetOutputColor)) {
                throwInvalidType(Carrot::ExpressionTypes::Float, typeToStore, "set output color component");
                assert(vars.shaderMode == ParticleShaderMode::Fragment);
                auto access = builder.createAccessChain(spv::StorageClassFunction, vars.fragment.particleColor,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                builder.createStore(popID(), access);
            }
            else if(var == TerminalNode::getInternalName(TerminalNodeType::DiscardPixel)) {
                throwInvalidType(Carrot::ExpressionTypes::Bool, typeToStore, "discard pixel");
                assert(vars.shaderMode == ParticleShaderMode::Fragment);
                auto shouldDiscard = popID();

                Block& thenBlock = builder.makeNewBlock();
                Block& mergeBlock = builder.makeNewBlock();
                { // selection merge
                    Instruction* merge = new Instruction(OpSelectionMerge);
                    merge->addIdOperand(mergeBlock.getId());
                    merge->addImmediateOperand(spv::SelectionControlMaskNone);
                    builder.getBuildPoint()->addInstruction(std::unique_ptr<Instruction>(merge));
                }

                builder.createConditionalBranch(shouldDiscard, &thenBlock, &mergeBlock);

                builder.setBuildPoint(&thenBlock);
                builder.createNoResultOp(spv::OpKill);

                builder.setBuildPoint(&mergeBlock);
            }
        // ================
            else {
                throw std::runtime_error("Unsupported variable type: "+var);
            }
        }

        void visitAdd(Carrot::AddExpression& expression) override {
            visitBinOp(spv::OpFAdd, expression.getOperand1(), expression.getOperand2());
        }

        void visitSub(Carrot::SubExpression& expression) override {
            visitBinOp(spv::OpFSub, expression.getOperand1(), expression.getOperand2());
        }

        void visitMult(Carrot::MultExpression& expression) override {
            visitBinOp(spv::OpFMul, expression.getOperand1(), expression.getOperand2());
        }

        void visitDiv(Carrot::DivExpression& expression) override {
            visitBinOp(spv::OpFDiv, expression.getOperand1(), expression.getOperand2());
        }

        void visitCompound(Carrot::CompoundExpression& expression) override {
            for(const auto& e : expression.getSubExpressions()) {
                if(e != nullptr)
                    visit(e);
            }
        }

        void visitMod(Carrot::ModExpression& expression) override {
            visitBinOp(spv::OpFMod, expression.getOperand1(), expression.getOperand2());
        }

        void visitLess(Carrot::LessExpression& expression) override {
            visitCompareOp(spv::OpFOrdLessThan, expression.getOperand1(), expression.getOperand2());
        }

        void visitLessOrEquals(Carrot::LessOrEqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdLessThanEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitGreater(Carrot::GreaterExpression& expression) override {
            visitCompareOp(spv::OpFOrdGreaterThan, expression.getOperand1(), expression.getOperand2());
        }

        void visitGreaterOrEquals(Carrot::GreaterOrEqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdGreaterThanEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitEquals(Carrot::EqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitNotEquals(Carrot::NotEqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdNotEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitOr(Carrot::OrExpression& expression) override {
            visitBinOp(spv::OpLogicalOr, expression.getOperand1(), expression.getOperand2());
        }

        void visitAnd(Carrot::AndExpression& expression) override {
            visitBinOp(spv::OpLogicalAnd, expression.getOperand1(), expression.getOperand2());
        }

        void visitXor(Carrot::XorExpression& expression) override {
            // SPIR-V has no logical XOR operation, so we have to implement it ourselves

            visitBinOp(spv::OpAtomicOr, expression.getOperand1(), expression.getOperand2());
            auto orResult = popID();
            auto orResultType = popType();
            assert(orResultType == Carrot::ExpressionTypes::Bool);

            visitBinOp(spv::OpAtomicAnd, expression.getOperand1(), expression.getOperand2());
            auto andResult = popID();
            auto andResultType = popType();
            assert(andResultType == Carrot::ExpressionTypes::Bool);

            auto negatedAnd = builder.createUnaryOp(spv::OpLogicalNot, builder.makeBoolType(), andResult);
            auto xorResult = builder.createBinOp(spv::OpLogicalAnd, builder.makeBoolType(), negatedAnd, orResult);
            ids.push(xorResult);
            types.push(Carrot::ExpressionTypes::Bool);
        }

        void visitBoolNegate(Carrot::BoolNegateExpression& expression) override {
            auto prevResult = popID();
            auto prevResultType = popType();
            assert(prevResultType == Carrot::ExpressionTypes::Bool);
            auto negated = builder.createUnaryOp(spv::OpLogicalNot, builder.makeBoolType(), prevResult);
            ids.push(negated);
            types.push(Carrot::ExpressionTypes::Bool);
        }

        void visitSin(Carrot::SinExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Sin);
        }

        void visitCos(Carrot::CosExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Cos);
        }

        void visitTan(Carrot::TanExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Tan);
        }

        void visitExp(Carrot::ExpExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Exp);
        }

        void visitAbs(Carrot::AbsExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450FAbs);
        }

        void visitSqrt(Carrot::SqrtExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Sqrt);
        }

        void visitLog(Carrot::LogExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Log);
        }

        void visitPlaceholder(Carrot::PlaceholderExpression& expression) override {
            throw std::runtime_error("Cannot accept placeholder expression!");
        }

        void visitPrefixed(Carrot::PrefixedExpression& expression) override {
            visit(expression.getPrefix());
            visit(expression.getExpression());
        }

        void visitOnce(Carrot::OnceExpression& expression) override {
            if(alreadyVisited.contains(expression.getUUID())) {
                return;
            }
            alreadyVisited.insert(expression.getUUID());
            visit(expression.getExpressionToExecute());
        }

    private:
        spv::Builder& builder;
        std::stack<Id> ids;
        std::stack<Carrot::ExpressionType> types;
        Variables vars;
        std::unordered_set<Carrot::UUID> alreadyVisited;

        void visitGLSLFunction(GLSLstd450 function) {
            auto operand = popID();
            auto operandType = popType();
            assert(operandType == Carrot::ExpressionTypes::Float);
            auto result = builder.createTriOp(spv::OpExtInst, builder.makeFloatType(32), vars.glslImport, function, operand);
            ids.push(result);
            types.push(Carrot::ExpressionTypes::Float);
        }

        void visitBinOp(spv::Op op, std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) {
            visit(left);
            visit(right);
            auto rightID = popID();
            auto leftID = popID();

            auto leftType = popType();
            auto rightType = popType();
            if(leftType != rightType) {
                throw std::runtime_error("Cannot operate on operands of different types");
            }

            const auto& returnType = leftType;

            ids.push(builder.createBinOp(op, asSpirType(returnType), leftID, rightID));
            types.push(returnType);
        }

        void visitCompareOp(spv::Op op, std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) {
            visit(left);
            visit(right);
            auto rightID = popID();
            auto leftID = popID();

            auto leftType = popType();
            auto rightType = popType();
            if(leftType != rightType) {
                throw std::runtime_error("Cannot operate on operands of different types");
            }

            auto returnType = Carrot::ExpressionTypes::Bool;

            ids.push(builder.createBinOp(op, asSpirType(returnType), leftID, rightID));
            types.push(returnType);
        }

        static void throwInvalidType(const Carrot::ExpressionType& expected, const Carrot::ExpressionType& actual, const std::string& where) {
            if(expected != actual) {
                throw std::runtime_error("Invalid type, expected " + expected.name() + ", got " + actual.name() + ", at " + where);
            }
        }

        [[nodiscard]] Id asSpirType(const Carrot::ExpressionType& type) const {
            if(type == Carrot::ExpressionTypes::Void) {
                return builder.makeVoidType();
            }
            if(type == Carrot::ExpressionTypes::Float) {
                return builder.makeFloatType(32);
            }
            if(type == Carrot::ExpressionTypes::Int) {
                return builder.makeIntConstant(32);
            }
            if(type == Carrot::ExpressionTypes::Bool) {
                return builder.makeBoolType();
            }
            throw std::runtime_error("Unsupported type");
        }

        Id popID() {
            auto id = ids.top();
            ids.pop();
            return id;
        }

        Carrot::ExpressionType popType() {
            auto type = types.top();
            types.pop();
            return type;
        }
    };

    ParticleShaderGenerator::ParticleShaderGenerator(ParticleShaderMode shaderMode, const std::string& projectName): shaderMode(shaderMode), projectName(projectName) {

    }

    std::vector<uint32_t> ParticleShaderGenerator::compileToSPIRV(const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        SpvBuildLogger logger;
        Builder builder(spv::SpvVersion::Spv_1_5, 0, &logger);

        builder.setSource(spv::SourceLanguageGLSL, 450);
        builder.setSourceFile(projectName);
        builder.addCapability(spv::CapabilityShader);
        auto glslImport = builder.import("GLSL.std.450");
        builder.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec3Type = builder.makeVectorType(float32Type, 3);
        /*
         layout(constant_id = 0) const uint MAX_PARTICLE_COUNT = 1000;

        struct Particle {
            vec3 position;
            float life;
            vec3 velocity;
            float size;

            uint id;
        };

        layout(set = 0, binding = 1) buffer Particles {
            Particle particles[MAX_PARTICLE_COUNT];
        };
         */
        auto particleType = builder.makeStructType(std::vector<Id>{
                vec3Type,
                float32Type,
                vec3Type,
                float32Type,

                uint32Type
        }, "Particle");
        builder.addMemberName(particleType, 0, "position");
        builder.addMemberName(particleType, 1, "life");
        builder.addMemberName(particleType, 2, "velocity");
        builder.addMemberName(particleType, 3, "size");
        builder.addMemberName(particleType, 4, "id");

        builder.addMemberDecoration(particleType, 0, spv::DecorationOffset, 0);
        builder.addMemberDecoration(particleType, 1, spv::DecorationOffset, 12);
        builder.addMemberDecoration(particleType, 2, spv::DecorationOffset, 16);
        builder.addMemberDecoration(particleType, 3, spv::DecorationOffset, 28);
        builder.addMemberDecoration(particleType, 4, spv::DecorationOffset, 32);

        auto maxParticleCount = builder.makeIntConstant(1000, true);
        builder.addName(maxParticleCount, "MAX_PARTICLE_COUNT");
        builder.addDecoration(maxParticleCount, spv::DecorationSpecId, 0);

        auto particleArrayType = builder.makeArrayType(particleType, maxParticleCount, 0);
        builder.addDecoration(particleArrayType, spv::DecorationArrayStride, 48);
        auto storageBufferParticles = builder.makeStructType(std::vector<Id>{particleArrayType}, "Particles");
        builder.addMemberName(storageBufferParticles, 0, "particles");
        builder.addDecoration(storageBufferParticles, spv::DecorationBlock);
        builder.addMemberDecoration(storageBufferParticles, 0, spv::DecorationOffset, 0);

        auto descriptorSet = builder.createVariable(spv::NoPrecision, spv::StorageClassStorageBuffer, storageBufferParticles);
        builder.addName(descriptorSet, "DescriptorSet0");
        builder.addDecoration(descriptorSet, spv::DecorationDescriptorSet, 0);
        builder.addDecoration(descriptorSet, spv::DecorationBinding, 1);

        switch(shaderMode) {
            case ParticleShaderMode::Compute:
                generateCompute(builder, glslImport, descriptorSet, expressions);
                break;

            case ParticleShaderMode::Fragment:
                generateFragment(builder, glslImport, descriptorSet, expressions);
                break;
        }

        std::vector<uint32_t> output;
        builder.dump(output);

        std::cout << "SpvBuilder output:\n";
        std::cerr << logger.getAllMessages();
        std::cout << "\n=== END of SpvBuilder output ===";

        return std::move(output);
    }

    void ParticleShaderGenerator::generateFragment(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        spv::Block* functionBlock;
        Function* mainFunction = builder.makeFunctionEntry(spv::NoPrecision, builder.makeVoidType(), "main", {}, {}, &functionBlock);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec2Type = builder.makeVectorType(float32Type, 2);
        auto vec3Type = builder.makeVectorType(float32Type, 3);
        auto vec4Type = builder.makeVectorType(float32Type, 4);

        /*
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outViewPosition;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out uint outIntProperties;

layout(location = 0) in flat uint particleIndex;
         */
        auto outColor = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, vec4Type);
        builder.addName(outColor, "outColor");
        builder.addDecoration(outColor, spv::DecorationLocation, 0);

        auto outViewPosition = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, vec3Type);
        builder.addName(outViewPosition, "outViewPosition");
        builder.addDecoration(outViewPosition, spv::DecorationLocation, 1);

        auto outNormal = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, vec3Type);
        builder.addName(outNormal, "outNormal");
        builder.addDecoration(outNormal, spv::DecorationLocation, 2);

        auto outIntProperties = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, uint32Type);
        builder.addName(outIntProperties, "outIntProperties");
        builder.addDecoration(outIntProperties, spv::DecorationLocation, 3);

        auto inParticleIndex = builder.createVariable(spv::NoPrecision, spv::StorageClassInput, uint32Type);
        builder.addName(inParticleIndex, "inParticleIndex");
        builder.addDecoration(inParticleIndex, spv::DecorationLocation, 0);
        builder.addDecoration(inParticleIndex, spv::DecorationFlat);

        auto inFragPosition = builder.createVariable(spv::NoPrecision, spv::StorageClassInput, vec2Type);
        builder.addName(inFragPosition, "inFragPosition");
        builder.addDecoration(inFragPosition, spv::DecorationLocation, 1);

        auto varParticleIndex = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto tmpParticleIndex = builder.createLoad(inParticleIndex, spv::NoPrecision);
        builder.createStore(tmpParticleIndex, varParticleIndex);
        builder.addName(varParticleIndex, "particleIndex");

        auto varFragmentPosition = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, vec2Type);
        auto tmpFragmentPosition = builder.createLoad(inFragPosition, spv::NoPrecision);
        builder.createStore(tmpFragmentPosition, varFragmentPosition);
        builder.addName(varFragmentPosition, "fragmentPosition");

        auto particleIndex = builder.createLoad(varParticleIndex, spv::NoPrecision);
        auto velocityAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(2)});
        auto positionAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(0)});
        auto lifeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(1)});
        auto sizeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(3)});

        auto varEmissionID = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto emissionIDAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(4)});
        builder.createStore(builder.createLoad(emissionIDAccess, spv::NoPrecision), varEmissionID);
        auto emissionID = builder.createLoad(varEmissionID, spv::NoPrecision);

        auto castParticleID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, particleIndex);

        auto varFinalColor = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, vec4Type);
        auto float1 = builder.makeFloatConstant(1.0f);
        builder.createStore(builder.createCompositeConstruct(vec4Type, {float1, float1, float1, float1}), varFinalColor);

        // user-specific code
        Variables vars{};
        vars.glslImport = glslImport;
        vars.shaderMode = shaderMode;
        vars.fragment.particleColor = varFinalColor;
        vars.fragment.fragmentPosition = varFragmentPosition;
        vars.emissionID = emissionID;
        vars.position = positionAccess;
        vars.life = builder.createLoad(lifeAccess, spv::NoPrecision);
        vars.velocity = velocityAccess;
        vars.size = sizeAccess;
        vars.particleIndex = castParticleID;
        for(auto& expr : expressions) {
            builder.getStringId(expr->toString());

            SPIRVisitor exprBuilder(builder, vars);
            exprBuilder.visit(expr);
        }

        builder.createStore(builder.createLoad(varFinalColor, spv::NoPrecision), outColor);
        builder.createStore(builder.makeUintConstant(0), outIntProperties);

        builder.leaveFunction();
        auto entryPoint = builder.addEntryPoint(spv::ExecutionModelFragment, mainFunction, "main");
        entryPoint->addIdOperand(inParticleIndex);
        entryPoint->addIdOperand(inFragPosition);
        entryPoint->addIdOperand(outColor);
        entryPoint->addIdOperand(outViewPosition);
        entryPoint->addIdOperand(outNormal);
        entryPoint->addIdOperand(outIntProperties);
        entryPoint->addIdOperand(descriptorSet);

        builder.addExecutionMode(mainFunction, spv::ExecutionModeOriginUpperLeft);
    }

    void ParticleShaderGenerator::generateCompute(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        spv::Block* functionBlock;
        Function* mainFunction = builder.makeFunctionEntry(spv::NoPrecision, builder.makeVoidType(), "main", {}, {}, &functionBlock);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec3Type = builder.makeVectorType(float32Type, 3);

        // Carrot::ParticleStatistics
        auto particleStatsType = builder.makeStructType(std::vector<Id>{builder.makeFloatType(32), builder.makeUintType(32)}, "ParticleStatistics");
        builder.addMemberName(particleStatsType, 0, "deltaTime");
        builder.addMemberName(particleStatsType, 1, "particleCount");

        builder.addDecoration(particleStatsType, spv::DecorationBlock);

        builder.addMemberDecoration(particleStatsType, 0, spv::DecorationOffset, 0);
        builder.addMemberDecoration(particleStatsType, 1, spv::DecorationOffset, 4);

        auto particleStats = builder.createVariable(spv::NoPrecision, spv::StorageClassStorageBuffer, particleStatsType);
        builder.addDecoration(particleStats, spv::DecorationDescriptorSet, 0);
        builder.addDecoration(particleStats, spv::DecorationBinding, 0);
        builder.addName(particleStats, "particleStats");

        auto globalInvocationId = builder.createVariable(spv::NoPrecision,
                                                         spv::StorageClassInput,
                                                         builder.makeVectorType(builder.makeUintType(32), 3),
                                                         "gl_GlobalInvocationID");
        builder.addDecoration(globalInvocationId, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);


        auto gl_WorkGroupSize = builder.makeCompositeConstant(builder.makeVectorType(builder.makeUintType(32), 3),
                                                              std::vector{
                                                                      builder.makeIntConstant(1024),
                                                                      builder.makeIntConstant(1),
                                                                      builder.makeIntConstant(1)
                                                              });
        builder.addDecoration(gl_WorkGroupSize, spv::DecorationBuiltIn, spv::BuiltIn::BuiltInWorkgroupSize);

        auto dtAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, particleStats, std::vector<Id>{builder.makeIntConstant(0)});

        auto varParticleIndex = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto globalInvocationAccess = builder.createAccessChain(spv::StorageClassInput, globalInvocationId, std::vector<Id>{builder.makeUintConstant(0)});
        auto tmpParticleIndex = builder.createLoad(globalInvocationAccess, spv::NoPrecision);
        builder.createStore(tmpParticleIndex, varParticleIndex);

        auto particleIndex = builder.createLoad(varParticleIndex, spv::NoPrecision);

        auto varEmissionID = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto emissionIDAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(4)});
        builder.createStore(emissionIDAccess, varEmissionID);
        auto emissionID = builder.createLoad(varEmissionID, spv::NoPrecision);

        auto velocityAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(2)});
        auto positionAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(0)});
        auto lifeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(1)});
        auto sizeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(3)});

        auto varDT = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, float32Type);
        builder.addName(varDT, "dt");

        auto particleCountAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, particleStats, std::vector<Id>{builder.makeIntConstant(1)});
        auto particleCount = builder.createLoad(particleCountAccess, spv::NoPrecision);


        // Generate: if(particleIndex >= totalCount)
        {
            auto shouldContinue = builder.createBinOp(spv::OpUGreaterThanEqual, builder.makeBoolType(), particleIndex, particleCount);
            spv::Builder::If ifValidIndex(shouldContinue, spv::SelectionControlMaskNone, builder);
            builder.makeReturn(false);
            ifValidIndex.makeEndIf();
        }

        auto dtTmp = builder.createLoad(dtAccess, spv::NoPrecision);
        builder.createStore(dtTmp, varDT);

        // update position
        auto dt = builder.createLoad(varDT, spv::NoPrecision);

        auto velocity = builder.createLoad(velocityAccess, spv::NoPrecision);
        auto velocityTimesDeltaTime = builder.createBinOp(spv::OpVectorTimesScalar, vec3Type, velocity, dt);
        auto position = builder.createLoad(positionAccess, spv::NoPrecision);
        auto updatedPosition = builder.createBinOp(spv::OpFAdd, vec3Type, velocityTimesDeltaTime, position);
        builder.createStore(updatedPosition, positionAccess);

        auto tmpLife = builder.createLoad(lifeAccess, spv::NoPrecision);
        auto updatedLife = builder.createBinOp(spv::OpFSub, float32Type, tmpLife, dt);
        builder.createStore(updatedLife, lifeAccess);

        auto castParticleID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, particleIndex);

        // user-specific code
        Variables vars{};
        vars.glslImport = glslImport;
        vars.shaderMode = shaderMode;
        vars.compute.deltaTime = dt;
        vars.emissionID = emissionID;
        vars.position = positionAccess;
        vars.life = builder.createLoad(lifeAccess, spv::NoPrecision);
        vars.velocity = velocityAccess;
        vars.size = sizeAccess;
        vars.particleIndex = castParticleID;
        for(auto& expr : expressions) {
            builder.getStringId(expr->toString());

            SPIRVisitor exprBuilder(builder, vars);
            exprBuilder.visit(expr);
        }

        builder.leaveFunction();
        auto entryPoint = builder.addEntryPoint(spv::ExecutionModelGLCompute, mainFunction, "main");
        entryPoint->addIdOperand(globalInvocationId);
        entryPoint->addIdOperand(particleStats);
        entryPoint->addIdOperand(descriptorSet);

        builder.addExecutionMode(mainFunction, spv::ExecutionModeLocalSize, 1024, 1, 1);
    }
}