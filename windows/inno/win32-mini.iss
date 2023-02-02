#include "version.iss"

[Setup]
AppName=JACK2
AppPublisher=jackaudio.org
AppPublisherURL=https://github.com/jackaudio/jack2/
AppSupportURL=https://github.com/jackaudio/jack2/issues/
AppUpdatesURL=https://github.com/jackaudio/jack2-releases/releases/
AppVersion={#VERSION}
DefaultDirName={commonpf32}\JACK2
DisableDirPage=yes
DisableWelcomePage=no
LicenseFile=..\..\COPYING
OutputBaseFilename=jack2-win32-{#VERSION}
OutputDir=.
UsePreviousAppDir=no

[Types]
Name: "full"; Description: "Full installation";
Name: "custom"; Description: "Custom installation"; Flags: iscustom;

[Components]
Name: jackserver; Description: "JACK Server"; Types: full custom; Flags: fixed;
Name: dev; Description: "Developer resources"; Types: full;

[Files]
; icon
Source: "jack.ico"; DestDir: "{app}"; Components: jackserver; Flags: ignoreversion;
; jackd and server libs
Source: "win32\bin\jackd.exe"; DestDir: "{app}"; Components: jackserver; Flags: ignoreversion;
Source: "win32\lib\libjacknet.dll"; DestDir: "{app}"; Components: jackserver; Flags: ignoreversion;
Source: "win32\lib\libjackserver.dll"; DestDir: "{app}"; Components: jackserver; Flags: ignoreversion;
; drivers
Source: "win32\lib\jack\*.dll"; DestDir: "{app}\jack"; Components: jackserver; Flags: ignoreversion;
; jack client lib (NOTE goes into windir)
Source: "win32\lib\libjack.dll"; DestDir: "{win}"; Components: jackserver; Flags: ignoreversion;
; dev
Source: "win32\include\jack\*.h"; DestDir: "{app}\include\jack"; Components: dev; Flags: ignoreversion;
Source: "win32\lib\*.a"; DestDir: "{app}\lib"; Components: dev; Flags: ignoreversion;
Source: "win32\lib\*.def"; DestDir: "{app}\lib"; Components: dev; Flags: ignoreversion;
Source: "win32\lib\*.lib"; DestDir: "{app}\lib"; Components: dev; Flags: ignoreversion;
Source: "win32\lib\jack\*.a"; DestDir: "{app}\lib\jack"; Components: dev; Flags: ignoreversion;

[Registry]
Root: HKLM; Subkey: "Software\JACK"; Flags: deletevalue uninsdeletekeyifempty uninsdeletevalue; ValueType: string; ValueName: "ServerExecutable"; ValueData: "{app}\jackd.exe"
Root: HKLM; Subkey: "Software\JACK"; Flags: deletevalue uninsdeletekeyifempty uninsdeletevalue; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"
Root: HKLM; Subkey: "Software\JACK"; Flags: deletevalue uninsdeletekeyifempty uninsdeletevalue; ValueType: string; ValueName: "Version"; ValueData: "{#VERSION}"
