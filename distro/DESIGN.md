# AURELIA Solstice — visual system

The three reference images are compositional targets, not assets to copy. The
result is an original AURELIA scene and layout built from KDE Plasma surfaces.

## Palette

| Token | Hex | Use |
|---|---|---|
| Midnight | `#090A0E` | Desktop, greeter, installer, boot |
| Champagne Gold | `#D6B878` | Accent, active state, small rules |
| Ivory | `#F4F0E7` | Primary type |
| Graphite | `#202127` | Cards and lifted surfaces |
| Warm Gray | `#85838A` | Muted type, metadata |
| Deep Ridge | `#10131D` | Wallpaper mountain layers |
| Pine | `#030509` | Foreground silhouettes |

The light wallpaper is the Ivory variant, not a second default theme: the
AURELIA global theme is intentionally midnight-first.

## Typography

- **Playfair Display** — large clock, weekday, calendar day. Editorial serif
  contrast against the technical UI. Bundled under OFL 1.1.
- **Inter** — panel, menu, card labels, dates, SDDM controls. Installed from
  Ubuntu's `fonts-inter`.
- Very wide tracking is used only for small all-caps labels (`SOLSTICE`,
  `CALENDAR`); the clock has restrained tracking so it remains readable.

## Composition mapping

| Reference element | AURELIA implementation |
|---|---|
| Thin top bar | Plasma top panel ~3.2% of screen height with Global Menu, spacer, system tray, digital clock |
| Giant weekday/time | Custom `org.aurelia.clock`, centered near the top, live system date/time, Playfair |
| Left desktop widgets | Custom month calendar at left; KDE Media Controller below it |
| Right desktop widgets | KDE Notes card at right |
| Large Start menu / pinned grid | Kickoff in grid mode with real pinned applications; built-in power/session controls |
| Clean centered taskbar | Floating bottom panel in `fit` length mode with Icon Tasks; no window labels |
| Power button | Kickoff and the system tray's real power/session UI |
| Scenic wallpaper | Original layered 4K mountain scene with pines, stars and a low gold sun |
| Installer/sign-in/boot consistency | AURELIA SDDM greeter, Calamares branding, GRUB theme and 4K backgrounds |

The references contain weather, reminders and synthetic system cards. Those are
**not fabricated** here. The Notes applet accepts real notes; Media Controller
shows real MPRIS media; system telemetry is available through the installed
System Monitor application rather than a fake card.

## Geometry

The layout script uses the screen geometry and round percentages:

| Surface | Geometry |
|---|---|
| Top panel | top, 100% width, ~3.2% height, `fill` |
| Bottom dock | bottom, centered, `fit`, floating, ~6% height |
| Clock | centered, ~42% screen width, ~45% of its own width tall, y ≈ 13% |
| Calendar | left ~2.2%, y ≈ 13%, width ~17% |
| Notes | right ~2.2%, y ≈ 13%, width ~15%, height ~28% |
| Media | left ~2.2%, y ≈ 53%, width ~17%, height ~12% |

At 1920×1080 the top bar is 35 px, the dock 65 px, the clock about 806 px wide
and the calendar about 326×333 px. At other sizes the same percentages produce a
proportional layout; the minimum widths prevent unusable cards on smaller
logical screens.

## Surfaces and interactions

- **Clock**: real local time and locale-driven weekday/month, updated every
  second.
- **Calendar**: real current month, real leap years, previous/next month
  controls, today highlight only in the current month.
- **Notes**: KDE's persistent notes applet; editable by the user.
- **Media**: KDE's MPRIS media controller; functional when Elisa/NOVAIR/etc.
  exposes a player. Empty state is honest rather than simulated.
- **Dock**: Kickoff, Icon Tasks, Breeze icons; pinned applications are real
  desktop files.
- **Installer**: live-only launcher; removed from the installed root.
- **First login**: one-shot look-and-feel application to avoid overwriting later
  user changes.

## What is deliberately not done

- No copy of Windows assets, icons, fonts or wallpaper.
- No fake data in clock, calendar, notes, media, network, battery or system
  status.
- No custom window manager, compositor or display server: KWin Wayland and
  Plasma are the actual desktop.
- No third-party global theme dependency; the color scheme, layout, applets,
  greeter and artwork are shipped in this tree.
