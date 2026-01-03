// Test function pointer storage

fn add a: int, b: int -> int:
    return a + b

fn test_ptr:
    unsafe:
        // Get address
        fptr = &add
        
        // Store in int and print
        addr: int = fptr
        println("Address: ", addr)
        
        // Load back and call
        fptr2 = addr
        result = fptr2(5, 3)
        println("Result: ", result)

test_ptr()
