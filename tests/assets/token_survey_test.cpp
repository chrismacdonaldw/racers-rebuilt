#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/jam_archive.hpp"
#include "assets/token_reader.hpp"
#include "game_files.hpp"

namespace rr::assets {
namespace {

constexpr std::array<std::string_view, 38> kTokenizedExtensions{
    "ADB", "BDB", "BVB", "CCB", "CDB", "CEB", "CMB", "CPB", "CRB", "DDB", "EMB", "EVB", "FDB",
    "GCB", "GDB", "GHB", "HZB", "IDB", "LEB", "LSB", "MAB", "MDB", "MIB", "MSB", "PCB", "PWB",
    "RAB", "RCB", "RRB", "SDB", "SKB", "SPB", "TDB", "TGB", "TIB", "TMB", "TRB", "WDB",
};
constexpr std::size_t kRetailTokenizedFileCount = 2819;

bool IsTokenized(std::string_view name) {
    return std::ranges::contains(kTokenizedExtensions, test::Extension(name));
}

Result<void> TokenizeToEnd(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    int depth = 0;
    while (true) {
        Result<Token> token = reader.Next();
        if (!token) {
            return std::unexpected(std::move(token.error()));
        }
        switch (token->kind) {
            case TokenKind::OpenBrace:
            case TokenKind::OpenBracket:
                ++depth;
                break;
            case TokenKind::CloseBrace:
            case TokenKind::CloseBracket:
                --depth;
                break;
            case TokenKind::End:
                if (depth != 0) {
                    return Fail(ErrorCode::InvalidData, "unbalanced delimiters");
                }
                return {};
            default:
                break;
        }
        if (depth < 0) {
            return Fail(ErrorCode::InvalidData,
                        std::format("unopened delimiter at offset {}", token->offset));
        }
    }
}

TEST_CASE("every tokenized file in the retail LEGO.JAM decodes to the end") {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    const Result<JamArchive> archive = JamArchive::Open(*gameFolder / "LEGO.JAM");
    REQUIRE(archive.has_value());

    const test::Survey survey = test::SurveyLayer(*archive, IsTokenized, TokenizeToEnd);

    CHECK(survey.fileCount == kRetailTokenizedFileCount);
    for (const std::string& failure : survey.failures) {
        FAIL_CHECK(failure);
    }
}

}
}
