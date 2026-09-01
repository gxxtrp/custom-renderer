#include <engine/renderer/render_graph.hpp>

#include <algorithm>
#include <queue>
#include <unordered_set>

#include <engine/core/log.hpp>

namespace engine::renderer {

RenderGraph::RenderGraph() {
  m_pool = &m_internalPool;
  m_textures.reserve(32);
  m_buffers.reserve(32);
  m_passes.reserve(32);
  m_executionOrder.reserve(32);
  m_postImageBarriers.reserve(8);
}

RenderGraph::RenderGraph(TransientResourcePool &externalPool)
    : m_pool(&externalPool) {
  m_textures.reserve(32);
  m_buffers.reserve(32);
  m_passes.reserve(32);
  m_executionOrder.reserve(32);
  m_postImageBarriers.reserve(8);
}

RenderGraph::~RenderGraph() = default;

RGTextureHandle RenderGraph::createTexture(const RGTextureDesc &desc) {
  const auto id = m_activeTextureCount++;
  if (id >= m_textures.size()) {
    m_textures.emplace_back();
  }
  auto &vTex = m_textures[id];
  vTex.desc = desc;
  vTex.physicalTexture = nullptr;
  vTex.isImported = false;
  vTex.initialLayout = rhi::ImageLayout::Undefined;
  vTex.currentLayout = rhi::ImageLayout::Undefined;
  vTex.finalLayout = rhi::ImageLayout::Undefined;
  vTex.currentStage = rhi::PipelineStageFlags::None;
  vTex.currentAccess = rhi::AccessFlags::None;
  vTex.firstPass = ~0u;
  vTex.lastPass = ~0u;
  vTex.writtenByPass = ~0u;
  return RGTextureHandle{id};
}

RGBufferHandle RenderGraph::createBuffer(const RGBufferDesc &desc) {
  const auto id = m_activeBufferCount++;
  if (id >= m_buffers.size()) {
    m_buffers.emplace_back();
  }
  auto &vBuf = m_buffers[id];
  vBuf.desc = desc;
  vBuf.physicalBuffer = nullptr;
  vBuf.isImported = false;
  vBuf.currentStage = rhi::PipelineStageFlags::None;
  vBuf.currentAccess = rhi::AccessFlags::None;
  vBuf.firstPass = ~0u;
  vBuf.lastPass = ~0u;
  vBuf.writtenByPass = ~0u;
  return RGBufferHandle{id};
}

RGTextureHandle RenderGraph::importTexture(std::string_view name,
                                           rhi::Texture *texture,
                                           rhi::ImageLayout initialLayout,
                                           rhi::ImageLayout finalLayout) {
  const auto id = m_activeTextureCount++;
  if (id >= m_textures.size()) {
    m_textures.emplace_back();
  }
  auto &vTex = m_textures[id];
  vTex.desc.name = std::string(name);
  if (texture != nullptr) {
    vTex.desc.width = texture->getWidth();
    vTex.desc.height = texture->getHeight();
    vTex.desc.depth = texture->getDepth();
    vTex.desc.mipLevels = texture->getMipLevels();
    vTex.desc.arrayLayers = texture->getArrayLayers();
    vTex.desc.format = texture->getFormat();
    vTex.desc.usage = texture->getUsage();
    vTex.desc.dimension = texture->getDimension();
  }
  vTex.physicalTexture = texture;
  vTex.isImported = true;
  vTex.initialLayout = initialLayout;
  vTex.currentLayout = initialLayout;
  vTex.finalLayout = finalLayout;
  vTex.currentStage = rhi::PipelineStageFlags::None;
  vTex.currentAccess = rhi::AccessFlags::None;
  vTex.firstPass = ~0u;
  vTex.lastPass = ~0u;
  vTex.writtenByPass = ~0u;
  return RGTextureHandle{id};
}

RGBufferHandle RenderGraph::importBuffer(std::string_view name,
                                         rhi::Buffer *buffer) {
  const auto id = m_activeBufferCount++;
  if (id >= m_buffers.size()) {
    m_buffers.emplace_back();
  }
  auto &vBuf = m_buffers[id];
  vBuf.desc.name = std::string(name);
  if (buffer != nullptr) {
    vBuf.desc.size = buffer->getSize();
    vBuf.desc.usage = buffer->getUsage();
    vBuf.desc.memoryUsage = buffer->getMemoryUsage();
  }
  vBuf.physicalBuffer = buffer;
  vBuf.isImported = true;
  vBuf.currentStage = rhi::PipelineStageFlags::None;
  vBuf.currentAccess = rhi::AccessFlags::None;
  vBuf.firstPass = ~0u;
  vBuf.lastPass = ~0u;
  vBuf.writtenByPass = ~0u;
  return RGBufferHandle{id};
}

void RenderGraph::addPass(std::string_view name, RGPassType type,
                          RGPassSetupCallback setup,
                          RGPassExecuteCallback execute) {
  const auto passIndex = m_activePassCount++;
  if (passIndex >= m_passes.size()) {
    m_passes.emplace_back();
  }
  auto &pass = m_passes[passIndex];
  pass.name = std::string(name);
  pass.type = type;
  pass.textureAccesses.clear();
  pass.bufferAccesses.clear();
  pass.colorAttachments.clear();
  pass.depthAttachment.reset();
  pass.imageBarriers.clear();
  pass.bufferBarriers.clear();
  pass.isSideEffect = false;
  pass.isCulled = false;
  pass.executeCallback = std::move(execute);

  RenderPassBuilder builder(*this, passIndex);
  if (setup) {
    setup(builder);
  }
}

void RenderGraph::addTextureAccess(core::u32 passIndex,
                                   const RGTextureAccess &access) {
  if (passIndex < m_activePassCount && access.handle.isValid() &&
      access.handle.id < m_activeTextureCount) {
    m_passes[passIndex].textureAccesses.push_back(access);
    if (access.isWrite) {
      m_textures[access.handle.id].writtenByPass = passIndex;
      if (m_textures[access.handle.id].isImported) {
        m_passes[passIndex].isSideEffect = true;
      }
    }
  }
}

void RenderGraph::addBufferAccess(core::u32 passIndex,
                                  const RGBufferAccess &access) {
  if (passIndex < m_activePassCount && access.handle.isValid() &&
      access.handle.id < m_activeBufferCount) {
    m_passes[passIndex].bufferAccesses.push_back(access);
    if (access.isWrite) {
      m_buffers[access.handle.id].writtenByPass = passIndex;
      if (m_buffers[access.handle.id].isImported) {
        m_passes[passIndex].isSideEffect = true;
      }
    }
  }
}

void RenderGraph::addColorAttachment(core::u32 passIndex,
                                     const RGColorAttachmentInfo &info) {
  if (passIndex < m_activePassCount) {
    m_passes[passIndex].colorAttachments.push_back(info);
  }
}

void RenderGraph::setDepthAttachment(core::u32 passIndex,
                                     const RGDepthAttachmentInfo &info) {
  if (passIndex < m_activePassCount) {
    m_passes[passIndex].depthAttachment = info;
  }
}

void RenderGraph::setPassSideEffect(core::u32 passIndex,
                                    bool sideEffect) noexcept {
  if (passIndex < m_activePassCount) {
    m_passes[passIndex].isSideEffect = sideEffect;
  }
}

void RenderGraph::compile() {
  // Compile without device: computes DAG, culls passes, sorts topologically
  const auto numPasses = m_activePassCount;
  if (numPasses == 0) {
    m_isCompiled = true;
    return;
  }

  // 1. Pass Culling: Identify active passes
  std::vector<bool> isPassActive(numPasses, false);
  std::queue<core::u32> liveQueue;

  for (core::u32 i = 0; i < numPasses; ++i) {
    if (m_passes[i].isSideEffect) {
      isPassActive[i] = true;
      liveQueue.push(i);
    }
  }

  // If no pass was explicitly marked as side effect, keep all passes active
  if (liveQueue.empty()) {
    for (core::u32 i = 0; i < numPasses; ++i) {
      isPassActive[i] = true;
      liveQueue.push(i);
    }
  } else {
    // Backwards propagation
    while (!liveQueue.empty()) {
      const auto passIdx = liveQueue.front();
      liveQueue.pop();

      const auto &pass = m_passes[passIdx];
      for (const auto &texAccess : pass.textureAccesses) {
        if (!texAccess.isWrite && texAccess.handle.isValid()) {
          const auto writerPass = m_textures[texAccess.handle.id].writtenByPass;
          if (writerPass != core::u32(~0u) && writerPass < numPasses &&
              !isPassActive[writerPass]) {
            isPassActive[writerPass] = true;
            liveQueue.push(writerPass);
          }
        }
      }
      for (const auto &bufAccess : pass.bufferAccesses) {
        if (!bufAccess.isWrite && bufAccess.handle.isValid()) {
          const auto writerPass = m_buffers[bufAccess.handle.id].writtenByPass;
          if (writerPass != core::u32(~0u) && writerPass < numPasses &&
              !isPassActive[writerPass]) {
            isPassActive[writerPass] = true;
            liveQueue.push(writerPass);
          }
        }
      }
    }
  }

  for (core::u32 i = 0; i < numPasses; ++i) {
    m_passes[i].isCulled = !isPassActive[i];
  }

  // 2. Topological Sort (Kahn's algorithm)
  std::vector<std::vector<core::u32>> adj(numPasses);
  std::vector<core::u32> inDegree(numPasses, 0);

  for (core::u32 u = 0; u < numPasses; ++u) {
    if (m_passes[u].isCulled) {
      continue;
    }
    for (core::u32 v = u + 1; v < numPasses; ++v) {
      if (m_passes[v].isCulled) {
        continue;
      }
      // Check for dependency: v depends on u if v reads/writes a resource
      // written by u, or if v writes a resource read by u (WAR hazard)
      bool depends = false;
      for (const auto &accV : m_passes[v].textureAccesses) {
        for (const auto &accU : m_passes[u].textureAccesses) {
          if (accV.handle == accU.handle && (accU.isWrite || accV.isWrite)) {
            depends = true;
            break;
          }
        }
        if (depends) {
          break;
        }
      }
      if (!depends) {
        for (const auto &accV : m_passes[v].bufferAccesses) {
          for (const auto &accU : m_passes[u].bufferAccesses) {
            if (accV.handle == accU.handle && (accU.isWrite || accV.isWrite)) {
              depends = true;
              break;
            }
          }
          if (depends) {
            break;
          }
        }
      }

      if (depends) {
        adj[u].push_back(v);
        ++inDegree[v];
      }
    }
  }

  std::queue<core::u32> zeroInDegree;
  for (core::u32 i = 0; i < numPasses; ++i) {
    if (!m_passes[i].isCulled && inDegree[i] == 0) {
      zeroInDegree.push(i);
    }
  }

  m_executionOrder.clear();
  while (!zeroInDegree.empty()) {
    const auto u = zeroInDegree.front();
    zeroInDegree.pop();
    m_executionOrder.push_back(u);

    for (const auto v : adj[u]) {
      if (--inDegree[v] == 0) {
        zeroInDegree.push(v);
      }
    }
  }

  // Safety fallback: if cycle was detected, append any unvisited active passes
  if (m_executionOrder.size() < numPasses) {
    for (core::u32 i = 0; i < numPasses; ++i) {
      if (!m_passes[i].isCulled &&
          std::find(m_executionOrder.begin(), m_executionOrder.end(), i) ==
              m_executionOrder.end()) {
        m_executionOrder.push_back(i);
      }
    }
  }

  // 3. Lifetime Tracking
  for (core::u32 execIdx = 0; execIdx < m_executionOrder.size(); ++execIdx) {
    const auto passIdx = m_executionOrder[execIdx];
    const auto &pass = m_passes[passIdx];
    for (const auto &acc : pass.textureAccesses) {
      if (acc.handle.isValid()) {
        auto &vTex = m_textures[acc.handle.id];
        if (vTex.firstPass == core::u32(~0u)) {
          vTex.firstPass = execIdx;
        }
        vTex.lastPass = execIdx;
      }
    }
    for (const auto &acc : pass.bufferAccesses) {
      if (acc.handle.isValid()) {
        auto &vBuf = m_buffers[acc.handle.id];
        if (vBuf.firstPass == core::u32(~0u)) {
          vBuf.firstPass = execIdx;
        }
        vBuf.lastPass = execIdx;
      }
    }
  }

  m_isCompiled = true;
}

void RenderGraph::compile(rhi::Device &device) {
  compile();

  // 4. Physical Resource Allocation
  for (core::u32 i = 0; i < m_activeTextureCount; ++i) {
    auto &vTex = m_textures[i];
    if (!vTex.isImported && vTex.physicalTexture == nullptr &&
        vTex.firstPass != core::u32(~0u)) {
      vTex.physicalTexture = m_pool->acquireTexture(device, vTex.desc);
    }
  }

  for (core::u32 i = 0; i < m_activeBufferCount; ++i) {
    auto &vBuf = m_buffers[i];
    if (!vBuf.isImported && vBuf.physicalBuffer == nullptr &&
        vBuf.firstPass != core::u32(~0u)) {
      vBuf.physicalBuffer = m_pool->acquireBuffer(device, vBuf.desc);
    }
  }

  // 5. Automated Barrier Synthesis
  for (auto passIdx : m_executionOrder) {
    auto &pass = m_passes[passIdx];
    pass.imageBarriers.clear();
    pass.bufferBarriers.clear();

    for (const auto &acc : pass.textureAccesses) {
      if (!acc.handle.isValid()) {
        continue;
      }
      auto &vTex = m_textures[acc.handle.id];
      if (vTex.physicalTexture == nullptr) {
        continue;
      }

      const bool layoutMismatch = (vTex.currentLayout != acc.layout);
      const bool writeHazard =
          acc.isWrite || (vTex.currentAccess != rhi::AccessFlags::None);

      if (layoutMismatch || writeHazard) {
        rhi::ImageBarrier barrier{
            .texture = vTex.physicalTexture,
            .srcAccess = vTex.currentAccess,
            .dstAccess = acc.access,
            .srcStage = (vTex.currentStage == rhi::PipelineStageFlags::None)
                            ? rhi::PipelineStageFlags::TopOfPipe
                            : vTex.currentStage,
            .dstStage = acc.stage,
            .oldLayout = vTex.currentLayout,
            .newLayout = acc.layout};
        pass.imageBarriers.push_back(barrier);

        vTex.currentLayout = acc.layout;
        vTex.currentStage = acc.stage;
        vTex.currentAccess = acc.access;
      }
    }

    for (const auto &acc : pass.bufferAccesses) {
      if (!acc.handle.isValid()) {
        continue;
      }
      auto &vBuf = m_buffers[acc.handle.id];
      if (vBuf.physicalBuffer == nullptr) {
        continue;
      }

      if (acc.isWrite || (vBuf.currentAccess != rhi::AccessFlags::None)) {
        rhi::BufferBarrier barrier{
            .buffer = vBuf.physicalBuffer,
            .offset = 0,
            .size = vBuf.desc.size,
            .srcAccess = vBuf.currentAccess,
            .dstAccess = acc.access,
            .srcStage = (vBuf.currentStage == rhi::PipelineStageFlags::None)
                            ? rhi::PipelineStageFlags::TopOfPipe
                            : vBuf.currentStage,
            .dstStage = acc.stage};
        pass.bufferBarriers.push_back(barrier);

        vBuf.currentStage = acc.stage;
        vBuf.currentAccess = acc.access;
      }
    }
  }

  // 6. Post-Graph Transitions (e.g. swapchain image to PresentSrc)
  m_postImageBarriers.clear();
  for (core::u32 i = 0; i < m_activeTextureCount; ++i) {
    auto &vTex = m_textures[i];
    if (vTex.isImported && vTex.finalLayout != rhi::ImageLayout::Undefined &&
        vTex.currentLayout != vTex.finalLayout &&
        vTex.physicalTexture != nullptr) {
      rhi::ImageBarrier postBarrier{
          .texture = vTex.physicalTexture,
          .srcAccess = vTex.currentAccess,
          .dstAccess = rhi::AccessFlags::None,
          .srcStage = (vTex.currentStage == rhi::PipelineStageFlags::None)
                          ? rhi::PipelineStageFlags::ColorAttachmentOutput
                          : vTex.currentStage,
          .dstStage = rhi::PipelineStageFlags::BottomOfPipe,
          .oldLayout = vTex.currentLayout,
          .newLayout = vTex.finalLayout};
      m_postImageBarriers.push_back(postBarrier);
      vTex.currentLayout = vTex.finalLayout;
    }
  }
}

void RenderGraph::execute(rhi::Device &device, rhi::CommandBuffer &cmd) {
  ENGINE_PROFILE_ZONE_NAMED("RenderGraph::Execute");

  if (!m_isCompiled || m_postImageBarriers.empty()) {
    compile(device);
  }

  for (const auto passIdx : m_executionOrder) {
    auto &pass = m_passes[passIdx];
    if (pass.isCulled) {
      continue;
    }

    ENGINE_PROFILE_ZONE_NAMED("RenderPass");

    // Emit pre-pass barriers
    if (!pass.imageBarriers.empty() || !pass.bufferBarriers.empty()) {
      rhi::PipelineBarrierDesc barrierDesc{.bufferBarriers =
                                               pass.bufferBarriers,
                                           .imageBarriers = pass.imageBarriers};
      cmd.pipelineBarrier(barrierDesc);
    }

    // Execute pass
    if (pass.type == RGPassType::Graphics &&
        (!pass.colorAttachments.empty() || pass.depthAttachment.has_value())) {
      std::vector<rhi::RenderingAttachmentDesc> colorAttDescs;
      colorAttDescs.reserve(pass.colorAttachments.size());

      core::u32 renderWidth = 0;
      core::u32 renderHeight = 0;

      for (const auto &colorAtt : pass.colorAttachments) {
        auto *tex = getPhysicalTexture(colorAtt.handle);
        if (tex != nullptr) {
          if (renderWidth == 0) {
            renderWidth = tex->getWidth();
            renderHeight = tex->getHeight();
          }
          colorAttDescs.push_back(rhi::RenderingAttachmentDesc{
              .texture = tex,
              .clearValue = colorAtt.clearValue,
              .layout = rhi::ImageLayout::ColorAttachmentOptimal,
              .clearOnLoad = colorAtt.clearOnLoad});
        }
      }

      rhi::RenderingDesc renderingDesc{
          .colorAttachments = colorAttDescs,
          .renderArea = rhi::Rect2D{.offset = {.x = 0, .y = 0},
                                    .extent = {.width = renderWidth,
                                               .height = renderHeight}},
          .hasDepth = false};

      if (pass.depthAttachment.has_value()) {
        auto *depthTex = getPhysicalTexture(pass.depthAttachment->handle);
        if (depthTex != nullptr) {
          if (renderWidth == 0) {
            renderWidth = depthTex->getWidth();
            renderHeight = depthTex->getHeight();
            renderingDesc.renderArea.extent = {.width = renderWidth,
                                               .height = renderHeight};
          }
          renderingDesc.hasDepth = true;
          renderingDesc.depthAttachment = rhi::RenderingAttachmentDesc{
              .texture = depthTex,
              .depthStencilClearValue = pass.depthAttachment->clearValue,
              .layout = rhi::ImageLayout::DepthStencilAttachmentOptimal,
              .clearOnLoad = pass.depthAttachment->clearOnLoad};
        }
      }

      cmd.beginRendering(renderingDesc);
      if (pass.executeCallback) {
        pass.executeCallback(cmd);
      }
      cmd.endRendering();
    } else {
      if (pass.executeCallback) {
        pass.executeCallback(cmd);
      }
    }
  }

  // Post-graph transitions (e.g. to PresentSrc)
  if (!m_postImageBarriers.empty()) {
    rhi::PipelineBarrierDesc postBarriers{.imageBarriers = m_postImageBarriers};
    cmd.pipelineBarrier(postBarriers);
  }
}

void RenderGraph::reset() noexcept {
  m_pool->reset();
  for (core::u32 i = 0; i < m_activePassCount; ++i) {
    auto &pass = m_passes[i];
    pass.textureAccesses.clear();
    pass.bufferAccesses.clear();
    pass.colorAttachments.clear();
    pass.depthAttachment.reset();
    pass.imageBarriers.clear();
    pass.bufferBarriers.clear();
    pass.executeCallback = nullptr;
    pass.isSideEffect = false;
    pass.isCulled = false;
  }
  m_activeTextureCount = 0;
  m_activeBufferCount = 0;
  m_activePassCount = 0;
  m_executionOrder.clear();
  m_postImageBarriers.clear();
  m_isCompiled = false;
}

rhi::Texture *
RenderGraph::getPhysicalTexture(RGTextureHandle handle) const noexcept {
  if (handle.isValid() && handle.id < m_activeTextureCount) {
    return m_textures[handle.id].physicalTexture;
  }
  return nullptr;
}

rhi::Buffer *
RenderGraph::getPhysicalBuffer(RGBufferHandle handle) const noexcept {
  if (handle.isValid() && handle.id < m_activeBufferCount) {
    return m_buffers[handle.id].physicalBuffer;
  }
  return nullptr;
}

const RGTextureDesc *
RenderGraph::getTextureDesc(RGTextureHandle handle) const noexcept {
  if (handle.isValid() && handle.id < m_activeTextureCount) {
    return &m_textures[handle.id].desc;
  }
  return nullptr;
}

const RGBufferDesc *
RenderGraph::getBufferDesc(RGBufferHandle handle) const noexcept {
  if (handle.isValid() && handle.id < m_activeBufferCount) {
    return &m_buffers[handle.id].desc;
  }
  return nullptr;
}

} // namespace engine::renderer
