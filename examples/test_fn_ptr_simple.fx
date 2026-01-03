// Simplest function pointer test

fn add a: int, b: int -> int:
    return a + b

fn test_ptr:
    unsafe:
        fptr = &add
        result = fptr(2, 3)
        println("Result: ", result)

test_ptr()
