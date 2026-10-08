; Nexus InnoSetup Script
; Requires Inno Setup 6

#define MyAppName "Nexus"
#ifndef MyAppVersion
  #define MyAppVersion "1.0.10"
#endif
#define MyAppPublisher "jelllove"
#define MyAppURL "https://www.jelllove.com"
#define MyAppExeName "Nexus.exe"
#ifndef MyAppExePath
  #define MyAppExePath MyAppExeName
#endif
#ifndef MyAppSourceDir
  #define MyAppSourceDir "..\dist"
#endif

[Setup]
AppId={{B1F2A3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL=https://github.com/jelllove/Nexus/issues
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=Output
OutputBaseFilename=Nexus-Setup-v{#MyAppVersion}-x64
SetupIconFile=..\resources\icons\app-icon.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExePath}
CloseApplications=yes
CloseApplicationsFilter=Nexus.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#MyAppSourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExePath}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExePath}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExePath}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
