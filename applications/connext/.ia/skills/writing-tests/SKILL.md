---
name: writing-tests
description: Provides test structure patterns, FIRST principles, and anti-patterns for writing effective tests. Use when implementing, reviewing, or refactoring test code. Covers four-phase structure, fixture usage, and common test smells.
---

# Writing Tests

Reference material for writing effective, maintainable tests.

## Contents

- [Quick Reference](#quick-reference) - Four-phase, SUT, FIRST at a glance
- [Detailed Patterns](./reference.md) - Deep dive with examples
- [Anti-Patterns](./anti-patterns.md) - What to avoid and fixes
- [Python & pytest](./python.md) - Language-specific conventions

## Quick Reference

### Four-Phase Test Structure

Every test follows: **Setup, Exercise, Verify, Teardown** (SEVT).

```python
def test_retries_failed_operations():
    # Setup
    attempts = []
    def operation():
        attempts.append(1)
        if len(attempts) < 3:
            raise ConnectionError("failed")
        return "success"

    # Exercise
    result = retry_operation(operation)

    # Verify
    assert result == "success"
    assert len(attempts) == 3
```

Use blank lines to separate phases, not comments.

### SUT Convention

Name the System Under Test `sut` for easy identification:

```python
def test_validates_email_format():
    sut = EmailValidator()

    result = sut.validate("invalid-email")

    assert result.is_valid == False
```

### Test Smells Quick Check

**Clarity:**
- Clear at a glance? (no Obscure Test)
- Tests ONE thing? (no Eager Test)
- All data visible? (no Mystery Guest)
- No if/for/while? (no Conditional Test Logic)

**Reliability:**
- Assertions have messages? (no Assertion Roulette)
- Won't break from unrelated changes? (no Fragile Test)
- Deterministic? (no Erratic Test)
- Fast? (no Slow Tests)

**Coverage:**
- Has assertions? (no Missing Assertions)
- Exercises new code? (no Untested Code)

### Test Types

#### Unit Tests

Test **individual components in isolation**. Fast, focused, independent.

**Characteristics:**
- Tests one function/class/method
- Runs in milliseconds
- No external dependencies
- Uses test doubles (fakes, mocks, stubs)
- Can run in parallel

**Example:**
```python
def test_calculates_total_price_with_tax():
    sut = PriceCalculator(tax_rate=0.1)

    result = sut.calculate(price=100.0)

    assert result == 110.0
```

**Reference** 
[how to write good unit tests](./unit-testing/SKILL.md) 
**When to use:** Verify business logic, algorithms, data transformations, validations.

#### Integration Tests

Test **multiple components working together**. Slower, verify interactions.

**Characteristics:**
- Tests component integration
- Runs in seconds
- May use real dependencies (database, filesystem)
- Cannot always run in parallel
- More setup/teardown

**Example:**
```python
def test_saves_order_to_database_and_sends_email():
    db = TestDatabase()
    email_service = FakeEmailService()
    sut = OrderService(db, email_service)
    order = Order(customer="John", total=100.0)

    sut.process(order)

    saved_order = db.query("SELECT * FROM orders WHERE customer='John'")
    assert saved_order.total == 100.0
    assert email_service.sent_emails[0].subject == "Order Confirmation"
```

**When to use:** Verify database operations, file I/O, API integrations, multi-component workflows.

**Key Differences:**

| Aspect | Unit Test | Integration Test |
|--------|-----------|------------------|
| Scope | Single component | Multiple components |
| Speed | Milliseconds | Seconds |
| Dependencies | Test doubles | Real or test instances |
| Isolation | Complete | Partial |
| Parallelization | Yes | Sometimes |
| Purpose | Verify logic | Verify integration |

**Testing Pyramid:** Write mostly unit tests (fast feedback), fewer integration tests (verify integration), fewest end-to-end tests (verify full system).
