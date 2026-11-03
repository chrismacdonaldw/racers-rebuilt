#include "assets/gltf_animation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/gltf_data.hpp"
#include "assets/gltf_joints.hpp"

#include <cgltf.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace rr::assets {
namespace {

constexpr float kMaxFrame = std::numeric_limits<std::uint16_t>::max();
constexpr std::size_t kMaxKeys = 4'000'000;

template <typename Value>
struct Key {
    std::uint16_t frame = 0;
    Value value;
};

struct NodeChannels {
    const cgltf_animation_sampler* rotation = nullptr;
    const cgltf_animation_sampler* translation = nullptr;
};

class AnimationBuilder {
public:
    explicit AnimationBuilder(AnimationAsset& out) : out_(out) {}

    KeyRun AddRotations(std::span<const Key<Quaternion>> keys) {
        return AddRun(keys, out_.rotationKeys);
    }

    KeyRun AddPositions(std::span<const Key<Vector3>> keys) {
        return AddRun(keys, out_.positionKeys);
    }

    KeyRun CopyRotations(const AnimationAsset& from, const KeyRun& run) {
        return CopyRun(from, run, from.rotationKeys, out_.rotationKeys);
    }

    KeyRun CopyPositions(const AnimationAsset& from, const KeyRun& run) {
        return CopyRun(from, run, from.positionKeys, out_.positionKeys);
    }

    void Add(const AnimationTrack& track) { out_.tracks.push_back(track); }

    void CopyTrack(const AnimationAsset& from, const AnimationTrack& track) {
        Add({.rotations = CopyRotations(from, track.rotations),
             .positions = CopyPositions(from, track.positions)});
    }

private:
    template <typename Value>
    KeyRun AddRun(std::span<const Key<Value>> keys, std::vector<Value>& pool) {
        if (keys.empty()) {
            return {};
        }
        const KeyRun run{.firstValue = static_cast<std::uint32_t>(pool.size()),
                         .firstFrame = static_cast<std::uint32_t>(out_.keyFrames.size()),
                         .count = static_cast<std::uint32_t>(keys.size())};
        for (const Key<Value>& key : keys) {
            pool.push_back(key.value);
            if (keys.size() > 1) {
                out_.keyFrames.push_back(key.frame);
            }
        }
        return keys.size() > 1 ? run : KeyRun{.firstValue = run.firstValue, .count = run.count};
    }

    template <typename Value>
    KeyRun CopyRun(const AnimationAsset& from, const KeyRun& run, const std::vector<Value>& source,
                   std::vector<Value>& pool) {
        std::vector<Key<Value>> keys;
        for (std::uint32_t i = 0; i < run.count; ++i) {
            const std::uint16_t frame =
                run.count > 1 ? from.keyFrames[run.firstFrame + i] : std::uint16_t{0};
            keys.push_back({.frame = frame, .value = source[run.firstValue + i]});
        }
        return AddRun(std::span<const Key<Value>>(keys), pool);
    }

    AnimationAsset& out_;
};

Result<std::uint16_t> ToFrame(float seconds, std::int32_t framesPerSecond) {
    const float frame = std::round(seconds * static_cast<float>(framesPerSecond));
    const bool isInRange = frame >= 0.0F && frame <= kMaxFrame;
    if (!isInRange) {
        return Fail(ErrorCode::InvalidData,
                    std::format("a glTF key at {} s is outside the game's frame range", seconds));
    }
    return static_cast<std::uint16_t>(frame);
}

Result<void> CheckSampler(const cgltf_animation_sampler& sampler, cgltf_type output) {
    const bool isReadable =
        sampler.input->type == cgltf_type_scalar && sampler.output->type == output &&
        sampler.output->count == sampler.input->count && sampler.input->buffer_view != nullptr &&
        sampler.output->buffer_view != nullptr && sampler.input->is_sparse == 0 &&
        sampler.output->is_sparse == 0;
    if (!isReadable) {
        return Fail(ErrorCode::InvalidData, "a glTF animation channel can't be read");
    }
    if (sampler.interpolation == cgltf_interpolation_type_cubic_spline) {
        return Fail(ErrorCode::InvalidData, "cubic spline glTF animations aren't supported");
    }
    return {};
}

bool IsUsable(const glm::vec4& raw, cgltf_type output) {
    const bool isFinite = std::isfinite(raw.x) && std::isfinite(raw.y) && std::isfinite(raw.z) &&
                          std::isfinite(raw.w);
    return isFinite && (output != cgltf_type_vec4 || glm::dot(raw, raw) > 0.0F);
}

template <typename Value, typename Convert>
Result<std::vector<Key<Value>>> ReadKeys(const cgltf_animation_sampler& sampler,
                                         std::int32_t framesPerSecond, cgltf_type output,
                                         std::size_t& keysLeft, Convert convert) {
    Result<void> checked = CheckSampler(sampler, output);
    if (!checked) {
        return std::unexpected(std::move(checked.error()));
    }
    const bool isStep = sampler.interpolation == cgltf_interpolation_type_step;
    const std::size_t keyCount = sampler.input->count * (isStep ? 2 : 1);
    if (keyCount > keysLeft) {
        return Fail(ErrorCode::InvalidData,
                    std::format("the glTF animations have more than {} keys", kMaxKeys));
    }
    keysLeft -= keyCount;
    std::vector<Key<Value>> keys;
    for (cgltf_size i = 0; i < sampler.input->count; ++i) {
        float seconds = 0.0F;
        cgltf_accessor_read_float(sampler.input, i, &seconds, 1);
        glm::vec4 raw{0.0F, 0.0F, 0.0F, 1.0F};
        cgltf_accessor_read_float(sampler.output, i, glm::value_ptr(raw),
                                  cgltf_num_components(output));
        Result<std::uint16_t> frame = ToFrame(seconds, framesPerSecond);
        if (!frame) {
            return std::unexpected(std::move(frame.error()));
        }
        if (!IsUsable(raw, output) || (!keys.empty() && *frame < keys.back().frame)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("the glTF animation key at {} s can't be used", seconds));
        }
        if (isStep && !keys.empty()) {
            keys.push_back({.frame = *frame, .value = keys.back().value});
        }
        keys.push_back({.frame = *frame, .value = convert(raw)});
    }
    return keys;
}

Quaternion ToGameRotation(const glm::vec4& gltf) {
    const Vector3 axis = ToGame(glm::vec3(gltf));
    return {.w = gltf.w, .x = axis.x, .y = axis.y, .z = axis.z};
}

template <typename Value>
std::int32_t LastFrame(const std::vector<Key<Value>>& keys) {
    return keys.empty() ? 0 : keys.back().frame;
}

class PartImporter {
public:
    PartImporter(AnimationBuilder& builder, const GltfJoints& joints,
                 const GltfAnimationTarget& target)
        : builder_(builder), joints_(joints), target_(target) {}

    Result<std::int32_t> AddTracks(const cgltf_animation& animation, const AnimationPart& part) {
        std::vector<NodeChannels> channels(target_.joints.size());
        for (const cgltf_animation_channel& channel :
             std::span(animation.channels, animation.channels_count)) {
            AssignChannel(channel, channels);
        }
        std::int32_t lastFrame = 0;
        for (std::size_t i = 0; i < channels.size(); ++i) {
            Result<std::int32_t> added = AddTrack(
                channels[i], target_.original.tracks[part.firstTrack + i], part.framesPerSecond);
            if (!added) {
                return added;
            }
            lastFrame = std::max(lastFrame, *added);
        }
        return lastFrame;
    }

private:
    void AssignChannel(const cgltf_animation_channel& channel,
                       std::vector<NodeChannels>& channels) const {
        if (channel.target_node == nullptr) {
            return;
        }
        const std::optional<std::size_t> joint = joints_.IndexOf(*channel.target_node);
        if (!joint) {
            return;
        }
        if (channel.target_path == cgltf_animation_path_type_rotation) {
            channels[*joint].rotation = channel.sampler;
        } else if (channel.target_path == cgltf_animation_path_type_translation) {
            channels[*joint].translation = channel.sampler;
        }
    }

    Result<std::int32_t> AddTrack(const NodeChannels& node, const AnimationTrack& original,
                                  std::int32_t framesPerSecond) {
        std::int32_t lastFrame = 0;
        Result<KeyRun> rotations =
            node.rotation == nullptr
                ? Result<KeyRun>(builder_.CopyRotations(target_.original, original.rotations))
                : ReadKeys<Quaternion>(*node.rotation, framesPerSecond, cgltf_type_vec4, keysLeft_,
                                       ToGameRotation)
                      .transform([&](const std::vector<Key<Quaternion>>& keys) {
                          lastFrame = LastFrame(keys);
                          return builder_.AddRotations(keys);
                      });
        if (!rotations) {
            return std::unexpected(std::move(rotations.error()));
        }
        const auto toGame = [this](const glm::vec4& raw) {
            return ToGame(glm::vec3(raw)) * (1.0F / target_.scale);
        };
        Result<KeyRun> positions =
            node.translation == nullptr
                ? Result<KeyRun>(builder_.CopyPositions(target_.original, original.positions))
                : ReadKeys<Vector3>(*node.translation, framesPerSecond, cgltf_type_vec3, keysLeft_,
                                    toGame)
                      .transform([&](const std::vector<Key<Vector3>>& keys) {
                          lastFrame = std::max(lastFrame, LastFrame(keys));
                          return builder_.AddPositions(keys);
                      });
        if (!positions) {
            return std::unexpected(std::move(positions.error()));
        }
        builder_.Add({.rotations = *rotations, .positions = *positions});
        return lastFrame;
    }

    AnimationBuilder& builder_;
    const GltfJoints& joints_;
    const GltfAnimationTarget& target_;
    std::size_t keysLeft_ = kMaxKeys;
};

const cgltf_animation* FindAnimation(const cgltf_data& data, const std::string& name) {
    const std::span animations(data.animations, data.animations_count);
    const auto found = std::ranges::find_if(animations, [&](const cgltf_animation& animation) {
        return animation.name != nullptr && ToUpperAscii(animation.name) == ToUpperAscii(name);
    });
    return found == animations.end() ? nullptr : &*found;
}

}

Result<AnimationAsset> ImportGltfAnimations(std::span<const std::byte> bytes,
                                            const GltfAnimationTarget& target) {
    Result<GltfData> data = ParseGlb(bytes);
    if (!data) {
        return std::unexpected(std::move(data.error()));
    }
    if ((*data)->animations_count == 0) {
        return Fail(ErrorCode::NotFound, "the glTF file has no animations");
    }
    if (target.joints.empty()) {
        return Fail(ErrorCode::InvalidData, "the animations have no skeleton to move");
    }
    const GltfJoints joints(**data, target.joints);
    AnimationAsset animations;
    AnimationBuilder builder(animations);
    PartImporter importer(builder, joints, target);
    const std::size_t jointCount = target.joints.size();
    for (const AnimationPart& part : target.original.parts) {
        if (part.firstTrack + jointCount > target.original.tracks.size()) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("part {} has fewer tracks than the skeleton has nodes", part.name));
        }
        AnimationPart copy = part;
        copy.firstTrack = static_cast<std::uint32_t>(animations.tracks.size());
        const cgltf_animation* animation = FindAnimation(**data, part.name);
        if (animation == nullptr) {
            for (std::size_t i = 0; i < jointCount; ++i) {
                builder.CopyTrack(target.original, target.original.tracks[part.firstTrack + i]);
            }
        } else {
            Result<std::int32_t> lastFrame = importer.AddTracks(*animation, part);
            if (!lastFrame) {
                return std::unexpected(std::move(lastFrame.error()));
            }
            if (*lastFrame > 0) {
                copy.frameCount = *lastFrame;
                copy.loopFrames = *lastFrame + 1;
            }
        }
        animations.parts.push_back(std::move(copy));
    }
    return animations;
}

}
