#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rr::test {

struct JamFile {
    std::string path;
    std::string contents;
};

inline constexpr std::size_t kJamRootFilesOffset = 8;
inline constexpr std::size_t kJamFileRecordBytes = 20;
inline constexpr std::size_t kJamRecordValueOffset = 12;

class JamBuilder {
public:
    [[nodiscard]] static std::vector<std::byte> Build(
        const std::vector<JamFile>& files, const std::vector<std::string>& directories = {}) {
        JamBuilder builder;
        for (const JamFile& file : files) {
            builder.AddFile(file);
        }
        for (const std::string& directory : directories) {
            builder.AddDirectories(directory);
        }
        return builder.Serialize();
    }

private:
    struct Node {
        std::string name;
        std::vector<JamFile> files;
        std::vector<std::size_t> children;
    };

    JamBuilder() : nodes_(1) {}

    std::size_t AddDirectories(std::string_view path) {
        std::size_t node = 0;
        while (!path.empty()) {
            const std::size_t end = std::min(path.find('/'), path.size());
            node = Child(node, path.substr(0, end));
            path.remove_prefix(std::min(end + 1, path.size()));
        }
        return node;
    }

    void AddFile(const JamFile& file) {
        const std::size_t separator = file.path.rfind('/');
        const bool isInRoot = separator == std::string::npos;
        const std::size_t node = isInRoot ? 0 : AddDirectories(file.path.substr(0, separator));
        const std::string name = isInRoot ? file.path : file.path.substr(separator + 1);
        nodes_[node].files.push_back({.path = name, .contents = file.contents});
    }

    std::size_t Child(std::size_t parent, std::string_view name) {
        for (const std::size_t child : nodes_[parent].children) {
            if (nodes_[child].name == name) {
                return child;
            }
        }
        nodes_.push_back({.name = std::string(name), .files = {}, .children = {}});
        nodes_[parent].children.push_back(nodes_.size() - 1);
        return nodes_.size() - 1;
    }

    std::vector<std::byte> Serialize() {
        std::vector<std::size_t> offsets;
        std::size_t offset = 4;
        for (const Node& node : nodes_) {
            offsets.push_back(offset);
            offset += 8 + (kJamFileRecordBytes * node.files.size()) + (16 * node.children.size());
        }
        dataStart_ = offset;
        AppendText("LJAM");
        for (const Node& node : nodes_) {
            WriteNode(node, offsets);
        }
        bytes_.insert(bytes_.end(), data_.begin(), data_.end());
        return bytes_;
    }

    void WriteNode(const Node& node, const std::vector<std::size_t>& offsets) {
        AppendU32(node.files.size());
        for (const JamFile& file : node.files) {
            AppendName(file.path);
            AppendU32(dataStart_ + data_.size());
            AppendU32(file.contents.size());
            for (const char c : file.contents) {
                data_.push_back(static_cast<std::byte>(c));
            }
        }
        AppendU32(node.children.size());
        for (const std::size_t child : node.children) {
            AppendName(nodes_[child].name);
            AppendU32(offsets[child]);
        }
    }

    void AppendText(std::string_view text) {
        for (const char c : text) {
            bytes_.push_back(static_cast<std::byte>(c));
        }
    }

    void AppendName(std::string_view name) {
        AppendText(name);
        bytes_.resize(bytes_.size() + 12 - name.size());
    }

    void AppendU32(std::size_t value) {
        for (std::size_t i = 0; i < 4; ++i) {
            bytes_.push_back(static_cast<std::byte>((value >> (8U * i)) & 0xFFU));
        }
    }

    std::vector<Node> nodes_;
    std::size_t dataStart_ = 0;
    std::vector<std::byte> bytes_;
    std::vector<std::byte> data_;
};

inline void PatchU32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
}

[[nodiscard]] inline std::string_view AsText(std::span<const std::byte> bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}
