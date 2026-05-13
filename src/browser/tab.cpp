#include "browser/tab.h"

namespace nebula::browser {

bool NebulaTab::CanGoBack() const {
    return browser && browser->CanGoBack();
}

bool NebulaTab::CanGoForward() const {
    return browser && browser->CanGoForward();
}

}  // namespace nebula::browser
