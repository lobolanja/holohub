---
name: refactoring-code
description: Provides code smell identification and refactoring techniques for improving existing code. Use when reviewing code for issues or refactoring to improve quality. Covers anti-patterns, when to refactor, and common refactoring moves.
---

# Refactoring Code

Reference material for identifying code smells and applying refactoring techniques.

## Purpose

Fix quality issues in production code identified by identify-code-issues. This is the REFACTOR phase - improve code without changing behavior.

## Contents

- [Quick Reference](#quick-reference) - Common smells at a glance
- [Detailed Patterns](./reference.md) - Deep dive with examples

## Quick Reference

### Common Code Smells

| Smell | Symptom | Fix |
|-------|---------|-----|
| **God Object** | Class does everything | Extract focused classes |
| **Hardcoded Dependencies** | `self.db = PostgresDatabase()` | Inject via constructor |
| **Type Discrimination** | `if type == "x": ... elif type == "y":` | Use polymorphism |
| **Long Method** | Method > 20 lines | Extract methods |
| **Long Parameter List** | 3+ parameters | Introduce parameter object |
| **Feature Envy** | Method uses other class's data more | Move method |
| **Data Clumps** | Same fields appear together | Extract class |

### Refactoring Safety

1. Tests must pass before refactoring
2. Make small changes
3. Run tests after each change
4. Commit frequently
5. If tests break, revert and try smaller step

### Running Tests

After each fix, run only the **tests that exercise the modified code** (not the entire test suite):

1. Identify which test files cover the modified production code
2. Run those specific test files:
   ```
   CTest {relevant_test_file} {run_args}
   ```

**Verify-fix loop:**
1. Apply the fix to production code
2. Run the tests that exercise that code
3. If tests fail:
   - Analyze why the fix broke the tests
   - Adjust the fix (remember: tests are the spec, don't change test behavior)
   - Run again
4. Only proceed to the next issue when the relevant tests pass
5. If stuck after 3 attempts, ask the human operator how to proceed

**You may modify:**
- App/production code (primary focus)
- Test code if needed to support code refactoring

**Critical constraint:** Tests are the specification. All tests must still pass after refactoring, and tests must still enforce the same requirements. Don't change what the tests verify.

**Do NOT:**
- Change code behavior (tests must pass before and after)
- Add features not required by tests