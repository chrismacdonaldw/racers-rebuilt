#include "app/menu/licence_menu.hpp"

#include <doctest/doctest.h>

#include <string>

#include "app/menu/name_field.hpp"
#include "sim/race/cheats.hpp"

namespace rr::app {
namespace {

sim::InputFrame Pressed(sim::MenuAction action) {
    sim::InputFrame input;
    input.menuPressed.set(sim::InputFrame::Index(action));
    return input;
}

NameField Field() {
    NameField field(4);
    field.Allow(u" ABC");
    return field;
}

}

TEST_CASE("the licence opens on the name and wraps round its four rows") {
    LicenceMenu menu;
    CHECK(menu.Focus() == LicenceRow::Name);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Up)) == LicenceRequest::None);
    CHECK(menu.Focus() == LicenceRow::Back);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Down)) == LicenceRequest::None);
    CHECK(menu.Focus() == LicenceRow::Name);
}

TEST_CASE("the licence's buttons ask for their requests and confirming the name asks nothing") {
    LicenceMenu menu;
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == LicenceRequest::None);
    static_cast<void>(menu.Tick(Pressed(sim::MenuAction::Down)));
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == LicenceRequest::Expression);
    static_cast<void>(menu.Tick(Pressed(sim::MenuAction::Down)));
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == LicenceRequest::Done);
    static_cast<void>(menu.Tick(Pressed(sim::MenuAction::Down)));
    CHECK(menu.Tick(Pressed(sim::MenuAction::Confirm)) == LicenceRequest::Leave);
    CHECK(menu.Tick(Pressed(sim::MenuAction::Back)) == LicenceRequest::Leave);
}

TEST_CASE("a name takes only the font's characters, up to its length") {
    NameField field = Field();
    CHECK(field.Type(u'A') == NameEdit::Typed);
    CHECK(field.Type(u'Z') == NameEdit::Refused);
    CHECK(field.Type(u' ') == NameEdit::Typed);
    CHECK(field.Type(u'B') == NameEdit::Typed);
    CHECK(field.Type(u'C') == NameEdit::Typed);
    CHECK(field.IsFull());
    CHECK(field.Type(u'A') == NameEdit::Refused);
    CHECK(field.Text() == u"A BC");
}

TEST_CASE("erasing a name stops at empty") {
    NameField field = Field();
    field.SetText(u"ABCABC");
    CHECK(field.Text() == u"ABCA");
    for (int erase = 0; erase < 4; ++erase) {
        CHECK(field.Erase() == NameEdit::Erased);
    }
    CHECK(field.Erase() == NameEdit::Refused);
    CHECK(field.Text().empty());
}

TEST_CASE("typed keys turn upper case as the original's do, missing a grave") {
    CHECK(LicenceUpper(u'a') == u'A');
    CHECK(LicenceUpper(u'z') == u'Z');
    CHECK(LicenceUpper(u'1') == u'1');
    CHECK(LicenceUpper(u'\u00E4') == u'\u00C4');
    CHECK(LicenceUpper(u'\u00E0') == u'\u00E0');
}

TEST_CASE("a licence name toggles the cheat its plate names") {
    sim::Cheats cheats;
    CHECK(cheats.EnterPlate(PlateText(u"FSTFRWRD")));
    CHECK(cheats.Has(sim::Cheat::FastForward));
    CHECK_FALSE(cheats.EnterPlate(PlateText(u"PLAYER")));
    CHECK(cheats.EnterPlate(PlateText(u"NMRCHTS")));
    CHECK(cheats.IsEmpty());
}

}
