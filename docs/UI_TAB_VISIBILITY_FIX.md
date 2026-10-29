# UI tab visibility / paint artifact fix

The custom `AutoWrapTabWidget` implementation has been replaced with a small wrapper around Qt's native `QTabWidget` and `QTabBar`.

The previous implementation manually moved tab buttons between a flow layout and a scroll-area layout, then explicitly called `show()` / `hide()` while refreshing tabs. That made ownership and effective visibility harder to reason about and could leave packet-tab controls painted over unrelated top-level pages.

The wrapper preserves the methods used by the rest of the application (`addTab`, `count`, `widget`, `currentIndex`, `setCurrentIndex`, `setTabVisible`, `setTabText`, `setTabTexts`, and `setTabToolTip`) but delegates page parenting, clipping, selection, and repaint scheduling to Qt. Long tab labels use Qt's native scroll buttons and right elision, with full labels available through tooltips. The i18n update path skips the internal native tab widgets because the wrapper translates from each tab page's untranslated source property.

`tests/ui_smoke.cpp` now checks that subsystem and packet-tab widgets remain descendants of the subsystem page and are not effectively visible when Overview or SSDV is selected.
