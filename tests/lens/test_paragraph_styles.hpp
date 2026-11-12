#pragma once

#include "lens/paragraph_styles.hpp"
#include "lens/json_helper.hpp"
#include "test.hpp"

namespace gdocsmith::lens::test {

TEST_CASE("paragraphDeclarationPatch and Export round trip") {
    ParagraphDeclaration decl;
    decl.direction = "RIGHT_TO_LEFT";
    decl.keepWithNext = true;
    decl.pageBreakBefore = true;
    decl.spaceAbove = 14.0;
    decl.spaceBelow = 6.0;

    ParagraphBorder border;
    border.color = "#1967D2";
    border.dashStyle = "DASH";
    border.padding = 4.0;
    border.width = 1.5;
    decl.borderTop = border;

    slimr::Json patch = paragraphDeclarationPatch(decl);
    CHECK(patch["direction"].get<std::string>() == "RIGHT_TO_LEFT");
    CHECK(patch["keepWithNext"].get<bool>() == true);
    CHECK(patch["pageBreakBefore"].get<bool>() == true);
    CHECK(dimensionPt(patch["spaceAbove"]) == 14.0);
    CHECK(dimensionPt(patch["spaceBelow"]) == 6.0);

    const auto& bt = patch["borderTop"];
    CHECK(bt.is_object());
    CHECK(bt["dashStyle"].get<std::string>() == "DASH");
    CHECK(dimensionPt(bt["padding"]) == 4.0);
    CHECK(dimensionPt(bt["width"]) == 1.5);
    CHECK(colorHexFromOptional(bt["color"]) == std::optional<std::string>("#1967D2"));

    auto exported = paragraphDeclarationExport(patch);
    CHECK(exported.direction == std::optional<std::string>("RIGHT_TO_LEFT"));
    CHECK(exported.keepWithNext == std::optional<bool>(true));
    CHECK(exported.pageBreakBefore == std::optional<bool>(true));
    CHECK(exported.spaceAbove == std::optional<double>(14.0));
    CHECK(exported.spaceBelow == std::optional<double>(6.0));
    CHECK(exported.borderTop.has_value());
    CHECK(exported.borderTop->color == std::optional<std::string>("#1967D2"));
    CHECK(exported.borderTop->dashStyle == "DASH");
    CHECK(exported.borderTop->padding == 4.0);
    CHECK(exported.borderTop->width == 1.5);
}

TEST_CASE("echoed defaults export empty") {
    slimr::Json echoed = slimr::json_obj();
    echoed["avoidWidowAndOrphan"] = true;
    echoed["direction"] = "LEFT_TO_RIGHT";
    echoed["keepLinesTogether"] = false;
    echoed["keepWithNext"] = false;
    echoed["pageBreakBefore"] = false;

    slimr::Json inherited = slimr::json_obj();
    inherited["avoidWidowAndOrphan"] = true;

    auto exported = paragraphDeclarationExport(echoed, false, inherited);
    CHECK(exported.empty());
}

} // namespace gdocsmith::lens::test
