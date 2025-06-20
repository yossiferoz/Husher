# Contributing to KhDetector

Thank you for your interest in contributing to KhDetector! This document provides guidelines for contributing to the project.

## Development Setup

1. Clone the repository:
```bash
git clone https://github.com/yossiferoz/Hush.git
cd Hush
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
- Keep functions focused and small
- Use RAII for resource management

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

- Test the plugin in at least one DAW (Ableton Live, Reaper, Logic Pro, etc.)

## Pull Request Process

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Commit your changes: `git commit -m 'Add amazing feature'`
4. Push to the branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

## Plugin Development Guidelines

### Real-time Safety
- No memory allocation in audio processing thread
- Use lock-free data structures
- Avoid blocking operations
- Test with thread sanitizer

### Performance
- Profile critical paths
- Use SIMD when appropriate
- Minimize CPU usage
- Target <5% CPU usage on modern systems

### Compatibility
- Test on multiple DAWs
- Support common sample rates (44.1kHz, 48kHz, 96kHz)
- Handle various buffer sizes
- Test on both Intel and Apple Silicon Macs

## Reporting Issues

Please use the issue templates provided in the repository:
- Bug reports: Include system info, DAW, and reproduction steps
- Feature requests: Describe the use case and expected behavior

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

## Questions?

Feel free to open an issue for any questions about contributing! 