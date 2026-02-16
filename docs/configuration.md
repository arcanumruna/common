# Configuration

**Header:** `include/configuration/Configuration.h`

Utilities for reading typed values from environment variables.

## Functions

### `getEnv`

```cpp
inline std::string getEnv(const char* key, const char* def = nullptr);
```

Returns the value of the environment variable `key` as a `std::string`.

- If the variable is set, its value is returned.
- If not set and `def` is provided, `def` is returned.
- If not set and `def` is `nullptr`, throws `std::runtime_error`.

**Parameters:**
- `key` — Environment variable name.
- `def` — Optional default value (C-string). Pass `nullptr` (default) to make the variable required.

**Throws:** `std::runtime_error` — when `key` is missing and no default is provided.

**Example:**
```cpp
std::string host = getEnv("SERVER_HOST", "localhost");
std::string secret = getEnv("API_SECRET"); // throws if not set
```

---

### `getEnvNum<T>`

```cpp
template <typename T>
inline T getEnvNum(const char* key, T def);
```

Returns the value of the environment variable `key` parsed as numeric type `T`.

- If the variable is not set, `def` is returned.
- For floating-point types (`float`, `double`, `long double`): parsed with `std::strtod`; full-string match is required.
- For integral types: parsed with `std::from_chars`; partial matches are not validated.

**Parameters:**
- `key` — Environment variable name.
- `def` — Default value returned when the variable is absent.

**Throws:** `std::runtime_error` — when the variable is set but not a valid number.

**Example:**
```cpp
int port    = getEnvNum<int>("SERVER_PORT", 8080);
float ratio = getEnvNum<float>("LOAD_RATIO", 0.5f);
```

**Note:** For integer types, trailing non-numeric characters (e.g. `"42abc"`) are silently ignored because `from_chars` only checks `errc`. Use `getEnv` + manual parsing if strict validation is needed.
