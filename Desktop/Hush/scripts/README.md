# KhDetector Packaging Scripts

This directory contains scripts for creating distribution packages for KhDetector audio plugins.

## Overview

- **`package-macos.sh`** - Creates notarized macOS .pkg installers
- **`package-windows.ps1`** - Creates Windows .msi installers using NSIS

Both scripts support code signing with dummy certificates for CI/development and real certificates for production.

## macOS Packaging (`package-macos.sh`)

### Prerequisites

- macOS 10.13 or later
- Xcode Command Line Tools
- Built plugins in `build/` directory
- Code signing certificate (optional, dummy cert used if not provided)

### Usage

```bash
# Basic usage (uses default settings)
./scripts/package-macos.sh

# With custom version and build directory
PRODUCT_VERSION="1.2.0" BUILD_DIR="release_build" ./scripts/package-macos.sh

# Skip notarization (for CI or development)
CI=true ./scripts/package-macos.sh
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PRODUCT_VERSION` | `1.0.0` | Version number for the package |
| `BUILD_DIR` | `build` | Directory containing built plugins |
| `PACKAGE_DIR` | `packages` | Output directory for packages |
| `DEVELOPER_ID` | `Developer ID Installer: KhDetector Audio (DUMMY123)` | Code signing identity |
| `NOTARIZATION_PROFILE` | `khdetector-notarization` | Keychain profile for notarization |
| `CI` | `false` | Set to `true` to skip notarization |
| `FORCE_NOTARIZATION` | `false` | Force notarization even in CI |

### Output Files

- `KhDetector-{VERSION}-macOS.pkg` - Signed and notarized installer
- `KhDetector-{VERSION}-macOS.pkg.sha256` - SHA256 checksum
- `KhDetector-{VERSION}-macOS.pkg.md5` - MD5 checksum

### Package Contents

The installer includes:
- **VST3 Plugin**: `/Library/Audio/Plug-Ins/VST3/KhDetector.vst3`
- **Audio Unit**: `/Library/Audio/Plug-Ins/Components/KhDetector.component`
- **CLAP Plugin**: `/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap`
- **Demo App**: `/Applications/KhDetector/opengl_gui_demo` (if available)

### Code Signing Setup

For production releases, you'll need:

1. **Developer ID Certificate**: Obtain from Apple Developer Program
2. **Keychain Profile**: Set up with `xcrun notarytool store-credentials`

```bash
# Set up notarization profile
xcrun notarytool store-credentials "khdetector-notarization" \
  --apple-id "your-apple-id@example.com" \
  --team-id "YOUR_TEAM_ID" \
  --password "app-specific-password"

# Set environment variables
export DEVELOPER_ID="Developer ID Installer: Your Name (TEAM_ID)"
export NOTARIZATION_PROFILE="khdetector-notarization"
```

## Windows Packaging (`package-windows.ps1`)

### Prerequisites

- Windows 10 or later
- PowerShell 5.1 or later
- NSIS (Nullsoft Scriptable Install System)
- Built plugins in `build/` directory
- Code signing certificate (optional, dummy cert created if not provided)

### Installation

```powershell
# Install NSIS via Chocolatey
choco install nsis

# Or download from https://nsis.sourceforge.io/
```

### Usage

```powershell
# Basic usage
.\scripts\package-windows.ps1

# With custom parameters
.\scripts\package-windows.ps1 -ProductVersion "1.2.0" -BuildDir "release_build"

# Skip code signing
.\scripts\package-windows.ps1 -SkipSigning

# With custom certificate
.\scripts\package-windows.ps1 -CertificateThumbprint "YOUR_CERT_THUMBPRINT"
```

### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ProductVersion` | `1.0.0` | Version number for the package |
| `BuildDir` | `build` | Directory containing built plugins |
| `PackageDir` | `packages` | Output directory for packages |
| `CertificateThumbprint` | `DUMMY123456789ABCDEF` | Certificate thumbprint for signing |
| `CertificatePassword` | `""` | Certificate password (if required) |
| `SkipSigning` | `false` | Skip code signing entirely |

### Output Files

- `KhDetector-{VERSION}-Windows.msi` - Signed MSI installer
- `KhDetector-{VERSION}-Windows.msi.sha256` - SHA256 checksum
- `KhDetector-{VERSION}-Windows.msi.md5` - MD5 checksum

### Package Contents

The installer includes:
- **VST3 Plugin**: `C:\Program Files\Common Files\VST3\KhDetector.vst3`
- **CLAP Plugin**: `C:\Program Files\Common Files\CLAP\KhDetector_CLAP.clap`
- **Demo App**: `C:\Program Files\KhDetector\Demo\opengl_gui_demo.exe` (optional)

### Code Signing Setup

For production releases, you'll need a code signing certificate:

1. **Obtain Certificate**: From a trusted CA (DigiCert, Sectigo, etc.)
2. **Install Certificate**: Import to Windows Certificate Store
3. **Find Thumbprint**: Use Certificate Manager or PowerShell

```powershell
# Find certificate thumbprint
Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert | 
  Select-Object Subject, Thumbprint

# Use with script
.\scripts\package-windows.ps1 -CertificateThumbprint "YOUR_ACTUAL_THUMBPRINT"
```

## CI/CD Integration

Both scripts are designed to work seamlessly in CI environments with dummy certificates for testing.

### GitHub Actions

The scripts are integrated into `.github/workflows/build-and-package.yml`:

- **Automatic versioning** from git tags
- **Dummy certificate generation** for CI builds
- **Multi-architecture builds** (macOS universal binaries)
- **Artifact upload** with checksums
- **Release asset upload** for tagged builds

### Local Development

For local testing without real certificates:

```bash
# macOS - uses dummy certificate automatically
CI=true ./scripts/package-macos.sh

# Windows - creates dummy certificate
.\scripts\package-windows.ps1 -SkipSigning
```

## Troubleshooting

### macOS Issues

**"pkgbuild not found"**
- Install Xcode Command Line Tools: `xcode-select --install`

**"No plugin formats found"**
- Ensure plugins are built in the specified build directory
- Check that VST3, AU, or CLAP files exist

**"Notarization failed"**
- Verify Apple ID credentials and app-specific password
- Check notarization profile setup
- Use `CI=true` to skip notarization for testing

### Windows Issues

**"NSIS not found"**
- Install NSIS: `choco install nsis`
- Add NSIS to PATH: `C:\Program Files (x86)\NSIS`

**"signtool.exe not found"**
- Install Windows SDK or Visual Studio
- Use `-SkipSigning` for development builds

**"Certificate not found"**
- Verify certificate is installed in Certificate Store
- Check thumbprint is correct (no spaces or special characters)

## Security Notes

### Dummy Certificates

The scripts generate dummy certificates for CI/development:
- **Not trusted** by operating systems
- **Only for testing** packaging process
- **Should not be distributed** to end users

### Production Certificates

For distribution:
- **macOS**: Use Apple Developer ID certificates
- **Windows**: Use certificates from trusted CAs
- **Store securely**: Never commit certificates to version control
- **Use CI secrets**: Store certificate info as encrypted secrets

## Output Structure

```
packages/
├── KhDetector-1.0.0-macOS.pkg
├── KhDetector-1.0.0-macOS.pkg.sha256
├── KhDetector-1.0.0-macOS.pkg.md5
├── KhDetector-1.0.0-Windows.msi
├── KhDetector-1.0.0-Windows.msi.sha256
└── KhDetector-1.0.0-Windows.msi.md5
```

Each package includes:
- **Installer file** with all plugin formats
- **SHA256 checksum** for integrity verification
- **MD5 checksum** for legacy compatibility
- **Code signature** (dummy or real)
- **Proper metadata** (version, company info, etc.)

## Testing Packages

### macOS Testing

```bash
# Verify package signature
pkgutil --check-signature packages/KhDetector-1.0.0-macOS.pkg

# List package contents
pkgutil --payload-files packages/KhDetector-1.0.0-macOS.pkg

# Install for testing (requires sudo)
sudo installer -pkg packages/KhDetector-1.0.0-macOS.pkg -target /
```

### Windows Testing

```powershell
# Verify MSI signature
Get-AuthenticodeSignature packages/KhDetector-1.0.0-Windows.msi

# Install silently for testing (requires admin)
msiexec /i packages/KhDetector-1.0.0-Windows.msi /quiet

# Uninstall
msiexec /x packages/KhDetector-1.0.0-Windows.msi /quiet
``` 