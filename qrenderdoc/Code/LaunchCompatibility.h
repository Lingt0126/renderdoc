// The MIT License (MIT). Copyright (c) 2026 RenderDoc contributors.
#pragma once

#include <QString>
#include "Code/Interface/QRDInterface.h"

struct LaunchCompatibilityResult
{
  CaptureSettings settings;
  QString profile;
  QString summary;
  QString error;
};

// Resolve a copy. Never modify the dialog's manual settings, host environment or game files.
LaunchCompatibilityResult ResolveLaunchCompatibility(const CaptureSettings &settings,
                                                     bool localWindowsLaunch);
