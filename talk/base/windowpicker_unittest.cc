#include "talk/base/gunit.h"
#include "talk/base/window.h"
#include "talk/base/windowpicker.h"
#include "talk/base/windowpickerfactory.h"

#ifdef OSX
#  define DISABLE_ON_MAC(name) DISABLED_ ## name
#else
#  define DISABLE_ON_MAC(name) name
#endif

TEST(WindowPickerTest, GetWindowList) {
  if (!talk_base::WindowPickerFactory::IsSupported()) {
    LOG(LS_INFO) << "skipping test: window capturing is not supported with "
                 << "current configuration.";
  }
  talk_base::scoped_ptr<talk_base::WindowPicker> picker(
      talk_base::WindowPickerFactory::CreateWindowPicker());
  EXPECT_TRUE(picker->Init());
  talk_base::WindowDescriptionList descriptions;
  EXPECT_TRUE(picker->GetWindowList(&descriptions));
}

// TODO(hughv) Investigate why this fails on pulse but not locally after
// upgrading to XCode 4.5.  The failure is GetDesktopList returning FALSE.
TEST(WindowPickerTest, DISABLE_ON_MAC(GetDesktopList)) {
  if (!talk_base::WindowPickerFactory::IsSupported()) {
    LOG(LS_INFO) << "skipping test: window capturing is not supported with "
                 << "current configuration.";
  }
  talk_base::scoped_ptr<talk_base::WindowPicker> picker(
      talk_base::WindowPickerFactory::CreateWindowPicker());
  EXPECT_TRUE(picker->Init());
  talk_base::DesktopDescriptionList descriptions;
  EXPECT_TRUE(picker->GetDesktopList(&descriptions));
  if (descriptions.size() > 0) {
    int width = 0;
    int height = 0;
    EXPECT_TRUE(picker->GetDesktopDimensions(descriptions[0].id(), &width,
                                             &height));
    EXPECT_GT(width, 0);
    EXPECT_GT(height, 0);

    // Test |IsPrimaryDesktop|. Only one desktop should be a primary.
    bool found_primary = false;
    for (talk_base::DesktopDescriptionList::iterator it = descriptions.begin();
         it != descriptions.end(); ++it) {
      if (it->primary()) {
        EXPECT_FALSE(found_primary);
        found_primary = true;
      }
    }
    EXPECT_TRUE(found_primary);
  }
}
