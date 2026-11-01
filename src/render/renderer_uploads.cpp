#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <limits>
#include <utility>
#include <vector>

#include "render/gpu_error.hpp"
#include "render/renderer.hpp"

namespace rr::render {

Result<MeshId> Renderer::UploadMesh(const assets::GeometryAsset& geometry) {
    const std::span<const assets::GeometryVertex> vertices = geometry.vertices;
    const std::span<const std::uint32_t> indices = geometry.indices;
    const bool hasValidIndices =
        std::ranges::all_of(indices, [&](std::uint32_t index) { return index < vertices.size(); });
    if (indices.empty() || indices.size() % 3 != 0 || !hasValidIndices) {
        return Fail(ErrorCode::InvalidData,
                    "mesh indices aren't a triangle list into its vertices");
    }

    Result<GpuBuffer> vertexBuffer =
        CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertices.size_bytes());
    Result<GpuBuffer> indexBuffer = vertexBuffer.and_then([&](const GpuBuffer&) {
        return CreateBuffer(SDL_GPU_BUFFERUSAGE_INDEX, indices.size_bytes());
    });
    if (!indexBuffer) {
        return std::unexpected(std::move(indexBuffer.error()));
    }
    const std::array parts{
        UploadPart{.bytes = std::as_bytes(vertices), .buffer = vertexBuffer->Get()},
        UploadPart{.bytes = std::as_bytes(indices), .buffer = indexBuffer->Get()},
    };
    return UploadBuffers(parts).transform([&] {
        return meshes_.Add({.vertices = std::move(*vertexBuffer),
                            .indices = std::move(*indexBuffer),
                            .indexCount = static_cast<std::uint32_t>(indices.size())});
    });
}

Result<TextureId> Renderer::UploadTexture(const assets::ImageAsset& image) {
    const std::size_t pixelCount = std::size_t{image.widthPx} * image.heightPx;
    if (pixelCount == 0 || image.rgba.size() != pixelCount * 4) {
        return Fail(ErrorCode::InvalidData,
                    std::format("a {}x{} image needs {} bytes of RGBA, not {}", image.widthPx,
                                image.heightPx, pixelCount * 4, image.rgba.size()));
    }
    const SDL_GPUTextureCreateInfo info{.type = SDL_GPU_TEXTURETYPE_2D,
                                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                        .width = image.widthPx,
                                        .height = image.heightPx,
                                        .layer_count_or_depth = 1,
                                        .num_levels = 1};
    GpuTexture texture(device_.get(), SDL_CreateGPUTexture(device_.get(), &info));
    if (texture.Get() == nullptr) {
        return GpuFailure("can't create texture");
    }
    const std::array<std::span<const std::byte>, 1> parts{image.rgba};
    Result<GpuTransferBuffer> staged = Stage(parts);
    if (!staged) {
        return std::unexpected(std::move(staged.error()));
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_.get());
    if (commands == nullptr) {
        return GpuFailure("can't start a texture upload");
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    const SDL_GPUTextureTransferInfo source{.transfer_buffer = staged->Get()};
    const SDL_GPUTextureRegion destination{
        .texture = texture.Get(), .w = image.widthPx, .h = image.heightPx, .d = 1};
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        return GpuFailure("can't submit a texture upload");
    }
    return textures_.Add(std::move(texture));
}

Result<GpuBuffer> Renderer::CreateBuffer(SDL_GPUBufferUsageFlags usage, std::size_t sizeBytes) {
    if (sizeBytes == 0 || sizeBytes > std::numeric_limits<std::uint32_t>::max()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("can't create a GPU buffer of {} bytes", sizeBytes));
    }
    const SDL_GPUBufferCreateInfo info{.usage = usage,
                                       .size = static_cast<std::uint32_t>(sizeBytes)};
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_.get(), &info);
    if (buffer == nullptr) {
        return GpuFailure("can't create a GPU buffer");
    }
    return GpuBuffer(device_.get(), buffer);
}

Result<Renderer::StreamBuffer> Renderer::CreateStream(std::size_t sizeBytes) {
    Result<GpuBuffer> vertices = CreateBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, sizeBytes);
    if (!vertices) {
        return std::unexpected(std::move(vertices.error()));
    }
    const SDL_GPUTransferBufferCreateInfo info{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                               .size = static_cast<std::uint32_t>(sizeBytes)};
    GpuTransferBuffer upload(device_.get(), SDL_CreateGPUTransferBuffer(device_.get(), &info));
    if (upload.Get() == nullptr) {
        return GpuFailure("can't create a per-frame upload buffer");
    }
    return StreamBuffer{.vertices = std::move(*vertices), .upload = std::move(upload)};
}

Result<GpuTransferBuffer> Renderer::Stage(std::span<const std::span<const std::byte>> parts) {
    std::size_t sizeBytes = 0;
    for (const std::span<const std::byte> part : parts) {
        sizeBytes += part.size();
    }
    if (sizeBytes > std::numeric_limits<std::uint32_t>::max()) {
        return Fail(ErrorCode::InvalidData, std::format("can't stage {} bytes at once", sizeBytes));
    }
    const SDL_GPUTransferBufferCreateInfo info{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                               .size = static_cast<std::uint32_t>(sizeBytes)};
    GpuTransferBuffer staging(device_.get(), SDL_CreateGPUTransferBuffer(device_.get(), &info));
    void* mapped = staging.Get() == nullptr
                       ? nullptr
                       : SDL_MapGPUTransferBuffer(device_.get(), staging.Get(), false);
    if (mapped == nullptr) {
        return GpuFailure("can't stage an upload");
    }
    auto* cursor = static_cast<std::byte*>(mapped);
    for (const std::span<const std::byte> part : parts) {
        cursor = std::ranges::copy(part, cursor).out;
    }
    SDL_UnmapGPUTransferBuffer(device_.get(), staging.Get());
    return staging;
}

Result<void> Renderer::UploadBuffers(std::span<const UploadPart> parts) {
    std::vector<std::span<const std::byte>> bytes;
    for (const UploadPart& part : parts) {
        bytes.push_back(part.bytes);
    }
    Result<GpuTransferBuffer> staged = Stage(bytes);
    if (!staged) {
        return std::unexpected(std::move(staged.error()));
    }
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_.get());
    if (commands == nullptr) {
        return GpuFailure("can't start a buffer upload");
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    std::uint32_t offset = 0;
    for (const UploadPart& part : parts) {
        const auto size = static_cast<std::uint32_t>(part.bytes.size());
        const SDL_GPUTransferBufferLocation source{.transfer_buffer = staged->Get(),
                                                   .offset = offset};
        const SDL_GPUBufferRegion destination{.buffer = part.buffer, .size = size};
        SDL_UploadToGPUBuffer(copy, &source, &destination, false);
        offset += size;
    }
    SDL_EndGPUCopyPass(copy);
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        return GpuFailure("can't submit a buffer upload");
    }
    return {};
}

Result<void> Renderer::UploadFrame(SDL_GPUCommandBuffer* commands, std::span<const DebugLine> lines,
                                   std::span<const QuadDraw> quads,
                                   std::span<const SpriteDraw> sprites) {
    lineVertices_.clear();
    for (const DebugLine& line : lines) {
        lineVertices_.push_back({.position = line.from, .color = line.color});
        lineVertices_.push_back({.position = line.to, .color = line.color});
    }
    BatchQuads(quads, quadVertices_, quadBatches_);
    BatchSprites(sprites, spriteVertices_, spriteBatches_);
    if (lines.empty() && quads.empty() && sprites.empty()) {
        return {};
    }
    const std::array uploads{
        StreamUpload{.stream = &lineStream_, .bytes = std::as_bytes(std::span(lineVertices_))},
        StreamUpload{.stream = &quadStream_, .bytes = std::as_bytes(std::span(quadVertices_))},
        StreamUpload{.stream = &spriteStream_, .bytes = std::as_bytes(std::span(spriteVertices_))}};
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
    Result<void> uploaded;
    for (const StreamUpload& upload : uploads) {
        if (upload.bytes.empty()) {
            continue;
        }
        SDL_GPUTransferBuffer* staging = upload.stream->upload.Get();
        void* mapped = SDL_MapGPUTransferBuffer(device_.get(), staging, true);
        if (mapped == nullptr) {
            uploaded = GpuFailure("can't map per-frame vertices");
            break;
        }
        std::ranges::copy(upload.bytes, static_cast<std::byte*>(mapped));
        SDL_UnmapGPUTransferBuffer(device_.get(), staging);
        const SDL_GPUTransferBufferLocation source{.transfer_buffer = staging};
        const SDL_GPUBufferRegion destination{
            .buffer = upload.stream->vertices.Get(),
            .size = static_cast<std::uint32_t>(upload.bytes.size())};
        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    }
    SDL_EndGPUCopyPass(copy);
    return uploaded;
}

}
