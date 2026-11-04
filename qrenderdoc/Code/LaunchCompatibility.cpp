// The MIT License (MIT). Copyright (c) 2026 RenderDoc contributors.
#include "LaunchCompatibility.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include "Code/QRDUtils.h"

namespace
{
QString Text(const char *text)
{
  return QCoreApplication::translate("LaunchCompatibility", text);
}

bool AddDefaultEnvironment(CaptureSettings &settings, const char *name, const char *value)
{
  for(const EnvironmentModification &env : settings.environment)
    if(QString::fromUtf8(env.name.c_str()).compare(QString::fromUtf8(name), Qt::CaseInsensitive) == 0)
      return false;
  settings.environment.push_back(EnvironmentModification(EnvMod::Set, EnvSep::NoSep, name, value));
  return true;
}
}

LaunchCompatibilityResult ResolveLaunchCompatibility(const CaptureSettings &input,
                                                     bool localWindowsLaunch)
{
  LaunchCompatibilityResult result;
  result.settings = input;
  result.profile = lit("standard");
  if(!localWindowsLaunch || input.inject)
  {
    result.summary = Text(
        "Compatibility profiles apply only to local Windows Launch, not injection or remote "
        "capture.");
    return result;
  }

  QString selection = input.compatibilityProfile;
  if(selection != lit("auto") && selection != lit("standard") && selection != lit("mihoyo-d3d11") &&
     selection != lit("returnal-d3d12"))
  {
    result.summary =
        Text("Unknown compatibility profile '%1': using manual settings only.").arg(selection);
    return result;
  }
  result.summary = Text("Standard launch: using manual settings only.");
  if(selection == lit("standard"))
    return result;

  QString executable = QString(input.executable).trimmed();
  if(executable.startsWith(QLatin1Char('"')) && executable.endsWith(QLatin1Char('"')))
    executable = executable.mid(1, executable.size() - 2);
  QFileInfo file(executable);
  const QString basename = file.fileName();
  QString game, dataDirectory;
  if(basename.compare(lit("YuanShen.exe"), Qt::CaseInsensitive) == 0)
  {
    game = Text("Genshin Impact");
    dataDirectory = lit("YuanShen_Data");
  }
  else if(basename.compare(lit("StarRail.exe"), Qt::CaseInsensitive) == 0)
  {
    game = Text("Honkai: Star Rail");
    dataDirectory = lit("StarRail_Data");
  }
  else if(basename.compare(lit("ZenlessZoneZero.exe"), Qt::CaseInsensitive) == 0)
  {
    game = Text("Zenless Zone Zero");
    dataDirectory = lit("ZenlessZoneZero_Data");
  }

  const bool mihoyo =
      !game.isEmpty() && file.isFile() && QFileInfo(file.dir().filePath(dataDirectory)).isDir();
  const bool returnalRoot = basename.compare(lit("Returnal.exe"), Qt::CaseInsensitive) == 0;
  const bool returnalShipping =
      basename.compare(lit("Returnal-Win64-Shipping.exe"), Qt::CaseInsensitive) == 0;
  if(selection == lit("auto"))
  {
    if(mihoyo)
      selection = lit("mihoyo-d3d11");
    else if(returnalRoot || returnalShipping)
      selection = lit("returnal-d3d12");
    else
      return result;
  }

  QStringList summary;
  if(selection == lit("mihoyo-d3d11"))
  {
    if(!mihoyo)
    {
      result.error = Text(
          "Select YuanShen.exe, StarRail.exe or ZenlessZoneZero.exe with its matching _Data "
          "directory.");
      result.summary = result.error;
      return result;
    }
    result.profile = selection;
    summary << Text("MiHoYo D3D11 compatibility: %1").arg(game);
    const char *names[] = {"RENDERDOC_DXGI_HOOK_FACTORY0", "RENDERDOC_D3D11_CAPTURE_OPTOUT"};
    const char *values[] = {"0", "1"};
    for(int i = 0; i < 2; ++i)
    {
      if(AddDefaultEnvironment(result.settings, names[i], values[i]))
        summary << Text("Added %1=%2 for this launch.")
                       .arg(QString::fromUtf8(names[i]), QString::fromUtf8(values[i]));
      else
        summary << Text("%1: keeping your manual environment setting.").arg(QString::fromUtf8(names[i]));
    }
  }
  else
  {
    QDir root = file.dir();
    if(returnalShipping)
      for(int i = 0; i < 3; ++i)
        root.cdUp();
    const QString shipping =
        root.filePath(lit("Returnal/Binaries/Win64/Returnal-Win64-Shipping.exe"));
    if((!returnalRoot && !returnalShipping) || !file.isFile() ||
       !QFileInfo(root.filePath(lit("Returnal.exe"))).isFile() || !QFileInfo(shipping).isFile() ||
       (returnalShipping && QFileInfo(shipping).canonicalFilePath().compare(
                                file.canonicalFilePath(), Qt::CaseInsensitive) != 0))
    {
      result.error = Text(
          "Returnal installation is incomplete. Select its root Returnal.exe or "
          "Returnal/Binaries/Win64/Returnal-Win64-Shipping.exe.");
      result.summary = result.error;
      return result;
    }
    result.profile = selection;
    result.settings.executable = QDir::toNativeSeparators(shipping);
    if(QString(input.workingDir).trimmed().isEmpty())
      result.settings.workingDir = QDir::toNativeSeparators(root.absolutePath());
    const QRegularExpression prefix(lit("^\\s*(?:\"Returnal\"|Returnal)(?:\\s|$)"),
                                    QRegularExpression::CaseInsensitiveOption);
    QString arguments = input.commandLine;
    if(!prefix.match(arguments).hasMatch())
      result.settings.commandLine =
          arguments.trimmed().isEmpty() ? lit("Returnal") : lit("Returnal ") + arguments;
    result.settings.options.hookIntoChildren = true;
    summary << Text("Returnal D3D12 compatibility: root launcher resolved to the game executable.")
            << Text("Working directory: %1").arg(QString(result.settings.workingDir))
            << Text("Returnal argument ensured; child process capture enabled.")
            << Text(
                   "For complete scene replay, disable DLSS in the game's graphics settings. Game "
                   "settings are not changed automatically.");
  }
  if(QString(result.settings.workingDir).trimmed().isEmpty())
    result.settings.workingDir = QDir::toNativeSeparators(file.absolutePath());
  summary << Text("Launch executable: %1").arg(QString(result.settings.executable));
  result.summary = summary.join(QLatin1Char('\n'));
  return result;
}

#if ENABLE_UNIT_TESTS
#include <QFile>
#include <QTemporaryDir>
#include "3rdparty/catch/catch.hpp"

TEST_CASE("Launch compatibility selection and isolation", "[launch-compatibility]")
{
  QTemporaryDir temporary;
  REQUIRE(temporary.isValid());
  REQUIRE(QDir(temporary.path()).mkpath(lit("games with spaces")));
  QDir root(QDir(temporary.path()).filePath(lit("games with spaces")));
  auto touch = [&root](const QString &name) {
    QFile file(root.filePath(name));
    REQUIRE(file.open(QIODevice::WriteOnly));
  };
  const char *executables[] = {"YuanShen", "StarRail", "ZenlessZoneZero"};
  for(const char *name : executables)
  {
    const QString stem = QString::fromUtf8(name);
    REQUIRE(root.mkpath(stem + lit("_Data")));
    touch(stem + lit(".exe"));
    CaptureSettings raw;
    raw.executable = root.filePath(stem + lit(".exe"));
    LaunchCompatibilityResult resolved = ResolveLaunchCompatibility(raw, true);
    CHECK(bool(resolved.profile == lit("mihoyo-d3d11")));
    CHECK(resolved.error.isEmpty());
    REQUIRE(resolved.settings.environment.size() == 2);
    CHECK(raw.environment.empty());
    CHECK(resolved.settings.environment[0].value == "0");
    CHECK(resolved.settings.environment[1].value == "1");
    raw.environment.push_back(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "renderdoc_dxgi_hook_factory0", "1"));
    resolved = ResolveLaunchCompatibility(raw, true);
    CHECK(resolved.settings.environment.size() == 2);
    CHECK(resolved.settings.environment[0].value == "1");
    raw.compatibilityProfile = "standard";
    CHECK(ResolveLaunchCompatibility(raw, true).settings.environment.size() == 1);
    raw.compatibilityProfile = "auto";
    CHECK(ResolveLaunchCompatibility(raw, false).settings.environment.size() == 1);
    raw.inject = true;
    CHECK(ResolveLaunchCompatibility(raw, true).settings.environment.size() == 1);
  }
  touch(lit("YUANSHEN.EXE"));
  CaptureSettings switching;
  switching.executable = root.filePath(lit("YUANSHEN.EXE"));
  CHECK(bool(ResolveLaunchCompatibility(switching, true).profile == lit("mihoyo-d3d11")));
  switching.executable = root.filePath(lit("YuanShen.exe.bak"));
  CHECK(bool(ResolveLaunchCompatibility(switching, true).profile == lit("standard")));
  switching.executable = root.filePath(lit("OtherYuanShen.exe"));
  CHECK(ResolveLaunchCompatibility(switching, true).settings.environment.empty());
  switching.compatibilityProfile = "mihoyo-d3d11";
  CHECK_FALSE(ResolveLaunchCompatibility(switching, true).error.isEmpty());
  REQUIRE(root.mkpath(lit("broken")));
  touch(lit("broken/YuanShen.exe"));
  switching.executable = root.filePath(lit("broken/YuanShen.exe"));
  switching.compatibilityProfile = "auto";
  CHECK(bool(ResolveLaunchCompatibility(switching, true).profile == lit("standard")));
  REQUIRE(root.mkpath(lit("Returnal/Binaries/Win64")));
  touch(lit("Returnal.exe"));
  touch(lit("Returnal/Binaries/Win64/Returnal-Win64-Shipping.exe"));
  CaptureSettings raw;
  raw.executable = root.filePath(lit("Returnal.exe"));
  raw.commandLine = "-windowed";
  LaunchCompatibilityResult resolved = ResolveLaunchCompatibility(raw, true);
  CHECK(resolved.error.isEmpty());
  CHECK(bool(resolved.profile == lit("returnal-d3d12")));
  CHECK(resolved.settings.commandLine == "Returnal -windowed");
  CHECK(resolved.settings.options.hookIntoChildren);
  CHECK(resolved.settings.environment.empty());
  CHECK(raw.commandLine == "-windowed");
  switching.executable = raw.executable;
  CHECK(ResolveLaunchCompatibility(switching, true).settings.environment.empty());
  raw.executable = resolved.settings.executable;
  raw.commandLine = "\"Returnal\" -windowed";
  raw.workingDir = temporary.path();
  resolved = ResolveLaunchCompatibility(raw, true);
  CHECK(resolved.settings.commandLine == raw.commandLine);
  CHECK(resolved.settings.workingDir == raw.workingDir);
  touch(lit("broken/Returnal.exe"));
  raw.executable = root.filePath(lit("broken/Returnal.exe"));
  CHECK_FALSE(ResolveLaunchCompatibility(raw, true).error.isEmpty());
  raw.executable = root.filePath(lit("OtherReturnal.exe"));
  CHECK(bool(ResolveLaunchCompatibility(raw, true).profile == lit("standard")));
  raw.compatibilityProfile = "returnal-d3d12";
  CHECK_FALSE(ResolveLaunchCompatibility(raw, true).error.isEmpty());
  raw.compatibilityProfile = "future-version";
  CHECK(bool(ResolveLaunchCompatibility(raw, true).profile == lit("standard")));
  CHECK(ResolveLaunchCompatibility(raw, true).summary.contains(lit("Unknown")));

  QVariantMap saved = QVariant(CaptureSettings()).toMap();
  CHECK(CaptureSettings(saved).compatibilityProfile == "auto");
  saved.remove(lit("compatibilityProfile"));
  CHECK(CaptureSettings(saved).compatibilityProfile == "auto");
  raw.compatibilityProfile = "standard";
  CHECK(CaptureSettings(QVariant(raw)).compatibilityProfile == "standard");
  raw.compatibilityProfile = "future-version";
  CHECK(CaptureSettings(QVariant(raw)).compatibilityProfile == "future-version");
}
#endif
