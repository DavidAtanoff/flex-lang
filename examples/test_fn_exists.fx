// Test if function exists

fn add a: int, b: int -> int:
    return a + b

fn main:
    // Direct call should work
    result = add(10, 20)
    println("Direct: ", result)

main()
