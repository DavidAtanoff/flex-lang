// Test inclusive range (.. operator) vs exclusive range (range() function)
println("Inclusive range 1..5 (should be 1,2,3,4,5):")
for i in 1..5:
    println("  i = {i}")

println("Exclusive range(1, 5) (should be 1,2,3,4):")
for j in range(1, 5):
    println("  j = {j}")

println("Exclusive range(5) (should be 0,1,2,3,4):")
for k in range(5):
    println("  k = {k}")

println("Done")
