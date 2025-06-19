# Windows Packaging Script for KhDetector
# Creates an MSI installer using NSIS for distribution

param(
    [string]$ProductVersion = "1.0.0",
    [string]$BuildDir = "build",
    [string]$PackageDir = "packages",
    [string]$CertificateThumbprint = "DUMMY123456789ABCDEF",
    [string]$CertificatePassword = "",
    [switch]$SkipSigning = $false
)

# Configuration
$ProductName = "KhDetector"
$CompanyName = "KhDetector Audio"
$BundleId = "com.khdetector.audio.plugin"
$OutputMsi = "$PackageDir\KhDetector-$ProductVersion-Windows.msi"

# Colors for output
$Colors = @{
    Red = "Red"
    Green = "Green"  
    Yellow = "Yellow"
    Blue = "Cyan"
}

function Write-Log {
    param([string]$Message, [string]$Color = "White")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] $Message" -ForegroundColor $Color
}

function Write-Error-Log {
    param([string]$Message)
    Write-Log "ERROR: $Message" -Color $Colors.Red
    exit 1
}

function Write-Warning-Log {
    param([string]$Message)
    Write-Log "WARNING: $Message" -Color $Colors.Yellow
}

function Write-Success-Log {
    param([string]$Message)
    Write-Log "SUCCESS: $Message" -Color $Colors.Green
}

# Check prerequisites
function Test-Prerequisites {
    Write-Log "Checking prerequisites..." -Color $Colors.Blue
    
    # Check if build directory exists
    if (!(Test-Path $BuildDir)) {
        Write-Error-Log "Build directory '$BuildDir' not found. Run build first."
    }
    
    # Check for NSIS
    $nsisPath = Get-Command "makensis.exe" -ErrorAction SilentlyContinue
    if (!$nsisPath) {
        # Try common installation paths
        $commonPaths = @(
            "${env:ProgramFiles}\NSIS\makensis.exe",
            "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
            "C:\Program Files\NSIS\makensis.exe",
            "C:\Program Files (x86)\NSIS\makensis.exe"
        )
        
        foreach ($path in $commonPaths) {
            if (Test-Path $path) {
                $global:NsisPath = $path
                break
            }
        }
        
        if (!$global:NsisPath) {
            Write-Error-Log "NSIS not found. Please install NSIS from https://nsis.sourceforge.io/"
        }
    } else {
        $global:NsisPath = $nsisPath.Source
    }
    
    # Check for signtool if not skipping signing
    if (!$SkipSigning) {
        $signTool = Get-Command "signtool.exe" -ErrorAction SilentlyContinue
        if (!$signTool) {
            Write-Warning-Log "signtool.exe not found. Code signing will be skipped."
            $global:SkipSigning = $true
        }
    }
    
    # Check for built plugins
    $vst3Path = "$BuildDir\VST3\KhDetector.vst3"
    $clapPath = "$BuildDir\KhDetector_CLAP.clap"
    
    if (!(Test-Path $vst3Path) -and !(Test-Path $clapPath)) {
        Write-Error-Log "No plugin formats found in build directory"
    }
    
    Write-Success-Log "Prerequisites check passed"
}

# Create package structure
function New-PackageStructure {
    Write-Log "Creating package structure..." -Color $Colors.Blue
    
    # Create directories
    New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
    New-Item -ItemType Directory -Force -Path "temp\payload" | Out-Null
    New-Item -ItemType Directory -Force -Path "temp\resources" | Out-Null
    
    Write-Success-Log "Package structure created"
}

# Sign plugins
function Invoke-PluginSigning {
    if ($SkipSigning) {
        Write-Warning-Log "Skipping plugin signing"
        return
    }
    
    Write-Log "Signing plugins..." -Color $Colors.Blue
    
    $vst3Path = "$BuildDir\VST3\KhDetector.vst3"
    $clapPath = "$BuildDir\KhDetector_CLAP.clap"
    
    # Sign VST3
    if (Test-Path $vst3Path) {
        Write-Log "Signing VST3 plugin..."
        $vst3Executable = "$vst3Path\Contents\x86_64-win\KhDetector.vst3"
        if (Test-Path $vst3Executable) {
            & signtool.exe sign /sha1 $CertificateThumbprint /fd SHA256 /t http://timestamp.digicert.com $vst3Executable
            if ($LASTEXITCODE -eq 0) {
                Write-Success-Log "VST3 plugin signed"
            } else {
                Write-Warning-Log "VST3 signing failed"
            }
        }
    }
    
    # Sign CLAP
    if (Test-Path $clapPath) {
        Write-Log "Signing CLAP plugin..."
        $clapExecutable = "$clapPath\Contents\x86_64-win\KhDetector_CLAP.clap"
        if (Test-Path $clapExecutable) {
            & signtool.exe sign /sha1 $CertificateThumbprint /fd SHA256 /t http://timestamp.digicert.com $clapExecutable
            if ($LASTEXITCODE -eq 0) {
                Write-Success-Log "CLAP plugin signed"
            } else {
                Write-Warning-Log "CLAP signing failed"
            }
        }
    }
}

# Copy plugins to package
function Copy-PluginsToPackage {
    Write-Log "Copying plugins to package..." -Color $Colors.Blue
    
    $vst3Path = "$BuildDir\VST3\KhDetector.vst3"
    $clapPath = "$BuildDir\KhDetector_CLAP.clap"
    
    # Copy VST3
    if (Test-Path $vst3Path) {
        Copy-Item -Path $vst3Path -Destination "temp\payload\" -Recurse -Force
        Write-Log "VST3 plugin copied"
    }
    
    # Copy CLAP
    if (Test-Path $clapPath) {
        Copy-Item -Path $clapPath -Destination "temp\payload\" -Recurse -Force
        Write-Log "CLAP plugin copied"
    }
    
    # Copy demo applications if they exist
    if (Test-Path "$BuildDir\opengl_gui_demo.exe") {
        New-Item -ItemType Directory -Force -Path "temp\payload\Demo" | Out-Null
        Copy-Item -Path "$BuildDir\opengl_gui_demo.exe" -Destination "temp\payload\Demo\" -Force
        Write-Log "Demo applications copied"
    }
    
    Write-Success-Log "Plugins copied to package"
}

# Create NSIS script
function New-NsisScript {
    Write-Log "Creating NSIS script..." -Color $Colors.Blue
    
    $nsisScript = @"
; KhDetector NSIS Installer Script
; Generated automatically by package-windows.ps1

!define PRODUCT_NAME "$ProductName"
!define PRODUCT_VERSION "$ProductVersion"
!define PRODUCT_PUBLISHER "$CompanyName"
!define PRODUCT_WEB_SITE "https://khdetector.com"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\KhDetector"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Modern UI
!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "resources\installer.ico"
!define MUI_UNICON "resources\uninstaller.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "resources\welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "resources\welcome.bmp"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "resources\License.txt"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Components page
!insertmacro MUI_PAGE_COMPONENTS
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Launch KhDetector Demo"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchDemo"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "`${PRODUCT_NAME} `${PRODUCT_VERSION}"
OutFile "$OutputMsi"
InstallDir "`$PROGRAMFILES64\KhDetector"
InstallDirRegKey HKLM "`${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

; Version info
VIProductVersion "$ProductVersion.0"
VIAddVersionKey "ProductName" "`${PRODUCT_NAME}"
VIAddVersionKey "ProductVersion" "`${PRODUCT_VERSION}"
VIAddVersionKey "CompanyName" "`${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "`${PRODUCT_NAME} Installer"
VIAddVersionKey "FileVersion" "`${PRODUCT_VERSION}.0"
VIAddVersionKey "LegalCopyright" "Copyright (c) 2024 `${PRODUCT_PUBLISHER}"

; System requirements
Function .onInit
  ; Check Windows version (Windows 10 or later)
  `${IfNot} `${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "This software requires Windows 10 or later."
    Abort
  `${EndIf}
  
  ; Check if 64-bit
  `${IfNot} `${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "This software requires a 64-bit version of Windows."
    Abort
  `${EndIf}
FunctionEnd

; Installation sections
Section "Core Files" SecCore
  SectionIn RO
  SetOutPath "`$INSTDIR"
  
  ; Create uninstaller
  WriteUninstaller "`$INSTDIR\uninstall.exe"
  
  ; Registry entries
  WriteRegStr HKLM "`${PRODUCT_DIR_REGKEY}" "" "`$INSTDIR"
  WriteRegStr `${PRODUCT_UNINST_ROOT_KEY} "`${PRODUCT_UNINST_KEY}" "DisplayName" "`$(^Name)"
  WriteRegStr `${PRODUCT_UNINST_ROOT_KEY} "`${PRODUCT_UNINST_KEY}" "UninstallString" "`$INSTDIR\uninstall.exe"
  WriteRegStr `${PRODUCT_UNINST_ROOT_KEY} "`${PRODUCT_UNINST_KEY}" "DisplayIcon" "`$INSTDIR\KhDetector.exe"
  WriteRegStr `${PRODUCT_UNINST_ROOT_KEY} "`${PRODUCT_UNINST_KEY}" "DisplayVersion" "`${PRODUCT_VERSION}"
  WriteRegStr `${PRODUCT_UNINST_ROOT_KEY} "`${PRODUCT_UNINST_KEY}" "Publisher" "`${PRODUCT_PUBLISHER}"
SectionEnd

Section "VST3 Plugin" SecVST3
  SetOutPath "`$COMMONFILES64\VST3"
  File /r "payload\KhDetector.vst3"
  
  ; Register VST3
  ExecWait '"`$SYSDIR\regsvr32.exe" /s "`$COMMONFILES64\VST3\KhDetector.vst3\Contents\x86_64-win\KhDetector.vst3"'
SectionEnd

Section "CLAP Plugin" SecCLAP
  SetOutPath "`$COMMONFILES64\CLAP"
  File /r "payload\KhDetector_CLAP.clap"
SectionEnd

Section /o "Demo Application" SecDemo
  SetOutPath "`$INSTDIR\Demo"
  File /r "payload\Demo\*.*"
  
  ; Create shortcuts
  CreateDirectory "`$SMPROGRAMS\KhDetector"
  CreateShortCut "`$SMPROGRAMS\KhDetector\KhDetector Demo.lnk" "`$INSTDIR\Demo\opengl_gui_demo.exe"
  CreateShortCut "`$SMPROGRAMS\KhDetector\Uninstall.lnk" "`$INSTDIR\uninstall.exe"
SectionEnd

; Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT `${SecCore} "Core KhDetector files (required)"
  !insertmacro MUI_DESCRIPTION_TEXT `${SecVST3} "VST3 plugin for most DAWs"
  !insertmacro MUI_DESCRIPTION_TEXT `${SecCLAP} "CLAP plugin (next-generation format)"
  !insertmacro MUI_DESCRIPTION_TEXT `${SecDemo} "Demo application and documentation"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Launch demo function
Function LaunchDemo
  Exec "`$INSTDIR\Demo\opengl_gui_demo.exe"
FunctionEnd

; Uninstaller
Section Uninstall
  ; Remove files
  Delete "`$INSTDIR\uninstall.exe"
  RMDir /r "`$INSTDIR\Demo"
  RMDir "`$INSTDIR"
  
  ; Remove plugins
  RMDir /r "`$COMMONFILES64\VST3\KhDetector.vst3"
  RMDir /r "`$COMMONFILES64\CLAP\KhDetector_CLAP.clap"
  
  ; Remove shortcuts
  Delete "`$SMPROGRAMS\KhDetector\KhDetector Demo.lnk"
  Delete "`$SMPROGRAMS\KhDetector\Uninstall.lnk"
  RMDir "`$SMPROGRAMS\KhDetector"
  
  ; Remove registry entries
  DeleteRegKey `${PRODUCT_UNINST_ROOT_KEY} "`${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "`${PRODUCT_DIR_REGKEY}"
  
  SetAutoClose true
SectionEnd
"@

    $nsisScript | Out-File -FilePath "temp\installer.nsi" -Encoding UTF8
    Write-Success-Log "NSIS script created"
}

# Create installer resources
function New-InstallerResources {
    Write-Log "Creating installer resources..." -Color $Colors.Blue
    
    # License text
    $licenseText = @"
KhDetector Software License Agreement

Copyright (c) 2024 KhDetector Audio. All rights reserved.

This software is provided "as is" without warranty of any kind, express or implied.

By installing this software, you agree to the terms and conditions of this license.

INSTALLATION LOCATIONS:
- VST3: C:\Program Files\Common Files\VST3\KhDetector.vst3
- CLAP: C:\Program Files\Common Files\CLAP\KhDetector_CLAP.clap

SYSTEM REQUIREMENTS:
- Windows 10 or later (64-bit)
- Compatible DAW software
- At least 100 MB free disk space

For full license terms, please visit: https://khdetector.com/license
"@

    $licenseText | Out-File -FilePath "temp\resources\License.txt" -Encoding UTF8
    
    # Create dummy icon files if they don't exist
    if (!(Test-Path "temp\resources\installer.ico")) {
        # Create a simple text file as placeholder
        "ICON" | Out-File -FilePath "temp\resources\installer.ico" -Encoding ASCII
    }
    
    if (!(Test-Path "temp\resources\uninstaller.ico")) {
        "ICON" | Out-File -FilePath "temp\resources\uninstaller.ico" -Encoding ASCII
    }
    
    if (!(Test-Path "temp\resources\welcome.bmp")) {
        "BMP" | Out-File -FilePath "temp\resources\welcome.bmp" -Encoding ASCII
    }
    
    Write-Success-Log "Installer resources created"
}

# Build MSI package
function Invoke-MsiBuild {
    Write-Log "Building MSI package..." -Color $Colors.Blue
    
    Push-Location "temp"
    try {
        & $global:NsisPath "installer.nsi"
        if ($LASTEXITCODE -ne 0) {
            Write-Error-Log "NSIS build failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
    
    # Move the output file to the correct location
    if (Test-Path "temp\$OutputMsi") {
        Move-Item "temp\$OutputMsi" $OutputMsi -Force
    }
    
    Write-Success-Log "MSI package built: $OutputMsi"
}

# Sign the final MSI
function Invoke-MsiSigning {
    if ($SkipSigning) {
        Write-Warning-Log "Skipping MSI signing"
        return
    }
    
    Write-Log "Signing MSI package..." -Color $Colors.Blue
    
    & signtool.exe sign /sha1 $CertificateThumbprint /fd SHA256 /t http://timestamp.digicert.com $OutputMsi
    
    if ($LASTEXITCODE -eq 0) {
        Write-Success-Log "MSI package signed successfully"
        
        # Verify signature
        & signtool.exe verify /pa $OutputMsi
        if ($LASTEXITCODE -eq 0) {
            Write-Success-Log "MSI signature verified"
        }
    } else {
        Write-Warning-Log "MSI signing failed"
    }
}

# Create checksums
function New-Checksums {
    Write-Log "Creating checksums..." -Color $Colors.Blue
    
    # SHA256
    $sha256 = Get-FileHash -Path $OutputMsi -Algorithm SHA256
    "$($sha256.Hash.ToLower())  $(Split-Path $OutputMsi -Leaf)" | Out-File -FilePath "$OutputMsi.sha256" -Encoding ASCII
    
    # MD5
    $md5 = Get-FileHash -Path $OutputMsi -Algorithm MD5
    "$($md5.Hash.ToLower())  $(Split-Path $OutputMsi -Leaf)" | Out-File -FilePath "$OutputMsi.md5" -Encoding ASCII
    
    Write-Success-Log "Checksums created"
}

# Cleanup
function Remove-TempFiles {
    Write-Log "Cleaning up temporary files..." -Color $Colors.Blue
    
    if (Test-Path "temp") {
        Remove-Item -Path "temp" -Recurse -Force
    }
    
    Write-Success-Log "Cleanup completed"
}

# Main execution
function Main {
    Write-Log "Starting Windows packaging for $ProductName v$ProductVersion" -Color $Colors.Blue
    
    try {
        Test-Prerequisites
        New-PackageStructure
        Invoke-PluginSigning
        Copy-PluginsToPackage
        New-InstallerResources
        New-NsisScript
        Invoke-MsiBuild
        Invoke-MsiSigning
        New-Checksums
        
        Write-Success-Log "Windows package created successfully!"
        Write-Log "Package location: $OutputMsi" -Color $Colors.Blue
        
        if (Test-Path $OutputMsi) {
            $size = (Get-Item $OutputMsi).Length / 1MB
            Write-Log "Package size: $([math]::Round($size, 2)) MB" -Color $Colors.Blue
        }
    }
    catch {
        Write-Error-Log "Packaging failed: $($_.Exception.Message)"
    }
    finally {
        Remove-TempFiles
    }
}

# Run main function
Main 