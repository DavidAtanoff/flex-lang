// Debug test for function pointers

fn add a: int, b: int -> int:
    return a + b

fn main:
    unsafe:
        // Test: Get function address
        println("Getting function address...")
        fptr = &add
        println("Got address, calling...")
        result = fptr(3, 4)
        println("Result: ", result)

main()
