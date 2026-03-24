[Setup]
AppId={{02621F03-7F92-4E33-AA17-017F79B31DCF}
AppName=uxplay-windows
AppVersion=1.73.0
AppPublisher=leapbtw
DefaultDirName={autopf}\uxplay-windows
DisableProgramGroupPage=yes
OutputBaseFilename=uxplay-windows
SetupIconFile=icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "dist\uxplay-windows\uxplay-windows.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\uxplay-windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\uxplay-windows"; Filename: "{app}\uxplay-windows.exe"
Name: "{autodesktop}\uxplay-windows"; Filename: "{app}\uxplay-windows.exe"; Tasks: desktopicon

[Run]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""uxplay-windows"" dir=in action=allow program=""{app}\_internal\bin\uxplay.exe"" enable=yes"; Flags: runhidden
Filename: "{app}\uxplay-windows.exe"; Description: "{cm:LaunchProgram,uxplay-windows}"; Flags: shellexec postinstall skipifsilent

[Code]
type
  TProcessEntry32 = record
    dwSize, cntUsage, th32ProcessID, th32DefaultHeapID, th32ModuleID, cntThreads, th32ParentProcessID: DWORD;
    pcPriClassBase: Longint;
    dwFlags: DWORD;
    szExeFile: array[0..259] of AnsiChar;
  end;

var
  BonjourDownloadPage: TDownloadWizardPage;
  BonjourInstallNeeded: Boolean;

function CreateToolhelp32Snapshot(dwFlags, th32ProcessID: DWORD): THandle; external 'CreateToolhelp32Snapshot@kernel32.dll stdcall';
function Process32First(hSnapshot: THandle; var lppe: TProcessEntry32): BOOL; external 'Process32First@kernel32.dll stdcall';
function Process32Next(hSnapshot: THandle; var lppe: TProcessEntry32): BOOL; external 'Process32Next@kernel32.dll stdcall';
function CloseHandle(hObject: THandle): BOOL; external 'CloseHandle@kernel32.dll stdcall';
function OpenProcess(dwDesiredAccess: DWORD; bInheritHandle: BOOL; dwProcessId: DWORD): THandle; external 'OpenProcess@kernel32.dll stdcall';
function TerminateProcess(hProcess: THandle; uExitCode: UINT): BOOL; external 'TerminateProcess@kernel32.dll stdcall';

function IsBonjourInstalled(): Boolean;
begin
  Result := RegKeyExists(HKEY_LOCAL_MACHINE, 'SYSTEM\CurrentControlSet\Services\Bonjour Service');
end;

function VerifyFileHash(const FileName: String; const ExpectedHash: String): Boolean;
var ResultCode: Integer; ActualHash: String; TmpFile: String;
begin
  Result := False; TmpFile := ExpandConstant('{tmp}\h.txt');
  if Exec('powershell.exe', Format('-NoProfile -Command "(Get-FileHash ''%s'' -Algorithm SHA256).Hash | Out-File -Encoding ascii ''%s''"', [FileName, TmpFile]), '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    if LoadStringFromFile(TmpFile, ActualHash) then Result := (CompareText(Trim(ActualHash), ExpectedHash) = 0);
end;

function KillProcess(const PName: String): Boolean;
var hSnap: THandle; PE: TProcessEntry32; hProc: THandle; i: Integer; EName: String;
begin
  Result := False; hSnap := CreateToolhelp32Snapshot($2, 0);
  if hSnap <> $FFFFFFFF then try
    PE.dwSize := SizeOf(PE);
    if Process32First(hSnap, PE) then while True do begin
      EName := ''; for i := 0 to 259 do begin if PE.szExeFile[i] = #0 then Break; EName := EName + PE.szExeFile[i]; end;
      if CompareText(EName, PName) = 0 then begin
        hProc := OpenProcess($1, False, PE.th32ProcessID);
        if hProc <> 0 then try Result := TerminateProcess(hProc, 1); finally CloseHandle(hProc); end;
      end;
      if not Process32Next(hSnap, PE) then Break;
    end;
  finally CloseHandle(hSnap); end;
end;

procedure InitializeWizard;
begin
  if not IsBonjourInstalled() then begin
    if MsgBox('Bonjour is required. Download and install automatically?', mbConfirmation, MB_YESNO) = IDYES then begin
      BonjourInstallNeeded := True;
      BonjourDownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), 'Downloading Bonjour...', nil);
      BonjourDownloadPage.Add('https://download.info.apple.com/Mac_OS_X/061-8098.20100603.gthyu/BonjourPSSetup.exe', 'BonjourPSSetup.exe', '');
    end else Abort;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var RCode: Integer; Path, ExPath, MsiPath: String;
begin
  Result := '';
  if BonjourInstallNeeded then try
    BonjourDownloadPage.Show; try BonjourDownloadPage.Download; Path := ExpandConstant('{tmp}\BonjourPSSetup.exe'); finally BonjourDownloadPage.Hide; end;
    if not VerifyFileHash(Path, 'EDB12502690E8F266F5310F0A381667A8CC3D7C91CB43C3A278913B1F8399580') then begin Result := 'Hash mismatch'; Exit; end;
    ExPath := ExpandConstant('{tmp}\bj'); CreateDir(ExPath);
    if Exec(Path, '/extract "' + ExPath + '"', '', SW_HIDE, ewWaitUntilTerminated, RCode) then begin
      MsiPath := ExPath + '\Bonjour64.msi';
      if not Exec('msiexec.exe', '/i "' + MsiPath + '"', '', SW_SHOW, ewWaitUntilTerminated, RCode) then Result := 'Install failed';
    end;
  except Result := GetExceptionMessage; end;
end;

function InitializeUninstall(): Boolean;
begin KillProcess('uxplay-windows.exe'); KillProcess('uxplay.exe'); Result := True; end;