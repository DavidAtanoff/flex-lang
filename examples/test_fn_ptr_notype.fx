// Test function pointer without explicit type

fn add a: int, b: int -> int:
    return a + b

fn test_ptr:
    // Try with explicit type annotation
    fptr: *fn(int, int) -> int = &add
    result = fptr(10, 20)
    println("Result: ", result)

test_ptr()
