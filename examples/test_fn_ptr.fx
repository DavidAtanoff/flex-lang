// Test function pointer types and calls

// Simple functions to use as function pointers
fn add a: int, b: int -> int:
    return a + b

fn multiply a: int, b: int -> int:
    return a * b

// Main test
fn main:
    unsafe:
        // Test 1: Get function address and call through pointer
        fptr = &add
        result = fptr(10, 5)
        println("add(10, 5) via pointer: ", result)
        
        // Test 2: Assign different function to same pointer
        fptr = &multiply
        result = fptr(10, 5)
        println("multiply(10, 5) via pointer: ", result)
        
        println("All function pointer tests passed!")

main()
