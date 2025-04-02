
assert(equals(1, 1));
assert(equals(1, 2) == false);
assert(equals(1, true) == false);
assert(equals(null, null));
assert(equals(null, false) == false);
assert(equals(true, true));
assert(equals([], []));
assert(equals([1, 2], [1, 2]));
assert(equals([1, 2], [1, 3]) == false);
assert(equals([1, 2], [2, 1]) == false);
assert(equals([1, 2], [1, 2, 3]) == false);
assert(equals([1, 2, 3], [1, 2]) == false);

assert(equals(sort([]), []));
assert(equals(sort([1]), [1]));
assert(equals(sort([5, 2, 9, 1, 5, 6]), [1, 2, 5, 5, 6, 9]));
assert(equals(sort([1, 2, 3, 4, 5, 6]), [1, 2, 3, 4, 5, 6]));

assert(equals(reverse([]), []));
assert(equals(reverse([1]), [1]));
assert(equals(reverse([1, 2]), [2, 1]));
assert(equals(reverse([1, 2, 3]), [3, 2, 1]));
