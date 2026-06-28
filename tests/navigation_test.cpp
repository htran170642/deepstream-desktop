#include <QApplication>
#include <gtest/gtest.h>

#include "MainWindow.hpp"

namespace {

// Verifies the sidebar -> stacked-widget wiring end to end: selectPage() only
// touches the sidebar, currentPage() only reads the stack. If they agree, the
// navigation signal/slot connection works.
TEST(NavigationTest, SidebarSelectionDrivesStack) {
    dsd::MainWindow window;

    window.selectPage(dsd::Page::Dashboard);
    EXPECT_EQ(window.currentPage(), dsd::Page::Dashboard);

    window.selectPage(dsd::Page::Alerts);
    EXPECT_EQ(window.currentPage(), dsd::Page::Alerts);

    window.selectPage(dsd::Page::Settings);
    EXPECT_EQ(window.currentPage(), dsd::Page::Settings);
}

}  // namespace

// Qt widgets require a QApplication instance, so this test owns main() instead
// of linking gtest_main.
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
