// Debug test 2 for function pointers

fn add a: int, b: int -> int:
    println("add called with: ", a, ", ", b)
    return a + b

fn test_ptr:
    unsafe:
        println("Step 1: Getting address")
        fptr = &add
        println("Step 2: Got address")
        
        // Store in a regular int to verify
        addr: int = fptr
        println("Address as int: ", addr)
        
        println("Step 3: About to call")
        // Try calling directly
        result = fptr(10, 20)
        println("Step 4: Result = ", result)

test_ptr()
