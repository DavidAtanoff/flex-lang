// Test direct function call without println in function

fn add a: int, b: int -> int:
    return a + b

fn test_direct:
    result = add(10, 20)
    println("Direct call result: ", result)

test_direct()
