// Test direct function call

fn add a: int, b: int -> int:
    println("add called with: ", a, ", ", b)
    return a + b

fn test_direct:
    result = add(10, 20)
    println("Direct call result: ", result)

test_direct()
