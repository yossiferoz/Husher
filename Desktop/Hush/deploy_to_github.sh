#!/bin/bash

# GitHub Deployment Script for KhDetector
# Helps deploy the project to GitHub with proper configuration

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

echo "🚀 KhDetector GitHub Deployment Script"
echo "======================================"
echo

# Check if git is configured
check_git_config() {
    log "Checking Git configuration..."
    
    local git_name=$(git config --global user.name 2>/dev/null || echo "")
    local git_email=$(git config --global user.email 2>/dev/null || echo "")
    
    if [[ -z "$git_name" ]]; then
        warning "Git user.name not configured"
        read -p "Enter your GitHub username: " github_username
        git config --global user.name "$github_username"
        success "Git user.name configured: $github_username"
    else
        success "Git user.name: $git_name"
    fi
    
    if [[ -z "$git_email" ]]; then
        warning "Git user.email not configured"
        read -p "Enter your GitHub email: " github_email
        git config --global user.email "$github_email"
        success "Git user.email configured: $github_email"
    else
        success "Git user.email: $git_email"
    fi
}

# Create GitHub repository
create_github_repo() {
    log "GitHub Repository Setup"
    echo
    echo "To create a GitHub repository, you have two options:"
    echo
    echo "1. 🌐 Create via GitHub Web Interface (Recommended)"
    echo "2. 📱 Create via GitHub CLI (if installed)"
    echo
    
    read -p "Choose option (1 or 2): " choice
    
    case $choice in
        1)
            create_repo_web
            ;;
        2)
            create_repo_cli
            ;;
        *)
            warning "Invalid choice. Using web interface method."
            create_repo_web
            ;;
    esac
}

# Create repository via web interface
create_repo_web() {
    log "Creating repository via GitHub web interface..."
    echo
    echo "📋 Follow these steps:"
    echo "1. Go to: https://github.com/new"
    echo "2. Repository name: KhDetector (or your preferred name)"
    echo "3. Description: Cross-platform audio plugin for kick drum detection using AI"
    echo "4. Set to Public or Private (your choice)"
    echo "5. ❌ DO NOT initialize with README, .gitignore, or license (we already have these)"
    echo "6. Click 'Create repository'"
    echo
    
    read -p "Press Enter after you've created the repository on GitHub..."
    
    echo
    read -p "Enter the repository URL (e.g., https://github.com/yourusername/KhDetector.git): " repo_url
    
    if [[ -z "$repo_url" ]]; then
        error "Repository URL is required"
        exit 1
    fi
    
    # Add remote origin
    git remote add origin "$repo_url"
    success "Remote origin added: $repo_url"
}

# Create repository via GitHub CLI
create_repo_cli() {
    log "Creating repository via GitHub CLI..."
    
    if ! command -v gh &> /dev/null; then
        error "GitHub CLI (gh) is not installed"
        echo "Install it from: https://cli.github.com/"
        echo "Or use the web interface method instead."
        exit 1
    fi
    
    # Check if logged in
    if ! gh auth status &> /dev/null; then
        log "Please log in to GitHub CLI..."
        gh auth login
    fi
    
    read -p "Enter repository name (default: KhDetector): " repo_name
    repo_name=${repo_name:-KhDetector}
    
    read -p "Make repository public? (y/N): " is_public
    
    local visibility_flag=""
    if [[ "$is_public" =~ ^[Yy]$ ]]; then
        visibility_flag="--public"
    else
        visibility_flag="--private"
    fi
    
    # Create repository
    gh repo create "$repo_name" \
        --description "Cross-platform audio plugin for kick drum detection using AI" \
        $visibility_flag \
        --source=. \
        --remote=origin \
        --push
    
    success "Repository created and pushed via GitHub CLI"
    return 0
}

# Push to GitHub
push_to_github() {
    log "Pushing to GitHub..."
    
    # Check if we have a remote
    if ! git remote get-url origin &> /dev/null; then
        error "No remote origin configured. Please run the repository creation step first."
        exit 1
    fi
    
    # Push to GitHub
    log "Pushing to remote repository..."
    git push -u origin master
    
    if [[ $? -eq 0 ]]; then
        success "Successfully pushed to GitHub!"
        
        # Get the repository URL
        local repo_url=$(git remote get-url origin)
        local web_url=${repo_url%.git}
        
        echo
        success "🎉 Deployment Complete!"
        echo
        log "Your KhDetector project is now on GitHub:"
        log "Repository: $web_url"
        echo
        log "Project includes:"
        log "  ✅ Complete audio plugin source code"
        log "  ✅ Cross-platform build system (CMake)"
        log "  ✅ CI/CD pipelines with GitHub Actions"
        log "  ✅ Comprehensive unit tests"
        log "  ✅ DMG installer for macOS"
        log "  ✅ Architecture documentation"
        log "  ✅ Installation guides"
        echo
        log "Next steps:"
        log "  1. 🌟 Star your repository if you want"
        log "  2. 📝 Edit the README.md if needed"
        log "  3. 🏷️  Create releases for plugin versions"
        log "  4. 🔧 Configure GitHub Actions secrets for signing"
        echo
        
        # Open in browser
        read -p "Open repository in browser? (y/N): " open_browser
        if [[ "$open_browser" =~ ^[Yy]$ ]]; then
            if command -v open &> /dev/null; then
                open "$web_url"
            elif command -v xdg-open &> /dev/null; then
                xdg-open "$web_url"
            else
                log "Please manually open: $web_url"
            fi
        fi
        
    else
        error "Failed to push to GitHub"
        echo
        log "Common solutions:"
        log "1. Check your GitHub credentials"
        log "2. Verify repository URL is correct"
        log "3. Ensure you have push permissions"
        log "4. Try: git push --set-upstream origin master"
        exit 1
    fi
}

# Update repository with latest features
update_repo_features() {
    log "Adding GitHub-specific features..."
    
    # Create GitHub issue templates
    mkdir -p .github/ISSUE_TEMPLATE
    
    cat > .github/ISSUE_TEMPLATE/bug_report.md << 'EOF'
---
name: Bug report
about: Create a report to help us improve
title: '[BUG] '
labels: bug
assignees: ''
---

**Describe the bug**
A clear and concise description of what the bug is.

**To Reproduce**
Steps to reproduce the behavior:
1. Go to '...'
2. Click on '....'
3. Scroll down to '....'
4. See error

**Expected behavior**
A clear and concise description of what you expected to happen.

**Screenshots**
If applicable, add screenshots to help explain your problem.

**System Information:**
 - OS: [e.g. macOS 12.0, Windows 11]
 - DAW: [e.g. Ableton Live 12, Logic Pro X]
 - Plugin Version: [e.g. 1.0.0]

**Additional context**
Add any other context about the problem here.
EOF

    cat > .github/ISSUE_TEMPLATE/feature_request.md << 'EOF'
---
name: Feature request
about: Suggest an idea for this project
title: '[FEATURE] '
labels: enhancement
assignees: ''
---

**Is your feature request related to a problem? Please describe.**
A clear and concise description of what the problem is. Ex. I'm always frustrated when [...]

**Describe the solution you'd like**
A clear and concise description of what you want to happen.

**Describe alternatives you've considered**
A clear and concise description of any alternative solutions or features you've considered.

**Additional context**
Add any other context or screenshots about the feature request here.
EOF

    # Create pull request template
    cat > .github/pull_request_template.md << 'EOF'
## Description
Brief description of the changes in this PR.

## Type of Change
- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update

## Testing
- [ ] I have tested this change locally
- [ ] I have added tests that prove my fix is effective or that my feature works
- [ ] New and existing unit tests pass locally with my changes

## Checklist
- [ ] My code follows the style guidelines of this project
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
EOF

    # Create contributing guidelines
    cat > CONTRIBUTING.md << 'EOF'
# Contributing to KhDetector

Thank you for your interest in contributing to KhDetector! This document provides guidelines for contributing to the project.

## Development Setup

1. Clone the repository:
```bash
git clone https://github.com/yourusername/KhDetector.git
cd KhDetector
```

2. Initialize submodules:
```bash
git submodule update --init --recursive
```

3. Build the project:
```bash
./build.sh
```

## Code Style

- Use C++17 standard
- Follow existing code formatting
- Add comments for complex algorithms
- Use meaningful variable names

## Testing

- Run unit tests before submitting PR:
```bash
cd build && ctest
```

- Test with sanitizers:
```bash
cmake -DENABLE_SANITIZERS=ON ..
make && ctest
```

## Pull Request Process

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Commit your changes: `git commit -m 'Add amazing feature'`
4. Push to the branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

## Reporting Issues

Please use the issue templates provided in the repository.

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.
EOF

    # Add these files to git
    git add .github/ISSUE_TEMPLATE/ .github/pull_request_template.md CONTRIBUTING.md
    git commit -m "Add GitHub templates and contributing guidelines"
    
    success "Added GitHub-specific features"
}

# Main execution
main() {
    log "Starting GitHub deployment for KhDetector..."
    echo
    
    check_git_config
    
    # Check if we already have a remote
    if git remote get-url origin &> /dev/null; then
        log "Remote origin already exists:"
        git remote get-url origin
        echo
        read -p "Push to existing remote? (y/N): " use_existing
        
        if [[ "$use_existing" =~ ^[Yy]$ ]]; then
            push_to_github
        else
            log "Remove existing remote and create new one? (y/N): "
            read -p "" remove_remote
            if [[ "$remove_remote" =~ ^[Yy]$ ]]; then
                git remote remove origin
                create_github_repo
                update_repo_features
                push_to_github
            else
                log "Deployment cancelled"
                exit 0
            fi
        fi
    else
        create_github_repo
        update_repo_features
        push_to_github
    fi
}

# Run main function
main "$@" 