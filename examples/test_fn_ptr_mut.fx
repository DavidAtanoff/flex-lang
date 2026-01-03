// Test function pointer with mutable variable

fn add a: int, b: int -> int:
    return a + b

fn test_ptr:
    unsafe:
        mut fptr = &add
        result = fptr(5, 3)
        println("Result: ", result)

test_ptr()
