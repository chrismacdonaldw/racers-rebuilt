#include "assets/gltf_data.hpp"

#include <algorithm>
#include <format>
#include <string_view>

namespace rr::assets {
namespace {

bool HasEmbeddedBuffers(const cgltf_data& data) {
    return std::ranges::all_of(
        std::span(data.buffers, data.buffers_count), [](const cgltf_buffer& buffer) {
            return buffer.uri == nullptr || std::string_view(buffer.uri).starts_with("data:");
        });
}

}

Result<GltfData> ParseGlb(std::span<const std::byte> bytes) {
    const cgltf_options options{};
    cgltf_data* raw = nullptr;
    cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
    GltfData data(raw, cgltf_free);
    if (result == cgltf_result_success && !HasEmbeddedBuffers(*data)) {
        result = cgltf_result_file_not_found;
    }
    if (result == cgltf_result_success) {
        result = cgltf_load_buffers(&options, data.get(), nullptr);
    }
    if (result == cgltf_result_success) {
        result = cgltf_validate(data.get());
    }
    if (result != cgltf_result_success) {
        return Fail(ErrorCode::InvalidData,
                    std::format("not a self-contained glTF binary (cgltf error {})",
                                static_cast<int>(result)));
    }
    return data;
}

}
