// Test function pointer address

fn add a: int, b: int -> int:
    return a + b

fn test_addr:
    unsafe:
        fptr = &add
        // Print the address as an integer
        addr: int = fptr
        println("Function address: ", addr)

test_addr()
