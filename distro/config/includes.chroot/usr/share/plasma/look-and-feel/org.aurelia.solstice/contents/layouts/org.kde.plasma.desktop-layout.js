// AURELIA OS first-login Plasma layout.
// The stops in this file are real Plasma widgets: app menu, system tray,
// digital clock, application launcher, icon tasks, and the AURELIA clock/calendar.

var plasma = getApiVersion(1);
var screen = { x: 0, y: 0, width: 1920, height: 1080 };
try {
    screen = screenGeometry(0);
} catch (error) {
    // Plasma 6 scripting always provides this, but keep a sane CI fallback.
    console.log("AURELIA layout: using 1920x1080 fallback");
}

// Top information bar.
var top = new Panel;
top.location = "top";
top.height = Math.round(screen.height * 0.032);
top.hiding = "none";
top.alignment = "left";
top.lengthMode = "fill";

// AURELIA application launcher at the left edge of the top bar. The same
// real Kickoff instance is available in the dock; this one gives the top bar
// the launcher affordance shown in the reference layout.
var topLauncher = top.addWidget("org.kde.plasma.kickoff");
topLauncher.currentConfigGroup = ["Configuration", "General"];
topLauncher.writeConfig("icon", "aurelia-logo");
topLauncher.writeConfig("favoritesDisplay", 0);

top.addWidget("org.kde.plasma.appmenu");

var topSpacer = top.addWidget("org.kde.plasma.panelspacer");
topSpacer.currentConfigGroup = ["Configuration", "General"];
topSpacer.writeConfig("expanding", true);

top.addWidget("org.kde.plasma.systemtray");
var topClock = top.addWidget("org.kde.plasma.digitalclock");
topClock.currentConfigGroup = ["Configuration", "Appearance"];
topClock.writeConfig("showDate", true);
topClock.writeConfig("showSeconds", false);

// Centered floating application dock.
var dock = new Panel;
dock.location = "bottom";
dock.height = Math.round(screen.height * 0.060);
dock.hiding = "none";
dock.alignment = "center";
dock.lengthMode = "fit";
dock.floating = true;

var launcher = dock.addWidget("org.kde.plasma.kickoff");
launcher.currentConfigGroup = ["Configuration", "General"];
launcher.writeConfig("icon", "aurelia-logo");
launcher.writeConfig("applicationsDisplay", 0);
launcher.writeConfig("favoritesDisplay", 0);
launcher.writeConfig("favorites", [
    "org.kde.dolphin.desktop",
    "org.kde.konsole.desktop",
    "org.kde.kate.desktop",
    "org.kde.gwenview.desktop",
    "org.kde.kcalc.desktop",
    "org.kde.ark.desktop",
    "systemsettings.desktop",
    "org.kde.discover.desktop",
    "novair.desktop",
    "aurelia-backup.desktop",
    "aurelia-migration.desktop",
    "org.kde.kclock.desktop",
    "org.kde.haruna.desktop",
    "org.kde.kamoso.desktop",
    "org.kde.korganizer.desktop",
    "org.kde.elisa.desktop",
    "org.kde.plasma-systemmonitor.desktop"
]);

var tasks = dock.addWidget("org.kde.plasma.icontasks");
tasks.currentConfigGroup = ["Configuration", "General"];
tasks.writeConfig("launchers", [
    "applications:org.kde.dolphin.desktop",
    "applications:org.kde.konsole.desktop",
    "applications:org.kde.kate.desktop",
    "applications:org.kde.gwenview.desktop",
    "applications:org.kde.kcalc.desktop",
    "applications:org.kde.ark.desktop",
    "applications:systemsettings.desktop",
    "applications:org.kde.discover.desktop",
    "applications:novair.desktop",
    "applications:org.kde.kclock.desktop",
    "applications:org.kde.haruna.desktop",
    "applications:org.kde.kamoso.desktop",
    "applications:org.kde.korganizer.desktop",
    "applications:org.kde.elisa.desktop",
    "applications:org.kde.plasma-systemmonitor.desktop"
]);
tasks.writeConfig("showOnlyMinimized", false);
tasks.writeConfig("indicateAudioStreams", false);

// Wallpaper and desktop composition.
var desktop = desktops()[0];
desktop.wallpaperPlugin = "org.kde.image";
desktop.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
desktop.writeConfig("Image", "file:///usr/share/wallpapers/AureliaSolstice/contents/images/3840x2160.png");

var calendarWidth = Math.max(260, Math.round(screen.width * 0.17));
var calendarHeight = Math.round(calendarWidth * 1.02);
desktop.addWidget("org.aurelia.calendar",
    Math.round(screen.width * 0.022),
    Math.round(screen.height * 0.13),
    calendarWidth,
    calendarHeight);

var clockWidth = Math.max(460, Math.round(screen.width * 0.42));
var clockHeight = Math.round(clockWidth * 0.45);
desktop.addWidget("org.aurelia.clock",
    Math.round((screen.width - clockWidth) / 2),
    Math.round(screen.height * 0.13),
    clockWidth,
    clockHeight);

// Media card. This is a real KDE widget: it stays empty until a media player
// is actually running, which is honest rather than a placeholder.
var mediaWidth = calendarWidth;
var mediaHeight = Math.max(92, Math.round(screen.height * 0.12));
desktop.addWidget("org.kde.plasma.mediacontroller",
    Math.round(screen.width * 0.022),
    Math.round(screen.height * 0.53),
    mediaWidth,
    mediaHeight);

// Deliberately no Weather, Notes or System Monitor card on a fresh desktop:
// Weather and System Monitor render as empty frames until the user picks a
// location or configures sensors, and the Notes sticky ships in its own yellow
// style. Shipping empty frames made the first-login desktop look unfinished.
// All three are still installed and can be added from the widget explorer.
