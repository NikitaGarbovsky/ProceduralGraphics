#include <glew.h>

/// <summary>
/// Goal: Submit as few, as well-organized draw calls as possible while drawing
/// exactly & ONLY what the camera needs.
/// 
/// This module manages the render pipeline that is run per frame. (Called in RendererCore, Render())
/// Pipeline stages produce a compact & efficient command list, execution consumes it.
/// </summary>
export module RendererPipeline;

// Includes
import RendererEntitys;
import RendererFrame;
import <algorithm>;
import <gtc/matrix_transform.hpp>;
import <gtc/type_ptr.hpp>;

// Function prototypes
void UpdateTransformsAndViewFrustrum(FrameContext& _f);         // Step 1
void CheckVisibility(FrameContext& _f);                         // Step 2
void BuildRenderItems(FrameContext& _f);                        // Step 3
void SortAndBatch(FrameContext& _f);                            // Step 4
void ExecuteCommands(FrameContext& _f);                         // Step 5
static bool SphereInFrustum(const glm::vec3& _center, float _radius, const glm::vec4 _planes[6]);
static void ExtractFrustumPlanes(const glm::mat4& _vp, glm::vec4 _outPlanes[6]);

// Helper
static uint64_t MakeSortKey(MaterialID _material, MeshID _mesh) {
    // Sort by material then mesh
    return (uint64_t(_material) << 32) | uint64_t(_mesh);
}

// Step 1: Compute Ready-To-Render data.
export void UpdateTransformsAndViewFrustrum(FrameContext& _f) {

    // Compute frustum planes once per frame
    _f.view = GCamera.view;
    _f.proj = GCamera.proj;

    _f.viewProj = _f.proj * _f.view;

    // This caches the view frustrum so 
    ExtractFrustumPlanes(_f.viewProj, _f.frustrumPlanes); 

    // Loops through all of the entity transforms and update their transforms.
    for (size_t i = 0; i < EntityTransforms.worldMatrix.size(); i++) {

        // Grab references to the transform data
        const auto& position = EntityTransforms.position[i];
        const auto& rotation = EntityTransforms.rotation[i];
        const auto& scale = EntityTransforms.scale[i];

        // Create a new matrix, update it with the reference transform data,
        glm::mat4 transformUpdateMatrix(1.0f);
        transformUpdateMatrix = glm::translate(transformUpdateMatrix, position);
        transformUpdateMatrix = glm::rotate(transformUpdateMatrix, rotation.x, glm::vec3(1, 0, 0));
        transformUpdateMatrix = glm::rotate(transformUpdateMatrix, rotation.y, glm::vec3(0, 1, 0));
        transformUpdateMatrix = glm::rotate(transformUpdateMatrix, rotation.z, glm::vec3(0, 0, 1));
        transformUpdateMatrix = glm::scale(transformUpdateMatrix, scale);

        // Apply the matrix.
        EntityTransforms.worldMatrix[i] = transformUpdateMatrix;
    }
}

// Step 2: Decide which render entities are worth to consider sending to the GPU.
export void CheckVisibility(FrameContext& _frame) {
    
    _frame.visible.clear();
    _frame.visible.reserve(CurrentRenderedEntitys.size()); // #TODO use custom arena allocator here

    for (uint32_t entIndex = 0; entIndex < (uint32_t)CurrentRenderedEntitys.size(); entIndex++)
    {
        const REntity& e = CurrentRenderedEntitys[entIndex];
        const Mesh& mesh = REntityMeshs[e.mesh];

        // Transform mesh local bounds center into world space
        const glm::mat4& M = EntityTransforms.worldMatrix[entIndex];
        glm::vec3 worldCenter = glm::vec3(M * glm::vec4(mesh.localBounds.center, 1.0f));

        // Conservative world radius: scale local radius by max axis scale
        glm::vec3 s = EntityTransforms.scale[entIndex];
        float maxScale = std::max({ std::abs(s.x), std::abs(s.y), std::abs(s.z) });
        float worldRadius = mesh.localBounds.radius * maxScale;

        // Early out, if objects are completely behind the camera (not visible), just skip frustrum test.
        glm::vec3 toObj = worldCenter - _frame.cameraPos;
        if (glm::dot(_frame.cameraForward, toObj) < -worldRadius) continue;

        if (SphereInFrustum(worldCenter, worldRadius, _frame.frustrumPlanes))
            _frame.visible.push_back(entIndex);
    }
}

// Step 3: Build the per-frame render queue.
// Goal: Convert visible world entities into draw-ready records that can be sorted/batched.
// 
// It produces two per-frame arrays:
// 1) instances[] : per-object payload uploaded to GPU once (currently just Model matrix)
// 2) items[]     : lightweight draw descriptors (program/vao/texture/indexCount + instanceIndex)
//                  used by the next stage to sort & batch into fewer draw calls.
export void BuildRenderItems(FrameContext& _frame) {
    // Clear previous frame data. FrameContext is scratch memory rebuilt every frame.
    _frame.items.clear();
    _frame.instances.clear();

    // Allocations (will be removed at some stage)
    _frame.items.reserve(_frame.visible.size()); // TODO use custom arena allocator here
    _frame.instances.reserve(_frame.visible.size()); // TODO use custom arena allocator here

    // For each visible entity: gather render state, write instance payload and emit a RenderItem.
    for (uint32_t entIndex : _frame.visible) {
        // Grab references to the visible render entity, mesh & material using its ID's.
        const REntity& e = CurrentRenderedEntitys[entIndex];
        const Mesh& mesh = REntityMeshs[e.mesh];
        const Material& mat = REntityMaterials[e.material];

        // Append this visible render entities payload (model matrix) 
        uint32_t instanceIndex = (uint32_t)_frame.instances.size();
        _frame.instances.push_back(InstanceData{ EntityTransforms.worldMatrix[entIndex] });

        // Construct the render queue entry
        RenderItem renderItem;
        renderItem.material = e.material; // Needed later for cached uniform locations
        renderItem.mesh = e.mesh;
        renderItem.indexCount = mesh.indexCount;
        renderItem.instanceIndex = instanceIndex;
        renderItem.sortKey = MakeSortKey(renderItem.material, renderItem.mesh);

        // Append it to the frame item array
        _frame.items.push_back(renderItem);
    }
}

// Step 4: Sort + Batch.
// Goal: Turn the per-visible-object RenderItems into a compact list of DrawCommands.
// #TODO, this is basic batching, upgrade this if necessary.
export void SortAndBatch(FrameContext& _frame) {

    // Sort by state to minimize program/vao/texture changes and enable easy batching.
    std::sort(_frame.items.begin(), _frame.items.end(),
        [](const RenderItem& a, const RenderItem& b) { return a.sortKey < b.sortKey; });

    _frame.commands.clear();
    if (_frame.items.empty()) return;

    // After sorting items, their associated instance payloads are no longer in draw order.
    // Build a reordered instance array that matches the sorted item order so that:
    // - each batch maps to a contiguous slice of instances[]
    // - we can upload instances[] once and draw ranges via instanceOffset/Count.
    std::vector<InstanceData> sortedInstances;
    sortedInstances.reserve(_frame.items.size()); // TODO use custom arena allocator here

    // Used to create 1 draw command for the current batch.
    auto flush = [&](const RenderItem& first, uint32_t offset, uint32_t count) {
        DrawCommand cmd;
        cmd.material = first.material;
        cmd.mesh = first.mesh;
        cmd.indexCount = first.indexCount;
        cmd.instanceOffset = offset;
        cmd.instanceCount = count;
        _frame.commands.push_back(cmd);
        };

    // Track the state of the current batch.
    RenderItem first = _frame.items[0];
    uint32_t batchOffset = 0;
    uint32_t batchCount = 0; // Number of instances in this batch.

    for (size_t i = 0; i < _frame.items.size(); i++) {
        const auto& it = _frame.items[i];

        // Append this item's instance payload in sorted draw order.
        sortedInstances.push_back(_frame.instances[it.instanceIndex]);

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
    _frame.instances.swap(sortedInstances);
}

static void BindInstanceAttribs(GLuint _vao, GLuint _instanceVBO, uintptr_t _baseByteOffset) {
    // Instance matrix at locations 4,5,6,7 (each a vec4), divisor=1
    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _instanceVBO);

    constexpr GLuint loc = 4;
    constexpr GLsizei stride = sizeof(InstanceData);

    for (GLuint i = 0; i < 4; i++) {
        glEnableVertexAttribArray(loc + i);
        glVertexAttribPointer(loc + i, 4, GL_FLOAT, GL_FALSE, stride,
            (const void*)(_baseByteOffset + sizeof(float) * 4 * i));
        glVertexAttribDivisor(loc + i, 1);
    }
}
// Step 5: Execute draw commands 
// Goal: Upload per-frame instance payload once, then iterate the compact command list.
// We bind expensive GPU state (program/VAO/texture) only when it changes.
// Each DrawCommand results in one instanced draw call that renders many objects. 
export void ExecuteCommands(FrameContext& _frame) {
    if (_frame.commands.empty()) return;

    // Upload all per-instance data in a single streaming update.
    // The instance array was reordered in SortAndBatch so that each batch corresponds
    // to a contiguous range (instanceOffset, instanceOffset + instanceCount).
    glBindBuffer(GL_ARRAY_BUFFER, _frame.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
        (GLsizeiptr)(_frame.instances.size() * sizeof(InstanceData)),
        _frame.instances.data(),
        GL_STREAM_DRAW);

    // Track last bound state so we avoid redundant binds.
    GLuint lastProgram = 0;
    GLuint lastVAO = 0;
    GLuint lastTex0 = 0;

    for (const auto& cmd : _frame.commands) {

        const Material& mat = REntityMaterials[cmd.material];
        const Mesh& mesh = REntityMeshs[cmd.mesh];

        // Bind shader program only when it changes.
        if (mat.program != lastProgram) {
            glUseProgram(mat.program);
            lastProgram = mat.program;

            // Upload per-frame uniforms once per program.
            if (mat.uView >= 0) glUniformMatrix4fv(mat.uView, 1, GL_FALSE, glm::value_ptr(_frame.view));
            if (mat.uProj >= 0) glUniformMatrix4fv(mat.uProj, 1, GL_FALSE, glm::value_ptr(_frame.proj));
        }

        // Bind texture only when it changes.
        if (mat.tex0 != lastTex0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mat.tex0);
            lastTex0 = mat.tex0;
        }

        // Bind VAO only when it changes.
        // This sets up the per-instance attribute layout on that VAO.
        if (mesh.vao != lastVAO) {
            BindInstanceAttribs(mesh.vao, _frame.instanceVBO, 0);
            lastVAO = mesh.vao;
        }

        // Point instanced attributes at the correct slice of the instance buffer for this batch.
        // This is how we implement instanceOffset without BaseInstance drawing.
        uintptr_t base = uintptr_t(cmd.instanceOffset) * sizeof(InstanceData);
        BindInstanceAttribs(mesh.vao, _frame.instanceVBO, base);

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

// #TODO move this somewhere else
static bool SphereInFrustum(const glm::vec3& _center, float _radius, const glm::vec4 _planes[6])
{
    for (int i = 0; i < 6; i++) {
        const glm::vec4& p = _planes[i];
        float dist = p.x * _center.x + p.y * _center.y + p.z * _center.z + p.w;
        if (dist < -_radius) return false; // completely outside this plane
    }
    return true;
}

// #TODO move this somewhere else
static void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 outPlanes[6])
{
    // Build rows from column-major matrix (glm::mat4 is column-major)
    const glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
    const glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
    const glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    const glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

    // Planes: (a,b,c,d) with ax + by + cz + d = 0
    outPlanes[0] = row3 + row0; // Left
    outPlanes[1] = row3 - row0; // Right
    outPlanes[2] = row3 + row1; // Bottom
    outPlanes[3] = row3 - row1; // Top
    outPlanes[4] = row3 + row2; // Near  
    outPlanes[5] = row3 - row2; // Far

    // Normalize planes (so distance tests are correct)
    for (int i = 0; i < 6; i++) {
        glm::vec3 n(outPlanes[i]);
        float len = glm::length(n);
        if (len > 0.0f) outPlanes[i] /= len;
    }
}
