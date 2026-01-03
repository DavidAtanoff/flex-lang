// Minimal function pointer test

fn add a: int, b: int -> int:
    return a + b

fn test_ptr:
    unsafe:
        fptr = &add
        result = fptr(10, 20)
        println("Result: ", result)

test_ptr()
