---
name: unit-testing
description: Comprehensive guide to writing effective unit tests following FIRST principles. Covers test isolation, test doubles, TDD practices, and unit test characteristics. Use when writing or reviewing unit tests to ensure quality and maintainability.
---

# Unit Testing

Reference material for writing effective unit tests that verify individual components in isolation.

## Contents

- [What is a Unit Test](#what-is-a-unit-test)
- [FIRST Principles](#first-principles)
- [Test Doubles](#test-doubles)
- [Writing Good Unit Tests](#writing-good-unit-tests)
- [Common Patterns](#common-patterns)
- [Anti-Patterns](#anti-patterns)

## What is a Unit Test

A **unit test** verifies a single unit of code (function, method, class) in **complete isolation** from its dependencies.

**Characteristics:**
- Tests one behavior/requirement
- Runs in milliseconds
- No external dependencies (database, filesystem, network)
- Deterministic (same input → same output)
- Can run in any order
- Can run in parallel

**Example Unit:**
```python
class OrderCalculator:
    def calculate_total(self, items: list[Item]) -> float:
        """Calculate total price including tax."""
        subtotal = sum(item.price * item.quantity for item in items)
        tax = subtotal * 0.1
        return subtotal + tax
```

**Unit Test:**
```python
def test_calculates_total_with_single_item():
    sut = OrderCalculator()
    items = [Item(price=100.0, quantity=1)]

    result = sut.calculate_total(items)

    assert result == 110.0  # 100 + 10% tax

def test_calculates_total_with_multiple_items():
    sut = OrderCalculator()
    items = [
        Item(price=100.0, quantity=2),
        Item(price=50.0, quantity=1)
    ]

    result = sut.calculate_total(items)

    assert result == 275.0  # (200 + 50) + 10% tax

def test_calculates_zero_for_empty_list():
    sut = OrderCalculator()
    items = []

    result = sut.calculate_total(items)

    assert result == 0.0
```

## FIRST Principles

Every unit test must follow FIRST principles:

| Principle | Meaning |
|-----------|---------|
| **Fast** | Milliseconds, not seconds. Use fakes, not real databases. |
| **Independent** | No shared state between tests. Each test runs in isolation. |
| **Repeatable** | Same result in any environment (local, CI, offline). |
| **Self-validating** | Pass/fail via assertions, no manual inspection. |
| **Timely** | Written before production code (TDD practice). |

### Fast

Unit tests must run in **milliseconds**. Slow tests = fewer test runs = bugs caught later.

**Violation (Slow):**
```python
def test_saves_user_to_database():
    db = PostgresDatabase("localhost:5432")  # Real database!
    sut = UserRepository(db)
    user = User(name="John")

    sut.save(user)

    saved_user = db.query("SELECT * FROM users WHERE name='John'")
    assert saved_user.name == "John"
```

**Solution (Fast):**
```python
def test_saves_user_to_database():
    db = FakeDatabase()  # In-memory fake
    sut = UserRepository(db)
    user = User(name="John")

    sut.save(user)

    assert db.saved_users[0].name == "John"
```

### Independent

Tests must **not depend on each other** or shared state. Run in any order.

**Violation (Dependent):**
```python
shared_counter = 0

def test_increments_counter():
    global shared_counter
    sut = Counter(shared_counter)

    sut.increment()

    shared_counter = sut.value
    assert shared_counter == 1

def test_increments_counter_twice():
    global shared_counter
    sut = Counter(shared_counter)  # Depends on previous test!

    sut.increment()

    assert sut.value == 2  # Fails if run alone
```

**Solution (Independent):**
```python
def test_increments_counter_from_zero():
    sut = Counter(initial_value=0)

    sut.increment()

    assert sut.value == 1

def test_increments_counter_from_five():
    sut = Counter(initial_value=5)

    sut.increment()

    assert sut.value == 6
```

### Repeatable

Tests produce the **same result every time**, regardless of environment.

**Violation (Not Repeatable):**
```python
def test_generates_unique_id():
    sut = IdGenerator()

    result = sut.generate()

    assert result == "123e4567-e89b-12d3-a456-426614174000"  # Random ID!
```

**Solution (Repeatable):**
```python
def test_generates_unique_id():
    fake_random = FakeRandomGenerator(seed=42)
    sut = IdGenerator(random_generator=fake_random)

    result = sut.generate()

    assert result.startswith("id-")
    assert len(result) == 39
```

Or test the behavior, not the value:
```python
def test_generates_unique_ids():
    sut = IdGenerator()

    id1 = sut.generate()
    id2 = sut.generate()

    assert id1 != id2
    assert len(id1) == 36
    assert len(id2) == 36
```

### Self-Validating

Tests **pass or fail automatically**. No manual inspection needed.

**Violation (Manual Inspection):**
```python
def test_generates_report():
    sut = ReportGenerator()
    data = {"sales": 1000, "profit": 200}

    report = sut.generate(data)

    print(report)  # Manual inspection required!
```

**Solution (Self-Validating):**
```python
def test_generates_report_with_sales():
    sut = ReportGenerator()
    data = {"sales": 1000, "profit": 200}

    report = sut.generate(data)

    assert "Sales: $1000" in report
    assert "Profit: $200" in report
    assert "Margin: 20%" in report
```

### Timely

Write tests **before or immediately after** production code (TDD).

**TDD Workflow:**
1. **Red** - Write failing test
2. **Green** - Write minimal code to pass
3. **Refactor** - Improve code quality
4. Repeat

```python
# 1. RED: Write test first
def test_validates_email_format():
    sut = EmailValidator()  # Doesn't exist yet!

    result = sut.validate("user@example.com")

    assert result.is_valid == True

# 2. GREEN: Implement minimal code
class EmailValidator:
    def validate(self, email: str) -> ValidationResult:
        return ValidationResult(is_valid="@" in email)

# 3. REFACTOR: Improve implementation
class EmailValidator:
    def validate(self, email: str) -> ValidationResult:
        pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
        is_valid = re.match(pattern, email) is not None
        return ValidationResult(is_valid=is_valid)
```

## Test Doubles

Replace real dependencies with test doubles to maintain isolation and speed.

### Types of Test Doubles

| Type | Purpose | Example |
|------|---------|----------|
| **Fake** | Simplified working implementation | In-memory database |
| **Stub** | Returns predefined values | `return "fixed_value"` |
| **Mock** | Verifies interactions | `assert called_with("x")` |
| **Spy** | Records calls for verification | `calls = []` |
| **Dummy** | Fills parameter list | `None` or empty object |

### Fake

Simplified implementation that works but isn't production-ready.

```python
class FakeDatabase:
    def __init__(self):
        self.users = {}

    def save(self, user):
        self.users[user.id] = user

    def find(self, user_id):
        return self.users.get(user_id)

def test_saves_and_retrieves_user():
    db = FakeDatabase()
    sut = UserRepository(db)
    user = User(id=1, name="John")

    sut.save(user)
    result = sut.find(1)

    assert result.name == "John"
```

### Stub

Returns predefined responses.

```python
class StubPaymentGateway:
    def process_payment(self, amount):
        return PaymentResult(success=True, transaction_id="STUB123")

def test_processes_order_with_successful_payment():
    payment_gateway = StubPaymentGateway()
    sut = OrderProcessor(payment_gateway)
    order = Order(total=100.0)

    result = sut.process(order)

    assert result.status == "completed"
    assert result.payment_id == "STUB123"
```

### Mock

Verifies that specific interactions occurred.

```python
from unittest.mock import Mock

def test_sends_email_notification():
    email_service = Mock()
    sut = UserService(email_service)
    user = User(email="user@example.com")

    sut.create_account(user)

    email_service.send.assert_called_once_with(
        to="user@example.com",
        subject="Welcome!",
        body="Your account has been created."
    )
```

## Writing Good Unit Tests

### Test One Behavior

Each test verifies **one specific behavior**.

**Violation (Multiple Behaviors):**
```python
def test_user_service():
    sut = UserService()

    user = sut.create("John")
    assert user.name == "John"

    updated = sut.update(user.id, "Jane")
    assert updated.name == "Jane"

    sut.delete(user.id)
    assert sut.find(user.id) is None
```

**Solution (One Behavior Per Test):**
```python
def test_creates_user_with_given_name():
    sut = UserService()

    user = sut.create("John")

    assert user.name == "John"

def test_updates_user_name():
    sut = UserService()
    user = sut.create("John")

    updated = sut.update(user.id, "Jane")

    assert updated.name == "Jane"

def test_deletes_user():
    sut = UserService()
    user = sut.create("John")

    sut.delete(user.id)

    assert sut.find(user.id) is None
```

### Use Descriptive Names

Test names should describe **what** and **expected outcome**.

**Pattern:** `test_<behavior>_<expected_result>`

```python
# Good names
def test_validates_email_returns_false_for_missing_at_symbol():
def test_calculates_total_price_including_tax():
def test_raises_error_when_user_not_found():

# Bad names
def test_validation():
def test_calculate():
def test_error():
```

### Arrange-Act-Assert (AAA)

Also known as Setup-Exercise-Verify. Use blank lines to separate phases.

```python
def test_applies_discount_to_order_total():
    # Arrange
    sut = OrderCalculator()
    order = Order(subtotal=100.0)
    discount = Discount(percent=10)

    # Act
    result = sut.apply_discount(order, discount)

    # Assert
    assert result.total == 90.0
```

### Test Edge Cases

Test boundary conditions, not just happy paths.

```python
def test_calculates_total_with_empty_cart():
    sut = ShoppingCart()

    total = sut.calculate_total()

    assert total == 0.0

def test_applies_maximum_discount_of_100_percent():
    sut = OrderCalculator()
    order = Order(subtotal=100.0)
    discount = Discount(percent=150)  # Over 100%

    result = sut.apply_discount(order, discount)

    assert result.total == 0.0  # Can't be negative

def test_rejects_negative_quantity():
    sut = OrderValidator()
    order = Order(items=[Item(quantity=-1)])

    result = sut.validate(order)

    assert result.is_valid == False
    assert "negative quantity" in result.errors[0]
```

## Common Patterns

### Test Fixtures

Reuse setup code across tests.

```python
import pytest

@pytest.fixture
def calculator():
    return OrderCalculator(tax_rate=0.1)

@pytest.fixture
def sample_order():
    return Order(
        items=[
            Item(price=100.0, quantity=1),
            Item(price=50.0, quantity=2)
        ]
    )

def test_calculates_total(calculator, sample_order):
    result = calculator.calculate_total(sample_order)

    assert result == 220.0  # (100 + 100) + 10% tax

def test_applies_discount(calculator, sample_order):
    discount = Discount(amount=20.0)

    result = calculator.apply_discount(sample_order, discount)

    assert result == 200.0
```

### Parametrized Tests

Test multiple inputs with same logic.

```python
import pytest

@pytest.mark.parametrize("email,expected", [
    ("user@example.com", True),
    ("user.name@example.co.uk", True),
    ("invalid-email", False),
    ("@example.com", False),
    ("user@", False),
    ("", False),
])
def test_validates_email_format(email, expected):
    sut = EmailValidator()

    result = sut.validate(email)

    assert result.is_valid == expected
```

### Exception Testing

Verify that exceptions are raised correctly.

```python
import pytest

def test_raises_error_when_user_not_found():
    sut = UserRepository(FakeDatabase())

    with pytest.raises(UserNotFoundError) as exc_info:
        sut.find(999)

    assert "User 999 not found" in str(exc_info.value)

def test_raises_error_with_specific_message():
    sut = OrderValidator()
    invalid_order = Order(items=[])

    with pytest.raises(ValidationError, match="Order must contain items"):
        sut.validate(invalid_order)
```

## Anti-Patterns

### Testing Implementation Details

**Violation:**
```python
def test_caches_results_internally():
    sut = DataProcessor()

    sut.process(data)

    assert len(sut._cache) == 1  # Testing internal state!
```

**Solution:**
```python
def test_returns_same_result_for_duplicate_requests():
    sut = DataProcessor()

    result1 = sut.process(data)
    result2 = sut.process(data)

    assert result1 == result2
```

### Multiple Assertions Without Context

**Violation:**
```python
def test_order_calculation():
    sut = OrderCalculator()

    assert sut.calculate(order1) == 100
    assert sut.calculate(order2) == 200
    assert sut.calculate(order3) == 300
```

**Solution:**
```python
def test_calculates_order_with_single_item():
    sut = OrderCalculator()
    order = Order(items=[Item(price=100)])

    result = sut.calculate(order)

    assert result == 100, f"Expected 100, got {result}"
```

### Using Real Dependencies

**Violation:**
```python
def test_fetches_user_data():
    api = RealApiClient()  # Network call!
    sut = UserService(api)

    user = sut.get_user(1)

    assert user.name == "John"
```

**Solution:**
```python
def test_fetches_user_data():
    api = FakeApiClient()
    api.set_response(User(id=1, name="John"))
    sut = UserService(api)

    user = sut.get_user(1)

    assert user.name == "John"
```

## Best Practices Checklist

- [ ] Tests run in milliseconds
- [ ] No external dependencies (database, network, filesystem)
- [ ] Each test is independent (can run in any order)
- [ ] Test names clearly describe behavior
- [ ] One behavior per test
- [ ] Use AAA pattern with blank line separators
- [ ] Test edge cases and error conditions
- [ ] Use test doubles for dependencies
- [ ] Assertions include helpful messages
- [ ] No conditional logic (if/for/while) in tests
- [ ] Tests written before or with production code