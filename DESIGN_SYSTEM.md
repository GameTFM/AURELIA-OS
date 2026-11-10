# AURELIA design system

AURELIA's visual language is called **Solstice**. Every surface — greeter, splash,
desktop, applets, installer, setup — reads its values from the same shipped files,
so there is one source of truth rather than per-app restyling.

## Sources of truth

| Token group | File |
|---|---|
| Colour scheme (window, view, button, selection, tooltip roles) | `/usr/share/color-schemes/Aurelia.colors` |
| Global theme, splash, layout, defaults | `/usr/share/plasma/look-and-feel/org.aurelia.solstice/` |
| Desktop composition (panels, dock, applets) | `.../contents/layouts/org.kde.plasma.desktop-layout.js` |
| Wallpaper collection | `/usr/share/wallpapers/AureliaSolstice/contents/images/` |
| Greeter | `/usr/share/sddm/themes/aurelia/` (`Main.qml`, `background.png`, `theme.conf`) |
| Boot splash | `/usr/share/plymouth/themes/aurelia/` and `aurelia-light/` |
| Display and UI type | `/usr/share/fonts/truetype/aurelia/PlayfairDisplay.ttf`, greeter font `Inter` |
| Iconography | `/usr/share/icons/hicolor/` plus the NOVAIR and AURELIA mark assets |

## Principles

1. **Calm contrast.** Dark, low-saturation surfaces; a single warm accent for
   selection and focus; text always meets contrast on its own surface rather than
   relying on inversion.
2. **One display face, one UI face.** Playfair Display is reserved for large
   expressive numerals and headings (clock, date, release name); Inter carries UI
   text, labels and controls.
3. **Depth by light, not by borders.** Panels and cards use surface elevation and
   a soft edge; avoid heavy outlines.
4. **Quiet motion.** The splash and greeter animate once; nothing loops in the
   desktop shell.
5. **Consistency across apps.** Applications use the KDE platform widgets so they
   inherit the same colour roles, spacing and focus rings instead of private
   styling. NOVAIR follows the same roles in Qt stylesheets.
6. **Explain, never fake.** Unavailable hardware or providers are shown as
   unavailable with the reason; no decorative switch or invented reading.

## Applet language

`org.aurelia.clock` and `org.aurelia.calendar` are Plasma QML applets that use the
theme's text and highlight roles, the display face for large numerals, and the
same 12-hour clock and locale-aware date formatting as the rest of the system.

The layout script places: top status bar (system tray, clock), left calendar
card, notes and media cards, and the floating bottom dock with pinned
applications. The KDE Welcome Center is not part of the design and is not
installed.

## Accessibility

Focus is visible on every interactive control, the greeter and applets use
theme-defined text roles (so contrast follows the selected scheme), and no
information is conveyed by colour alone.
