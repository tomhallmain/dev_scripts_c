# dev_scripts_c

A C implementation of the dev_scripts command-line utilities, providing efficient data processing and development workflow tools.

## Project Structure

```
dev_scripts_c/
├── src/                    # Source files
│   ├── core/              # Core functionality
│   │   ├── dsc.c         # Main entry point
│   │   ├── command.c     # Command registration system
│   │   └── file.c        # File handling utilities
│   ├── commands/         # Individual command implementations
│   │   ├── fit.c        # Data fitting command
│   │   ├── reorder.c    # Data reordering command
│   │   └── ...
│   └── utils/           # Utility functions
├── include/              # Header files
│   ├── dsc.h           # Main header
│   ├── command.h       # Command system interface
│   └── ...
├── tests/               # Test files
├── lib/                 # External dependencies
├── build/              # Build output
└── Makefile           # Build configuration
```

## Building

```bash
# Build the project
make

# Run tests
make test

# Clean build artifacts
make clean
```

## Development

This project follows these key principles:

1. **Modular Design**: Each command is implemented as a separate module
2. **Error Handling**: Consistent error handling throughout
3. **Memory Safety**: Careful memory management with cleanup handlers
4. **Testing**: Comprehensive test coverage
5. **Documentation**: Clear documentation of APIs and usage

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## License

MIT License 