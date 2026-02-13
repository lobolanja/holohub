# Copilot Guidelines for Connext Application Development

## Overview

This document provides guidelines for GitHub Copilot to effectively assist with developing Holoscan applications using RTI Connext DDS. These guidelines ensure consistent application of defined coding skills and best practices.

## Mandatory Skills Application

When assisting with development tasks, you **MUST** apply the following skills located in `.ia/skills/`:

### 1. Writing Code Skill (`skills/writing-code/`)

**When to apply:** Any task involving code implementation, new features, or writing test code.

**Key principles to enforce:**

- **SOLID Principles:**
  - Single Responsibility: Each class has ONE reason to change
  - Open/Closed: Open for extension, closed for modification
  - Liskov Substitution: Subtypes must be substitutable for base types
  - Interface Segregation: Focused interfaces, not fat ones
  - Dependency Inversion: Depend on abstractions, not concretions

- **Clean Code Practices:**
  - Class names are nouns: `PaymentProcessor`, `DDSReceiver`, `PayloadValidator`
  - Method names are verbs: `process()`, `validate()`, `send()`, `receive()`
  - Functions should be small (≤20 lines)
  - Do one thing at one level of abstraction
  - Few arguments: 0-1 ideal, 2 good, 3+ avoid
  - Use exceptions instead of error codes
  - Don't return or pass null

- **Minimal Code Approach:**
  - Only implement what tests explicitly check
  - Start with hardcoded values if it passes tests
  - Let failing tests drive real logic implementation

### 2. Refactoring Skill (`skills/refactor/`)

**When to apply:** Any task involving code analysis, quality improvement, or refactoring existing code.

**Code smells to identify:**

| Smell | Symptom | Recommended Fix |
|-------|---------|-----------------|
| God Object | Class does everything | Extract focused classes |
| Hardcoded Dependencies | Direct instantiation in constructors | Inject via constructor |
| Type Discrimination | `if type == "x": ... elif type == "y":` | Use polymorphism |
| Long Method | Method > 20 lines | Extract methods |
| Long Parameter List | 3+ parameters | Introduce parameter object |
| Feature Envy | Method uses other class's data more | Move method |
| Data Clumps | Same fields appear together | Extract class |

**Refactoring safety protocol:**

1. ✅ Tests must pass before refactoring
2. 🔄 Make small, incremental changes
3. ✅ Run tests after each change
4. 💾 Commit frequently
5. ⚠️ If tests break, revert and try smaller steps

**Test execution after refactoring:**

- Run only tests that exercise the modified code
- Use: `CTest {relevant_test_file} {run_args}`
- Verify-fix loop:
  1. Apply fix to production code
  2. Run relevant tests
  3. If tests fail, analyze and adjust (don't change test behavior)
  4. Repeat until tests pass

**Critical constraints:**

- ❌ Do NOT change code behavior (tests must pass before and after)
- ❌ Do NOT add features not required by tests
- ❌ Do NOT change what tests verify (tests are the specification)
- ✅ You MAY modify app/production code
- ✅ You MAY modify test code to support refactoring

## Connext Application Context

### Technology Stack

- **Holoscan SDK**: Application framework
- **RTI Connext DDS 7.3.0**: Data Distribution Service middleware
- **ANO Transport**: Advanced Network Operator (simulated via DDS)
- **CUDA**: GPU acceleration
- **C++**: Primary implementation language

### Project Structure

```
connext/
├── .ia/
│   ├── skills/          # Coding guidelines (writing-code, refactor)
│   └── agents/          # (empty - for future agent definitions)
├── common/              # Shared utilities
├── connext_ano_basic_app/  # Basic ANO example
└── connext_app_cpp/     # Main C++ application
```

### Configuration Files

- `connext_receiver.yaml`: RX mode configuration
- `connext_sender.yaml`: TX mode configuration

**Key sections:**
- `demo`: Runtime controls (message_count, message_period_ms, discovery_wait_ms)
- `payload_source`: Payload configuration
- `connext_tx`/`connext_rx`: Transport-specific parameters (enable_dds, enable_ano, domain_id, topic_name, ano_channel)

### Build and Run Guidelines

**Build command:**
```sh
./holohub build connext_app_cpp --build-type debug [--local]
```

**Environment requirements:**
- `RTI_LICENSE_FILE`: Must point to valid RTI license file
- `LD_LIBRARY_PATH`: Include CUDA toolkit libraries

## Development Workflow

### 1. Implementing New Features

1. Apply **writing-code** skill principles
2. Start with test-driven approach when possible
3. Keep functions small and focused
4. Use dependency injection for external dependencies (DDS, file I/O, etc.)
5. Follow SOLID principles
6. Ensure proper error handling with exceptions

### 2. Refactoring Existing Code

1. Apply **refactoring** skill principles
2. Ensure all tests pass before starting
3. Identify code smells systematically
4. Make incremental changes
5. Run relevant tests after each change
6. Never change test behavior or requirements
7. Commit frequently

### 3. Code Review Support

When analyzing code for issues:
1. Check against SOLID principles
2. Identify code smells from refactoring skill
3. Verify clean code practices
4. Suggest specific refactoring moves
5. Ensure dependency injection is used
6. Check function size and complexity

## Example Scenarios

### Scenario: Adding a New DDS Topic Handler

**Apply writing-code skill:**
- Create focused class with single responsibility: `TopicHandler`
- Use dependency injection for DDS participant
- Keep methods small (≤20 lines)
- Use meaningful names: `publishMessage()`, `subscribeToTopic()`
- Handle errors with exceptions

### Scenario: Refactoring Large Receiver Method

**Apply refactoring skill:**
1. Identify smell: Long Method (>20 lines)
2. Extract smaller methods with clear names
3. Run tests after each extraction
4. Ensure behavior remains identical

### Scenario: Removing Hardcoded Dependencies

**Apply both skills:**
1. **Refactoring**: Identify hardcoded dependency smell
2. **Writing-code**: Replace with constructor injection following Dependency Inversion principle
3. Update tests to inject mock dependencies
4. Verify all tests still pass

## Additional Guidelines

### Holoscan-Specific Considerations

- Follow Holoscan operator patterns for new components
- Use Holoscan SDK data types and conventions
- Respect YAML configuration structure
- Maintain compatibility with both DDS and ANO transports

### RTI Connext Best Practices

- Always check RTI_LICENSE_FILE before operations
- Use proper DDS QoS policies
- Handle discovery timeouts appropriately
- Clean up DDS entities properly

### Documentation Requirements

- Update README.md for new features
- Document configuration parameters in YAML
- Add inline comments for complex DDS interactions
- Follow HoloHub CONTRIBUTING.md guidelines

## References

- Writing Code Skill: `.ia/skills/writing-code/SKILL.md`
- Refactoring Skill: `.ia/skills/refactor/SKILL.md`
- HoloHub Contributing Guide: `/workspace/holohub/CONTRIBUTING.md`
- Connext Application README: `./README.md`
