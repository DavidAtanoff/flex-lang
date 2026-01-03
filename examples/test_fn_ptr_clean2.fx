// Clean function pointer test
fn add a: int, b: int -> int:
    return a + b

fn main:
    unsafe:
        fptr = &add
        result = fptr(10, 20)
        println("Result: ", result)

main()
