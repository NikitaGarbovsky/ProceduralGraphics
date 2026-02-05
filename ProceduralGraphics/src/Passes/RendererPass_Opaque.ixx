module;

#include <algorithm>
#include <glew.h>
#include <string>
#include <gtx/string_cast.hpp>

/// <summary>
/// The main Opaque pass of the renderering pipeline.
/// </summary>
export module RendererPass_Opaque;

import RendererFrame;
import RendererEntitys;
import <gtc/matrix_transform.hpp>;
import <gtc/type_ptr.hpp>;
import DebugUtilities;
import RendererTransformUtils;

// Function prototypes for helpers
static uint64_t MakeSortKey(MaterialID _material, MeshID _mesh);

// Opaque pass steps.
void UpdateREntityTransforms(PassContext& _p);                  // Step 1
void CheckVisibility(FrameCommon& _f, PassContext& _p);         // Step 2
void BuildRenderItems(PassContext& _p);                         // Step 3
void SortAndBatch(PassContext& _p);                             // Step 4
void ExecuteCommands(const FrameCommon& _f, PassContext& _p);   // Step 5

// Builds the Opaque pass step by step
export void OpaquePass_Build(FrameCommon& _f, PassContext& _p)
{
    UpdateREntityTransforms(_p);                                // Step 1
    CheckVisibility(_f, _p);                                    // Step 2
    BuildRenderItems(_p);                                       // Step 3
    SortAndBatch(_p);                                           // Step 4
}

export void OpaquePass_Execute(const FrameCommon& _f, PassContext& _p)
{
    ExecuteCommands(_f, _p);                                    // Step 5
}

// Step 1: Compute Ready-To-Render data.
void UpdateREntityTransforms(PassContext& _p) {
    // Loops through all of the entity transforms and updates them.
    for (size_t i = 0; i < EntityTransforms.worldMatrix.size(); i++) {
        // Update entitiy transform
        EntityTransforms.worldMatrix[i] = GetEntityModelMatrix(i);
    }
}

// Step 2: Decide which render entities are worth to consider sending to the GPU.
void CheckVisibility(FrameCommon& _f, PassContext& _p) {

    FrustrumCulledEntitiesThisFrame = 0;
    _p.visible.clear();
    _p.visible.reserve(CurrentRenderedEntitys.size()); // #TODO use custom arena allocator here

    // Debug to disable culling #TODO just enable this in a UI button when you do so.
    /*for (uint32_t entIndex = 0; entIndex < CurrentRenderedEntitys.size(); entIndex++)
    {
        _frame.visible.push_back(entIndex);
    }
    return;*/

    for (uint32_t entIndex{}; entIndex < (uint32_t)CurrentRenderedEntitys.size(); entIndex++)
    {
        const REntity& e = CurrentRenderedEntitys[entIndex];
        const Bounds& b = e.localBounds;

        const glm::mat4& M = EntityTransforms.worldMatrix[entIndex];
        glm::vec3 worldCenter = glm::vec3(M * glm::vec4(b.center, 1.0f));

        glm::vec3 s = EntityTransforms.scale[entIndex];
        float maxScale = std::max({ std::abs(s.x), std::abs(s.y), std::abs(s.z) });
        float worldRadius = b.radius * maxScale;

        // Early out, if objects are completely behind the camera (not visible), just skip frustrum test.
        glm::vec3 toObj = worldCenter - _f.cameraPos;
        if (glm::dot(_f.cameraForward, toObj) < -worldRadius) {
            continue;
        }

        bool culled = SphereInFrustum(worldCenter, worldRadius, _f.frustrumPlanes);
        if (culled)
        {
            // Debug output
            /*std::string str = "Successfully culled Entity: ";
            str += std::to_string(entIndex + 1);
            str += "  Center Value = ";
            str += glm::to_string(worldCenter);
            std::string str0 = " Radius = ";
            str0 += std::to_string(worldRadius);
            Log((str += str0).c_str());*/

            _p.visible.push_back(entIndex);
        }
            
        
    }
    // Updated for the Editor UI.
    FrustrumCulledEntitiesThisFrame = CurrentRenderedEntitys.size() - _p.visible.size();
}

// Step 3: Build the per-frame render queue.
// Goal: Convert visible world entities into draw-ready records that can be sorted/batched.
// 
// It produces two per-frame arrays:
// 1) instances[] : per-object payload uploaded to GPU once (currently just Model matrix)
// 2) items[]     : lightweight draw descriptors (program/vao/texture/indexCount + instanceIndex)
//                  used by the next stage to sort & batch into fewer draw calls.
void BuildRenderItems(PassContext& _p) {
    // Clear previous frame data. FrameContext is scratch memory rebuilt every frame.
    _p.items.clear();
    _p.instances.clear();

    size_t totalPrims = 0;
    for (uint32_t entIndex : _p.visible){
        totalPrims += CurrentRenderedEntitys[entIndex].submeshCount;
    }

    _p.items.reserve(totalPrims); // TODO use custom arena allocator here
    _p.instances.reserve(totalPrims); // TODO use custom arena allocator here

    // For each visible entity: gather render state, write instance payload and emit a RenderItem.
    for (uint32_t entIndex : _p.visible) {

        const REntity& e = CurrentRenderedEntitys[entIndex];

        for (uint32_t i = 0; i < e.submeshCount; ++i)
        {
            const Submesh& sm = REntitySubmeshes[e.firstSubmesh + i];
            const Mesh& mesh = REntityMeshs[sm.mesh];

            // Append this visible render entities payload (model matrix) 
            uint32_t instanceIndex = (uint32_t)_p.instances.size();
            InstanceData inst{};
            inst.model = EntityTransforms.worldMatrix[entIndex];
            inst.entityId = entIndex + 1; // SAME ID for all submeshes of this entity
            _p.instances.push_back(inst);

            // Construct the render queue entry
            RenderItem it{};
            it.material = sm.material;
            it.mesh = sm.mesh;
            it.indexCount = mesh.indexCount;
            it.instanceIndex = instanceIndex;
            it.sortKey = MakeSortKey(it.material, it.mesh);

            // Append it to the frame item array
            _p.items.push_back(it);
        }
    }
}

// Step 4: Sort + Batch.
// Goal: Turn the per-visible-object RenderItems into a compact list of DrawCommands.
// #TODO, this is basic batching, upgrade this if necessary.
void SortAndBatch(PassContext& _p) {

    // Sort by state to minimize program/vao/texture changes and enable easy batching.
    std::sort(_p.items.begin(), _p.items.end(),
        [](const RenderItem& a, const RenderItem& b) { return a.sortKey < b.sortKey; });

    _p.commands.clear();
    if (_p.items.empty()) return;

    // After sorting items, their associated instance payloads are no longer in draw order.
    // Build a reordered instance array that matches the sorted item order so that:
    // - each batch maps to a contiguous slice of instances[]
    // - we can upload instances[] once and draw ranges via instanceOffset/Count.
    std::vector<InstanceData> sortedInstances;
    sortedInstances.reserve(_p.items.size()); // TODO use custom arena allocator here

    // Used to create 1 draw command for the current batch.
    auto flush = [&](const RenderItem& first, uint32_t offset, uint32_t count) {
        DrawCommand cmd;
        cmd.material = first.material;
        cmd.mesh = first.mesh;
        cmd.indexCount = first.indexCount;
        cmd.instanceOffset = offset;
        cmd.instanceCount = count;
        _p.commands.push_back(cmd);
        };

    // Track the state of the current batch.
    RenderItem first = _p.items[0];
    uint32_t batchOffset = 0;
    uint32_t batchCount = 0; // Number of instances in this batch.

    for (size_t i = 0; i < _p.items.size(); i++) {
        const auto& it = _p.items[i];

        // Append this item's instance payload in sorted draw order.
        sortedInstances.push_back(_p.instances[it.instanceIndex]);

        // Determine if this item can be batched with the current batch.
        bool sameState =
            it.material == first.material && it.mesh == first.mesh;

        if (i == 0) {
            batchOffset = 0;
            batchCount = 1;
        }
        else if (sameState) {
            // Same state, extend the current batch (one more instance).
            batchCount++;
        }
        else {
            // State changed, flush previous batch and start a new one.
            flush(first, batchOffset, batchCount);
            batchOffset += batchCount;
            batchCount = 1;
            first = it;
        }
    }

    // Flush the final batch.
    flush(first, batchOffset, batchCount);

    // Replace instance list with the reordered one so instanceOffset/count are valid.
    _p.instances.swap(sortedInstances);
}

// Step 5: Execute draw commands 
// Goal: Upload per-frame instance payload once, then iterate the compact command list.
// We bind expensive GPU state (program/VAO/texture) only when it changes.
// Each DrawCommand results in one instanced draw call that renders many objects. 
export void ExecuteCommands(const FrameCommon& _f, PassContext& _p) {
    if (_p.commands.empty()) return;
    if (_p.instanceVBO == 0) { LogCrash("Instance VBO is invalid, you probably didn't initialize the renderer pipeline"); return; }

    // Upload all per-instance data in a single streaming update.
    // The instance array was reordered in SortAndBatch so that each batch corresponds
    // to a contiguous range (instanceOffset, instanceOffset + instanceCount).
    glBindBuffer(GL_ARRAY_BUFFER, _p.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(_p.instances.size() * sizeof(InstanceData)),
        _p.instances.data(),
        GL_STREAM_DRAW);

    // Track last bound state so we avoid redundant binds.
    GLuint lastProgram = 0;
    GLuint lastVAO = 0;
    GLuint lastTex0 = 0;

    for (const auto& cmd : _p.commands) {

        const Material& mat = REntityMaterials[cmd.material];
        const Mesh& mesh = REntityMeshs[cmd.mesh];

        // Find out if the program has changed,
        bool programChanged = (mat.program != lastProgram);

        // Bind shader program only when it changes.
        if (programChanged) {
            glUseProgram(mat.program);
            lastProgram = mat.program;

            // Upload per-frame uniforms once per program.
            if (mat.uView >= 0) glUniformMatrix4fv(mat.uView, 1, GL_FALSE, glm::value_ptr(_f.view));
            if (mat.uProj >= 0) glUniformMatrix4fv(mat.uProj, 1, GL_FALSE, glm::value_ptr(_f.proj));
            if (mat.uCameraPos >= 0) glUniform3fv(mat.uCameraPos, 1, glm::value_ptr(_f.cameraPos));
        }

        // Bind texture only when it changes.
        if (programChanged || mat.tex0 != lastTex0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mat.tex0);
            if (mat.uTex0 >= 0) glUniform1i(mat.uTex0, 0);
            lastTex0 = mat.tex0;
        }

        // Bind VAO only when it changes.
        // This sets up the per-instance attribute layout on that VAO.
        if (mesh.vao != lastVAO) {
            BindInstanceAttribs(mesh.vao, _p.instanceVBO, 0);
            lastVAO = mesh.vao;
        }

        // Point instanced attributes at the correct slice of the instance buffer for this batch.
        // This is how we implement instanceOffset without BaseInstance drawing.
        uintptr_t base = uintptr_t(cmd.instanceOffset) * sizeof(InstanceData);
        BindInstanceAttribs(mesh.vao, _p.instanceVBO, base);

        // Issue one instanced draw for this batch.
        glBindVertexArray(mesh.vao);
        glDrawElementsInstanced(GL_TRIANGLES,
            (GLsizei)cmd.indexCount,
            GL_UNSIGNED_INT,
            nullptr,
            (GLsizei)cmd.instanceCount);
    }

    // Clean up bindings 
    glBindVertexArray(0);
    glUseProgram(0);
}

// Helper
static uint64_t MakeSortKey(MaterialID _material, MeshID _mesh) {
    // Sort by material then mesh
    return (uint64_t(_material) << 32) | uint64_t(_mesh);
}
