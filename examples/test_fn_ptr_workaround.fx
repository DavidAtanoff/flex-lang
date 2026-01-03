fn main:
    // Define add as a nested function
    fn add a: int, b: int -> int:
        return a + b
    
    unsafe:
        fptr = &add
        result = fptr(10, 20)
        println("Result: ", result)

main()
